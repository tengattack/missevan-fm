
#ifndef _SLICE_BUFFER_H_
#define _SLICE_BUFFER_H_
#pragma once

#include <common/Buffer.h>

class CSliceBuffer : public CBuffer
{
public:
	CSliceBuffer();
	~CSliceBuffer();

	UINT Slice(UINT start, UINT end);
	UINT Slice(UINT start);
};

#endif
