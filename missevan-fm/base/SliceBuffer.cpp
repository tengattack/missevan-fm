#include "StdAfx.h"
#include "SliceBuffer.h"


CSliceBuffer::CSliceBuffer()
	: CBuffer()
{
}


CSliceBuffer::~CSliceBuffer()
{
}

UINT CSliceBuffer::Slice(UINT start, UINT end)
{
	if (end < start) {
		// TODO: assert here
		ClearBuffer();
		return 0;
	}
	UINT length = end - start;
	if (length >= 0) {
		if (start > 0) {
			MoveMemory(m_pBase, m_pBase + start, length);
		}
		m_pPtr = m_pBase + length;
	} else {
		ClearBuffer();
	}
	return length;
}

UINT CSliceBuffer::Slice(UINT start)
{
	return Slice(start, m_pPtr - m_pBase);
}