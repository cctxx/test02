#pragma once

#include "D3D11Includes.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"


#define DEBUG_D3D11_CONSTANT_BUFFER_STATS 0

#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
#include <map>
#endif


class ConstantBuffersD3D11
{
public:
	ConstantBuffersD3D11();
	~ConstantBuffersD3D11() { Clear(); }

	void Clear();
	void InvalidateState();

	struct ConstBuffer {
		int			bindIndex[kShaderTypeCount];
		unsigned	bindStages;
		bool	dirty;
		UInt8*			data;
		ID3D11Buffer*	buffer;
		#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
		int		statsDirty;
		int		stats[kShaderTypeCount]
		int*	tryCounts;
		int*	changeCounts;
		#endif
	};

	void SetCBInfo (int id, int size);
	int  FindAndBindCB (int id, ShaderType shaderType, int bind, int size);
	void ResetBinds (ShaderType shaderType);

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
	ConstBuffers	m_Buffers;

	ID3D11Buffer*	m_ActiveBuffers[kShaderTypeCount][16];
};


#if !DEBUG_D3D11_CONSTANT_BUFFER_STATS
inline void ConstantBuffersD3D11::NewFrame() { }
#endif

