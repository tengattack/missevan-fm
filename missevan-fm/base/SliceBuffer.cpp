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
	if (end < start)
		return 0;
	UINT length = end - start;
	MoveMemory(m_pBase, m_pBase + start, length);
	m_pPtr = m_pBase + length;
	return length;
}

UINT CSliceBuffer::Slice(UINT start)
{
	return Slice(start, m_pPtr - m_pBase);
}