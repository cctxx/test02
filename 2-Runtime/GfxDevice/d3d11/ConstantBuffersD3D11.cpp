#include "UnityPrefix.h"
#include "ConstantBuffersD3D11.h"
#include "D3D11Context.h"
#include "D3D11Utils.h"


// NEVER enable this for release! Turns off all CB caching and makes things
// very slow, for debugging.
#define DEBUG_DISABLE_CONSTANT_BUFFER_CACHES (0 && !UNITY_RELEASE)


#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
#include "External/shaderlab/Library/FastPropertyName.h"
extern std::string g_LastParsedShaderName;
#endif


ConstantBuffersD3D11::ConstantBuffersD3D11()
{
	InvalidateState();
}

void ConstantBuffersD3D11::InvalidateState()
{
	memset (m_ActiveBuffers, 0, sizeof(m_ActiveBuffers));
}


void ConstantBuffersD3D11::Clear()
{
	memset (m_ActiveBuffers, 0, sizeof(m_ActiveBuffers));
	for (size_t i = 0; i < m_Buffers.size(); ++i)
	{
		ConstBuffer& cb = m_Buffers[i];
		delete[] cb.data;
		if (cb.buffer)
		{
			cb.buffer->Release();
			REGISTER_EXTERNAL_GFX_DEALLOCATION(cb.buffer);
		}
		#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
		delete[] cb.changeCounts;
		delete[] cb.tryCounts;
		#endif
	}
	m_Buffers.clear();
	m_BufferKeys.clear();
}


void ConstantBuffersD3D11::SetCBInfo (int id, int size)
{
	size_t n = m_Buffers.size();
	Assert (m_BufferKeys.size() == n);
	UInt32 key = id | (size<<16);
	for (size_t i = 0; i < n; ++i)
	{
		if (m_BufferKeys[i] == key)
			return;
	}

	// not found, create one
	ConstBuffer cb;
	cb.data = new UInt8[size];
	memset (cb.data, 0, size);
	cb.dirty = true;
	for (int i = 0; i < kShaderTypeCount; ++i)
		cb.bindIndex[i] = -1;
	cb.bindStages = 0;
	#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
	cb.statsDirty = 0;
	for (int i = 0; i < kShaderTypeCount; ++i)
		cb.stats[i] = 0;
	ShaderLab::FastPropertyName name;
	name.index = id;
	printf_console ("DX11 Constant Buffer Info: new %s size=%i shader=%s\n", name.GetName(), size, g_LastParsedShaderName.c_str());
	cb.changeCounts = new int[size/4];
	memset (cb.changeCounts, 0, size);
	cb.tryCounts = new int[size/4];
	memset (cb.tryCounts, 0, size);
	#endif

	ID3D11Device* dev = GetD3D11Device();
	// Default usage and using UpdateSubresource is seemingly preferred path in drivers
	// over dynamic buffer with Map.
	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	HRESULT hr = dev->CreateBuffer (&desc, NULL, &cb.buffer);
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(cb.buffer,size,this);
	Assert (SUCCEEDED(hr));
	SetDebugNameD3D11 (cb.buffer, Format("ConstantBuffer-%d-%d", id, size));

	m_Buffers.push_back (cb);
	m_BufferKeys.push_back (key);
}

void ConstantBuffersD3D11::SetBuiltinCBConstant (int id, int offset, const void* data, int size)
{
	int idx = GetCBIndexByID (id);
	ConstBuffer& cb = m_Buffers[idx];
	Assert (offset >= 0 && offset+size <= (m_BufferKeys[idx]>>16) && size > 0);

	#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
	for (int i = offset/4; i < offset/4+size/4; ++i)
		++cb.tryCounts[i];
	#endif

	if (DEBUG_DISABLE_CONSTANT_BUFFER_CACHES || memcmp(cb.data+offset, data, size) != 0)
	{
		memcpy (cb.data+offset, data, size);
		cb.dirty = true;

		#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
		for (int i = offset/4; i < offset/4+size/4; ++i)
			++cb.changeCounts[i];
		#endif
	}
}

