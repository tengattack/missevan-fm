#include "StdAfx.h"
#include "LoopbackAudioCapture.h"

#include <base/logging.h>
#include <boost/format.hpp>
#include <base/windows_version.h>
#include <MMDeviceAPI.h>
#include <AudioClient.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

CLoopbackAudioCapture::CLoopbackAudioCapture(uint32 bufferLength)
	: CAudioCapture(bufferLength)
	, _ChatEndpoint(NULL)
	, _AudioClient(NULL)
	, _CaptureClient(NULL)
	, _ChatThread(NULL)
	, _ShutdownEvent(NULL)
	, _AudioSamplesReadyEvent(NULL)
	, _EnableTransform(false)
	, _EventCallback(false)
{
	memset(&_waveFormat, 0, sizeof(_waveFormat));
}

CLoopbackAudioCapture::~CLoopbackAudioCapture()
{
	Shutdown();
}

//
//  This sample only works on Windows 7
//
ulong IsWin7OrLater(ulong *buildNumber)
{
	bool bWin7OrLater = true;

	OSVERSIONINFO ver = {};
	ver.dwOSVersionInfoSize = sizeof(ver);

	if (GetVersionEx(&ver)) {
		bWin7OrLater = (ver.dwMajorVersion > 6) ||
			((ver.dwMajorVersion == 6) && (ver.dwMinorVersion >= 1));
		*buildNumber = ver.dwBuildNumber;
	}

	if (bWin7OrLater) {
		RTL_OSVERSIONINFOW rovi = {};
		rovi.dwOSVersionInfoSize = sizeof(rovi);
		if (base::win::GetRealOSVersion(&rovi)) {
			*buildNumber = rovi.dwBuildNumber;
			return rovi.dwMajorVersion;
		}
	}

	return bWin7OrLater ? 7 : 0;
}

bool CLoopbackAudioCapture::Initialize(AudioFormat *format)
{
	CAudioCapture::Initialize(format);

	ulong buildNum = 0;
	ulong osVersion = IsWin7OrLater(&buildNum);
	if (osVersion < 6) {
		return false;
	} else if (osVersion >= 10) {
		// loopback event callback only works in part of Windows 10
		if (buildNum >= 15000) {
			_EventCallback = true;
		}
	}

	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_DeviceEnumerator));
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to instantiate device enumerator: " << boost::format("0x%08x") % hr;
		return false;
	}

	hr = _DeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &_ChatEndpoint);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to retrieve default endpoint: " << boost::format("0x%08x") % hr;
		return false;
	}
	//
	//  Create our shutdown event - we want an auto reset event that starts in the not-signaled state.
	//
	_ShutdownEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (_ShutdownEvent == NULL)
	{
		PLOG(ERROR) << "Unable to create shutdown event";
		return false;
	}

	_AudioSamplesReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (_ShutdownEvent == NULL)
	{
		PLOG(ERROR) << "Unable to create samples ready event";
		return false;
	}

	return true;
}

void CLoopbackAudioCapture::Shutdown()
{
	Stop();

	if (_ShutdownEvent)
	{
		CloseHandle(_ShutdownEvent);
		_ShutdownEvent = NULL;
	}
	if (_AudioSamplesReadyEvent)
	{
		CloseHandle(_AudioSamplesReadyEvent);
		_AudioSamplesReadyEvent = NULL;
	}

	SafeRelease(&_ChatEndpoint);
	SafeRelease(&_DeviceEnumerator);
}

