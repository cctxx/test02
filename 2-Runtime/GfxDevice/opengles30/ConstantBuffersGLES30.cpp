#include "UnityPrefix.h"
#include "ConstantBuffersGLES30.h"
#include "AssertGLES30.h"

// NEVER enable this for release! Turns off all CB caching and makes things
// very slow, for debugging.
#define DEBUG_DISABLE_CONSTANT_BUFFER_CACHES (0 && !UNITY_RELEASE)

#define CONSTANT_BUFFER_ID_MASK 0xB0000000
#define MAKE_CONSTANT_BUFFER_ID(id) (id|CONSTANT_BUFFER_ID_MASK)


#if DEBUG_GLES30_UNIFORM_BUFFER_STATS
#include "External/shaderlab/Library/FastPropertyName.h"
extern std::string g_LastParsedShaderName;
#endif

ConstantBuffersGLES30::ConstantBuffersGLES30()
{
	memset (m_ActiveBuffers, 0, sizeof(m_ActiveBuffers));
}

void ConstantBuffersGLES30::Clear()
{
	memset (m_ActiveBuffers, 0, sizeof(m_ActiveBuffers));
	for (size_t i = 0; i < m_Buffers.size(); ++i)
	{
		ConstBuffer& cb = m_Buffers[i];
		delete[] cb.data;
		if (cb.buffer)
		{
			REGISTER_EXTERNAL_GFX_DEALLOCATION(MAKE_CONSTANT_BUFFER_ID(cb.buffer));
			GLES_CHK(glDeleteBuffers(1, (GLuint*)&cb.buffer));
		}
		#if DEBUG_GLES30_CONSTANT_BUFFER_STATS
		delete[] cb.changeCounts;
		delete[] cb.tryCounts;
		#endif
	}
	m_Buffers.clear();
	m_BufferKeys.clear();
}

void ConstantBuffersGLES30::SetCBInfo(int id, int size)
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
	cb.bindIndex = -1;
	#if DEBUG_GLES30_CONSTANT_BUFFER_STATS
	cb.statsDirty = 0;
	cb.stats = 0;
	ShaderLab::FastPropertyName name;
	name.index = id;
	printf_console ("GLES30 Uniform Buffer Info: new %s size=%i shader=%s\n", name.GetName(), size, g_LastParsedShaderName.c_str());
	cb.changeCounts = new int[size/4];
	memset (cb.changeCounts, 0, size);
	cb.tryCounts = new int[size/4];
	memset (cb.tryCounts, 0, size);
	#endif

	GLES_CHK(glGenBuffers(1, (GLuint*)&cb.buffer));
	GLES_CHK(glBindBuffer(GL_UNIFORM_BUFFER, cb.buffer));
	GLES_CHK(glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_DYNAMIC_DRAW));
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(MAKE_CONSTANT_BUFFER_ID(cb.buffer),size,this);

	m_Buffers.push_back (cb);
	m_BufferKeys.push_back (key);
}

int ConstantBuffersGLES30::FindAndBindCB (int id, int bind, int size)
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
				cb.bindIndex = bind;
			}
			return idx;
		}
	}
	Assert (false);
	return -1;
}

void ConstantBuffersGLES30::ResetBinds()
{
	for (ConstBuffers::iterator it = m_Buffers.begin(), itEnd = m_Buffers.end(); it != itEnd; ++it)
	{
		it->bindIndex = -1;
	}
}

void ConstantBuffersGLES30::SetCBConstant (int idx, int offset, const void* data, int size)
{
	Assert (idx >= 0 && idx < m_Buffers.size());
	ConstBuffer& cb = m_Buffers[idx];
	Assert (offset >= 0 && offset+size <= (m_BufferKeys[idx]>>16) && size > 0);

	#if DEBUG_GLES30_CONSTANT_BUFFER_STATS
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

			#if DEBUG_GLES30_CONSTANT_BUFFER_STATS
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

			#if DEBUG_GLES30_CONSTANT_BUFFER_STATS
			for (int i = offset/4; i < offset/4+size/4; ++i)
				++cb.changeCounts[i];
			#endif
		}
	}
}

void ConstantBuffersGLES30::UpdateBuffers ()
{
	size_t n = m_Buffers.size();

	#if !UNITY_RELEASE
	// check if we have duplicate buffers bound to the same slot (should never happen!)
	UInt32 bound = 0;
	for (size_t i = 0; i < n; ++i)
	{
		ConstBuffer& cb = m_Buffers[i];
		int bind = cb.bindIndex;
		if (bind >= 0 && bind < 32)
		{
			Assert (!(bound & (1<<bind)));
			bound |= (1<<bind);
		}
	}
	#endif


	for (size_t i = 0; i < n; ++i)
	{
		ConstBuffer& cb = m_Buffers[i];
		if (DEBUG_DISABLE_CONSTANT_BUFFER_CACHES || cb.dirty)
		{
			GLES_CHK(glBindBuffer(GL_UNIFORM_BUFFER, cb.buffer));

			UInt32 bufferSize = (m_BufferKeys[i]>>16);
			GLES_CHK(glBufferData(GL_UNIFORM_BUFFER, bufferSize, cb.data, GL_DYNAMIC_DRAW));
			//void* data = glMapBufferRange(GL_UNIFORM_BUFFER, 0, bufferSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
			//GLESAssert();
			//if (data != NULL)
			//{
			//	::memcpy(data, cb.data, bufferSize);
			//}
			//GLES_CHK(glUnmapBuffer(GL_UNIFORM_BUFFER));
			#if DEBUG_GLES30_CONSTANT_BUFFER_STATS
			++cb.statsDirty;
			#endif
		}

		// Bind
		int bindIndex = cb.bindIndex;
		if (bindIndex >= 0 && (DEBUG_DISABLE_CONSTANT_BUFFER_CACHES || m_ActiveBuffers[bindIndex] != cb.buffer))
		{
			GLES_CHK(glBindBufferBase(GL_UNIFORM_BUFFER, bindIndex, cb.buffer));
		}
		cb.dirty = false;
	}
}

#if 0

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

#endif