void ConstantBuffersD3D11::SetCBConstant (int idx, int offset, const void* data, int size)
{
	Assert (idx >= 0 && idx < m_Buffers.size());
	ConstBuffer& cb = m_Buffers[idx];
	Assert (offset >= 0 && offset+size <= (m_BufferKeys[idx]>>16) && size > 0);

	#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
	for (int i = offset/4; i < offset/4+size/4; ++i)
		++cb.tryCounts[i];
	#endif

	if (size == 4)
	{
		UInt32* dstData = (UInt32*)(cb.data+offset);
		UInt32 srcData = *(UInt32*)data;
		if (DEBUG_DISABLE_CONSTANT_BUFFER_CACHES || *dstData != srcData)
		{
			*dstData = srcData;
			cb.dirty = true;

			#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
			for (int i = offset/4; i < offset/4+size/4; ++i)
				++cb.changeCounts[i];
			#endif
		}
	}
	else
	{
		if (DEBUG_DISABLE_CONSTANT_BUFFER_CACHES || memcmp(cb.data+offset, data, size) != 0)
		{
			memcpy (cb.data+offset, data, size);
			cb.dirty = true;

			#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
			for (int i = offset/4; i < offset/4+size/4; ++i)
				++cb.changeCounts[i];
			#endif
		}
	}
}

int ConstantBuffersD3D11::FindAndBindCB (int id, ShaderType shaderType, int bind, int size)
{
	UInt32 key = id | (size<<16);
	int idx = 0;
	for (ConstBufferKeys::const_iterator it = m_BufferKeys.begin(), itEnd = m_BufferKeys.end(); it != itEnd; ++it, ++idx)
	{
		if (*it == key)
		{
			ConstBuffer& cb = m_Buffers[idx];
			if (bind >= 0)
			{
				cb.bindIndex[shaderType] = bind;
				cb.bindStages |= (1<<shaderType);
			}
			return idx;
		}
	}
	Assert (false);
	return -1;
}

void ConstantBuffersD3D11::ResetBinds(ShaderType shaderType)
{
	for (ConstBuffers::iterator it = m_Buffers.begin(), itEnd = m_Buffers.end(); it != itEnd; ++it)
	{
		it->bindIndex[shaderType] = -1;
		it->bindStages &= ~(1<<shaderType);
	}
}


void ConstantBuffersD3D11::UpdateBuffers ()
{
	ID3D11DeviceContext* ctx = GetD3D11Context();

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr;
	size_t n = m_Buffers.size();

	#if !UNITY_RELEASE
	// check if we have duplicate buffers bound to the same slot (should never happen!)
	UInt32 bound[kShaderTypeCount] = {0};
	for (size_t i = 0; i < n; ++i)
	{
		ConstBuffer& cb = m_Buffers[i];
		for (int pt = kShaderVertex; pt < kShaderTypeCount; ++pt)
		{
			int bind = cb.bindIndex[pt];
			if (bind >= 0 && bind < 32)
			{
				Assert (!(bound[pt] & (1<<bind)));
				bound[pt] |= (1<<bind);
			}
		}
	}
	#endif


	for (size_t i = 0; i < n; ++i)
	{
		ConstBuffer& cb = m_Buffers[i];
		if (!DEBUG_DISABLE_CONSTANT_BUFFER_CACHES && cb.bindStages == 0)
			continue;
		if (DEBUG_DISABLE_CONSTANT_BUFFER_CACHES || cb.dirty)
		{
			ctx->UpdateSubresource (cb.buffer, 0, NULL, cb.data, (m_BufferKeys[i]>>16), 1);
			#if DEBUG_D3D11_CONSTANT_BUFFER_STATS
			++cb.statsDirty;
			#endif
		}

		int bindIndex;

		// Bind to used stages
		// WP8 seems to be buggy with constant buffers; we need to always rebind them. Hence UNITY_WP8 test below.
	#define BIND_CB(cbShaderType,dxCall) \
		bindIndex = cb.bindIndex[cbShaderType]; \
		if (bindIndex >= 0 && (DEBUG_DISABLE_CONSTANT_BUFFER_CACHES || UNITY_WP8 || m_ActiveBuffers[cbShaderType][bindIndex] != cb.buffer)) { \
			ctx->dxCall (bindIndex, 1, &cb.buffer); \
			m_ActiveBuffers[cbShaderType][bindIndex] = cb.buffer; \
		}

		BIND_CB(kShaderVertex,VSSetConstantBuffers);
		BIND_CB(kShaderFragment,PSSetConstantBuffers);
		BIND_CB(kShaderGeometry,GSSetConstantBuffers);
		BIND_CB(kShaderHull,HSSetConstantBuffers);
		BIND_CB(kShaderDomain,DSSetConstantBuffers);
		cb.dirty = false;
	}
}