bool CLoopbackAudioCapture::Start()
{
	HRESULT hr;
	WAVEFORMATEX *mixFormat = NULL;

	if (_EnableTransform) {
		// disbale transformer for previous device
		_Transform.Shutdown();
		_EnableTransform = false;
	}

	hr = _ChatEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&_AudioClient));
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to activate audio client: " << boost::format("0x%08x") % hr;
		return false;
	}
	hr = _AudioClient->GetMixFormat(&mixFormat);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to get mix format on audio client: " << boost::format("0x%08x") % hr;
		return false;
	}

	if (mixFormat->wFormatTag != WAVE_FORMAT_PCM
		|| mixFormat->nSamplesPerSec != _format.sampleRate
		|| mixFormat->nChannels != _format.channels
		|| mixFormat->wBitsPerSample != _format.bits)
	{
		WAVEFORMATEXTENSIBLE *cloestWaveFormat = NULL;
		_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		_waveFormat.nSamplesPerSec = _format.sampleRate;
		_waveFormat.nChannels = _format.channels;
		_waveFormat.wBitsPerSample = _format.bits;
		_waveFormat.nBlockAlign = (_waveFormat.nChannels * _waveFormat.wBitsPerSample) / 8;
		_waveFormat.nAvgBytesPerSec = _waveFormat.nSamplesPerSec * _waveFormat.nBlockAlign;
		_waveFormat.cbSize = 0;

		// try initialize for specify format
		hr = _AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &_waveFormat, (WAVEFORMATEX **)&cloestWaveFormat);

		if (hr == S_OK) {
			// the format is supported
			memcpy(mixFormat, &_waveFormat, sizeof(_waveFormat));
		} else {
			memcpy(&_waveFormat, mixFormat, sizeof(_waveFormat));

			if (!_Transform.Initialize(mixFormat, &_format))
			{
				PLOG(ERROR) << "Unable to initialize audio transformer.";
				return false;
			}

			_Transform.RegisterCallback(TransformProc, this);
			if (!_Transform.Start())
			{
				_Transform.Shutdown();
				PLOG(ERROR) << "Unable to start audio transformer.";
				return false;
			}

			LOG(INFO) << "Enable audio transformer (CLoopbackAudioCapture)";
			_EnableTransform = true;
		}

		if (cloestWaveFormat) {
			CoTaskMemFree(cloestWaveFormat);
		}
	} else {
		memcpy(&_waveFormat, mixFormat, sizeof(_waveFormat));
	}
	
	//
	//  Initialize the chat transport - Initialize WASAPI in event driven mode, associate the audio client with
	//  our samples ready event handle, retrieve a capture/render client for the transport, create the chat thread
	//  and start the audio engine.
	//
	// REFTIMES_PER_SEC
	// AUDCLNT_SESSIONFLAGS_DISPLAY_HIDE | AUDCLNT_STREAMFLAGS_NOPERSIST | 
	// (uint64)_bufferLength * 10000000 / mixFormat->nAvgBytesPerSec
	hr = _AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		(_EventCallback ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0) | AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_LOOPBACK,
		REFTIMES_PER_SEC, // This parameter is of type REFERENCE_TIME and is expressed in 100-nanosecond units.
		0, mixFormat, NULL);

	CoTaskMemFree(mixFormat);
	mixFormat = NULL;

	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to initialize audio client: " << boost::format("0x%08x") % hr;
		return false;
	}
	
	// reset events
	ResetEvent(_ShutdownEvent);
	ResetEvent(_AudioSamplesReadyEvent);

	if (_EventCallback)
	{
		hr = _AudioClient->SetEventHandle(_AudioSamplesReadyEvent);
		if (FAILED(hr))
		{
			PLOG(ERROR) << "Unable to set ready event: " << boost::format("0x%08x") % hr;
			return false;
		}
	}

	hr = _AudioClient->GetService(IID_PPV_ARGS(&_CaptureClient));
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to get Capture/Render client: " << boost::format("0x%08x") % hr;
		return false;
	}

	if (!InitializeStreamSwitch(_AudioClient, eRender, eMultimedia))
	{
		return false;
	}

	if (_ChatThread == NULL)
	{
		//
		//  Now create the thread which is going to drive the "Chat".
		//
		_ChatThread = CreateThread(NULL, 0, _EventCallback ? WasapiEventThread : WasapiThread, this, 0, NULL);
		if (_ChatThread == NULL)
		{
			PLOG(ERROR) << "Unable to create transport thread";
			return false;
		}
	}

	if (_EventCallback) {
		//
		//  We're ready to go, start the chat!
		//
		hr = _AudioClient->Start();
		if (FAILED(hr))
		{
			PLOG(ERROR) << "Unable to start chat client: " << boost::format("0x%08x") % hr;
			return false;
		}
	}

	return true;
}

