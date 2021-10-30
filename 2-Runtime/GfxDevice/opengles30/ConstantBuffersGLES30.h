#pragma once

#include "IncludesGLES30.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"


#define DEBUG_GLES30_CONSTANT_BUFFER_STATS 0

#if DEBUG_GLES30_CONSTANT_BUFFER_STATS
#include <map>
#endif


class ConstantBuffersGLES30
{
public:
	ConstantBuffersGLES30();
	~ConstantBuffersGLES30() { Clear(); }

	void Clear();

	struct ConstBuffer {
		int			bindIndex;
		bool		dirty;
		UInt8*		data;
		UInt32		buffer;
		#if DEBUG_GLES30_CONSTANT_BUFFER_STATS
		int		statsDirty;
		int		stats
		int*	tryCounts;
		int*	changeCounts;
		#endif
	};

	void SetCBInfo (int id, int size);
	int  FindAndBindCB (int id, int bind, int size);
	void ResetBinds ();

	void SetBuiltinCBConstant (int id, int offset, const void* data, int size);
	void SetCBConstant (int index, int offset, const void* data, int size);

	void UpdateBuffers();
	void NewFrame();

private:
	inline int GetCBIndexByID (int id) const
	{
		UInt32 key = id;
		int n = m_BufferKeys.size();
		for (int i = 0; i < n; ++i)
		{
			if ((m_BufferKeys[i]&0xFFFF) == key)
				return i;
		}
		Assert (false);
		return -1;
	}

private:
	typedef std::vector<UInt32> ConstBufferKeys;
	typedef std::vector<ConstBuffer> ConstBuffers;
	ConstBufferKeys	m_BufferKeys;
	ConstBuffers		m_Buffers;

	UInt32 m_ActiveBuffers[16];
};


#if !DEBUG_GLES30_CONSTANT_BUFFER_STATS
inline void ConstantBuffersGLES30::NewFrame() { }
#endif