#if DEBUG_D3D11_CONSTANT_BUFFER_STATS

static void WriteTGAFile (const char* filename, int width, int height, const UInt8* bgr)
{
	FILE* f = fopen(filename, "wb");
	// header
	putc(0,f);
	putc(0,f);
	putc(2,f); // uncompressed RGB
	putc(0,f); putc(0,f);
	putc(0,f); putc(0,f);
	putc(0,f);
	putc(0,f); putc(0,f);
	putc(0,f); putc(0,f);
	putc((width & 0x00FF),f);
	putc((width & 0xFF00)>>8,f);
	putc((height & 0x00FF),f);
	putc((height & 0xFF00)>>8,f);
	putc(24,f); // 24 bit
	putc(0x20,f); // vertical flip
	// data
	fwrite (bgr, 3, width*height, f);
	fclose (f);
}

static void DensityToBGR (int density, UInt8* bgr)
{
	if (density < 1)
	{
		bgr[0] = bgr[1] = bgr[2] = 0;
		return;
	}
	bgr[0] = clamp(40+density/4, 0, 255);
	bgr[1] = clamp(40+density/4, 0, 255);
	bgr[2] = clamp(40+density/4, 0, 255);
}

static void PutBGRPixelBlock (UInt8* img, int imgWidth, int x, int y, const UInt8* bgr)
{
	for (int i = 0; i < 4; ++i)
	{
		UInt8* ptr = img + ((y+i)*imgWidth+x) * 3;
		for (int j = 0; j < 4; ++j, ptr += 3)
		{
			ptr[0] = bgr[0];
			ptr[1] = bgr[1];
			ptr[2] = bgr[2];
		}
	}
}


void ConstantBuffersD3D11::NewFrame()
{
	if (GetAsyncKeyState(VK_F7))
	{
		printf_console ("DX11 Constant Buffer stats:\n");
		float traffic = 0.0f;
		int uploads = 0;
		int maxSize = 0;
		for (size_t i = 0; i < m_BufferKeys.size(); ++i)
			maxSize = std::max(int(m_BufferKeys[i]>>16), maxSize);

		int imgWidth = maxSize+1;
		int imgHeight = m_Buffers.size()*3*4;
		UInt8* imgData = new UInt8[imgWidth*imgHeight*3];
		memset (imgData, 0, imgWidth*imgHeight*3);

		for (size_t i = 0; i < m_Buffers.size(); ++i)
		{
			ConstBuffer& cb = m_Buffers[i];
			int cbId = (m_BufferKeys[i]&0xFFFF);
			int cbSize = (m_BufferKeys[i]>>16);
			ShaderLab::FastPropertyName name;
			name.index = cbId;
			traffic += (cbSize*cb.statsDirty)/1024.0f;
			uploads += cb.statsDirty;
			printf_console ("   %s  size:%i (%.1fkB in %i upl) vs:%i ps:%i\n", name.GetName(), cbSize, (cbSize*cb.statsDirty)/1024.0f, cb.statsDirty, cb.statsVS, cb.statsPS);
			if (cb.statsDirty > 0)
			{
				for (int j = 0; j < cbSize/4; ++j)
				{
					UInt8 bgr[3];
					DensityToBGR (cb.tryCounts[j], bgr);
					PutBGRPixelBlock (imgData, imgWidth, j*4, i*3*4, bgr);
					DensityToBGR (cb.changeCounts[j], bgr);
					PutBGRPixelBlock (imgData, imgWidth, j*4, i*3*4+4, bgr);
				}
			}
			for (int j = 0; j < 8; ++j)
			{
				imgData[((i*3*4+j)*imgWidth + cbSize)*3 + 1] = 255;
			}
		}
		WriteTGAFile ("cbStats.tga", imgWidth, imgHeight, imgData);
		delete[] imgData;
		printf_console (" =%i uploads, %.1fkB traffic\n\n", uploads, traffic);
	}

	// reset stats
	for (size_t i = 0; i < m_Buffers.size(); ++i)
	{
		ConstBuffer& cb = m_Buffers[i];
		int cbSize = (m_BufferKeys[i]>>16);
		cb.statsDirty = cb.statsVS = cb.statsPS = 0;
		memset (cb.changeCounts, 0, cbSize/4*4);
		memset (cb.tryCounts, 0, cbSize/4*4);
	}
}
#endif