void CLoopbackAudioCapture::Stop()
{
	if (_AudioClient)
	{
		_AudioClient->Stop();
	}
	if (_ShutdownEvent)
	{
		SetEvent(_ShutdownEvent);
	}
	if (_ChatThread)
	{
		WaitForSingleObject(_ChatThread, INFINITE);

		CloseHandle(_ChatThread);
		_ChatThread = NULL;
	}
	if (_EnableTransform) {
		_Transform.Stop();
		_Transform.Shutdown();
		_EnableTransform = false;
	}

	SafeRelease(&_CaptureClient);
	SafeRelease(&_AudioClient);

	memset(&_waveFormat, 0, sizeof(_waveFormat));
}

bool CLoopbackAudioCapture::HandleStreamSwitchEvent()
{
	if (_AudioClient)
	{
		_AudioClient->Stop();
	}
	SafeRelease(&_ChatEndpoint);
	SafeRelease(&_AudioClient);
	SafeRelease(&_CaptureClient);

	HRESULT hr = _DeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &_ChatEndpoint);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to retrieve default endpoint: " << boost::format("0x%08x") % hr;
		return false;
	}

	return Start();
}

DWORD CLoopbackAudioCapture::WasapiThread(LPVOID Context)
{
	CLoopbackAudioCapture *pCapture = static_cast<CLoopbackAudioCapture *>(Context);

	bool stillPlaying = true;
	WAVEFORMATEX *pwfx = &pCapture->_waveFormat;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	UINT32 bufferFrameCount;
	UINT32 framesAvailable;
	UINT32 packetLength = 0;
	HRESULT hr;
	BYTE *pData;
	DWORD flags;

	hr = pCapture->_AudioClient->GetBufferSize(&bufferFrameCount);
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to get buffer size: " << boost::format("0x%08x") % hr;
		goto exit_l;
	}

	// Calculate the actual duration of the allocated buffer.
	hnsActualDuration = REFTIMES_PER_SEC * bufferFrameCount / pwfx->nSamplesPerSec;

	hr = pCapture->_AudioClient->Start();
	if (FAILED(hr))
	{
		PLOG(ERROR) << "Unable to start chat client: " << boost::format("0x%08x") % hr;
		goto exit_l;
	}

	HANDLE waitArray[2] = { pCapture->_ShutdownEvent, pCapture->_StreamSwitchEvent };

	while (stillPlaying)
	{
		// Sleep for half the buffer duration.
		REFERENCE_TIME waitResult = WaitForMultipleObjects(2, waitArray, FALSE, 
			DWORD(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

		switch (waitResult) {
		case WAIT_OBJECT_0 + 0: /* _ShutdownEvent */
			stillPlaying = false;
			break;
		case WAIT_OBJECT_0 + 1: /* _StreamSwitchEvent */
			if (!pCapture->HandleStreamSwitchEvent())
			{
				// stillPlaying = false;
			}
			else
			{
				pCapture->_InStreamSwitch = false;
			}
			break;
		case WAIT_TIMEOUT: {

			if (pCapture->_CaptureClient == NULL) {
				break;
			}

			HFG(pCapture->_CaptureClient->GetNextPacketSize(&packetLength));

			while (packetLength != 0)
			{
				// Get the available data in the shared buffer.
				hr = pCapture->_CaptureClient->GetBuffer(
					&pData,
					&framesAvailable,
					&flags, NULL, NULL);

				if (FAILED(hr)) {
					PLOG(ERROR) << "Unable to get capture buffer: " << boost::format("0x%08x") % hr;
					goto exit_l;
				} else if (hr != S_OK) {
					// may AUDCLNT_S_BUFFER_EMPTY
					break;
				}

				if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
				{
					if (pCapture->_callback) {
						AudioFormat *format = &pCapture->_format;
						uint32 length = framesAvailable * format->channels * format->bits / 8;
						pCapture->_callback(kCaptureData, NULL, length, pCapture->_user_data);
					}
				}
				else if (pCapture->_EnableTransform)
				{
					pCapture->_Transform.Encode(pData, framesAvailable * pwfx->nChannels * pwfx->wBitsPerSample / 8);
				}
				else if (pCapture->_callback)
				{
					AudioFormat *format = &pCapture->_format;
					pCapture->_callback(kCaptureData, pData, framesAvailable * format->channels * format->bits / 8, pCapture->_user_data);
				}

				HFG(pCapture->_CaptureClient->ReleaseBuffer(framesAvailable));
				HFG(pCapture->_CaptureClient->GetNextPacketSize(&packetLength));
			}

			break;
		}
		}
	}

	return 0;

exit_l:
	CloseHandle(pCapture->_ChatThread);
	pCapture->_ChatThread = NULL;
	return 1;
}

DWORD CLoopbackAudioCapture::WasapiEventThread(LPVOID Context)
{
	bool stillPlaying = true;
	CLoopbackAudioCapture *pCapture = static_cast<CLoopbackAudioCapture *>(Context);
	WAVEFORMATEX *pwfx = &pCapture->_waveFormat;
	HANDLE waitArray[3] = { pCapture->_ShutdownEvent, pCapture->_StreamSwitchEvent, pCapture->_AudioSamplesReadyEvent };

	while (stillPlaying)
	{
		HRESULT hr;
		DWORD waitResult = WaitForMultipleObjects(3, waitArray, FALSE, INFINITE);
		switch (waitResult)
		{
		case WAIT_OBJECT_0 + 0:
			stillPlaying = false;       // We're done, exit the loop.
			break;
		case WAIT_OBJECT_0 + 1: {
			if (!pCapture->HandleStreamSwitchEvent())
			{
				// stillPlaying = false;
			}
			else
			{
				pCapture->_InStreamSwitch = false;
			}
			break;
		}
		case WAIT_OBJECT_0 + 2: {

			if (pCapture->_CaptureClient == NULL) {
				break;
			}

			//
			//  Either stream silence to the audio client or ignore the audio samples.
			//
			//  Note that we don't check for errors here.  This is because 
			//      (a) there's no way of reporting the failure
			//      (b) once the streaming engine has started there's really no way for it to fail.
			//
			BYTE *pData;
			UINT32 framesAvailable;
			DWORD flags = 0;

			// hr = pCapture->_AudioClient->GetCurrentPadding(&framesAvailable);
			hr = pCapture->_CaptureClient->GetBuffer(&pData, &framesAvailable, &flags, NULL, NULL);
			if (FAILED(hr)) {
				PLOG(ERROR) << "IAudioCaptureClient GetBuffer Error: " << boost::format("0x%08x") % hr;
				goto exit_l;
			}

			if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
			{
				if (pCapture->_callback) {
					AudioFormat *format = &pCapture->_format;
					uint32 length = framesAvailable * format->channels * format->bits / 8;
					pCapture->_callback(kCaptureData, NULL, length, pCapture->_user_data);
				}
			}
			else if (pCapture->_EnableTransform)
			{
				pCapture->_Transform.Encode(pData, framesAvailable * pwfx->nChannels * pwfx->wBitsPerSample / 8);
			}
			else if (pCapture->_callback)
			{
				AudioFormat *format = &pCapture->_format;
				pCapture->_callback(kCaptureData, pData, framesAvailable * format->channels * format->bits / 8, pCapture->_user_data);
			}

			hr = pCapture->_CaptureClient->ReleaseBuffer(framesAvailable);
			if (FAILED(hr))
			{
				PLOG(ERROR) << "Unable to release capture buffer: " << boost::format("0x%08x") % hr;
			}
			break;
		}
		}
	}

exit_l:
	pCapture->_ChatThread = NULL;
	return 0;
}

void CLoopbackAudioCapture::TransformProc(uint8 *data, ulong length, ulong samples, void *user_data)
{
	CLoopbackAudioCapture *pCapture = (CLoopbackAudioCapture *)user_data;
	if (pCapture->_callback)
	{
		pCapture->_callback(kCaptureData, data, length, pCapture->_user_data);
	}
}