#include "UnityPrefix.h"
#include "ComputeShader.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Profiler/Profiler.h"

PROFILER_INFORMATION(gDispatchComputeProfile, "Compute.Dispatch", kProfilerRender)


// --------------------------------------------------------------------------


struct ComputeProgramStruct
{
	ComputeProgramHandle handle;
	int cbBindPoints[kMaxSupportedConstantBuffers];
	int texBindPoints[kMaxSupportedComputeResources];
	TextureID textures[kMaxSupportedComputeResources];
	unsigned builtinSamplers[kMaxSupportedComputeResources]; // highest 16 bits - builtin sampler type; lowest 16 bits - bind point
	int inBufBindPoints[kMaxSupportedComputeResources];
	ComputeBufferID inBuffers[kMaxSupportedComputeResources];
	UInt32 outBufBindPoints[kMaxSupportedComputeResources]; // highest bit indicates whether this is raw UAV or a texture UAV
	ComputeBufferID outBuffers[kMaxSupportedComputeResources];
	TextureID outTextures[kMaxSupportedComputeResources];
};


ComputeShader::ComputeShader(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
, m_Programs(NULL)
, m_ProgramCount(0)
, m_DataBuffer(NULL)
, m_DataBufferSize(0)
, m_CBDirty(0)
{
}

ComputeShader::~ComputeShader ()
{
	DestroyRuntimeData ();
}


void ComputeShader::DestroyRuntimeData ()
{
	GfxDevice& device = GetGfxDevice();
	for (int i = 0; i < m_ProgramCount; ++i)
	{
		device.DestroyComputeProgram (m_Programs[i].handle);
	}
	delete[] m_Programs;
	m_Programs = NULL;
	m_ProgramCount = 0;

	device.DestroyComputeConstantBuffers (m_ConstantBuffers.size(), m_CBs);

	delete[] m_DataBuffer;
	m_DataBuffer = NULL;
	m_DataBufferSize = 0;
	m_CBDirty = 0;
}

void ComputeShader::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	DestroyRuntimeData ();
	CreateRuntimeData();
}

void ComputeShader::CreateRuntimeData()
{
	// create compute programs
	GfxDevice& device = GetGfxDevice();
	m_ProgramCount = m_Kernels.size();
	m_Programs = new ComputeProgramStruct[m_ProgramCount];
	for (int i = 0; i < m_ProgramCount; ++i)
	{
		const ComputeShaderKernel& kernelData = m_Kernels[i];
		m_Programs[i].handle = device.CreateComputeProgram (kernelData.code.data(), kernelData.code.size());

		// compute CS bind points for this kernel
		memset (m_Programs[i].cbBindPoints, -1, sizeof(m_Programs[i].cbBindPoints));
		for (size_t r = 0, nr = kernelData.cbs.size(); r < nr; ++r)
		{
			const ComputeShaderResource& res = kernelData.cbs[r];
			for (size_t cb = 0, ncb = m_ConstantBuffers.size(); cb < ncb; ++cb)
			{
				if (m_ConstantBuffers[cb].name == res.name)
				{
					m_Programs[i].cbBindPoints[cb] = res.bindPoint;
					break;
				}
			}
		}

		for (size_t t = 0, nt = kernelData.textures.size(); t < nt; ++t)
		{
			m_Programs[i].texBindPoints[t] = kernelData.textures[t].bindPoint;
			m_Programs[i].textures[t].m_ID = 0;
		}
		for (size_t s = 0, ns = kernelData.builtinSamplers.size(); s < ns; ++s)
		{
			m_Programs[i].builtinSamplers[s] = (kernelData.builtinSamplers[s].sampler << 16) | (kernelData.builtinSamplers[s].bindPoint);
		}
		for (size_t b = 0, nb = kernelData.inBuffers.size(); b < nb; ++b)
		{
			m_Programs[i].inBufBindPoints[b] = kernelData.inBuffers[b].bindPoint;
			m_Programs[i].inBuffers[b] = ComputeBufferID();
		}
		for (size_t b = 0, nb = kernelData.outBuffers.size(); b < nb; ++b)
		{
			m_Programs[i].outBufBindPoints[b] = kernelData.outBuffers[b].bindPoint;
			m_Programs[i].outBuffers[b] = ComputeBufferID();
			m_Programs[i].outTextures[b].m_ID = 0;
		}
	}

	// calculate space to hold all CBs, and offsets into the buffer
	m_CBDirty = 0;
	m_DataBufferSize = 0;
	for (int cb = 0; cb < m_ConstantBuffers.size(); ++cb)
	{
		m_CBSizes[cb] = m_ConstantBuffers[cb].byteSize;
		m_CBOffsets[cb] = m_DataBufferSize;
		m_DataBufferSize += m_ConstantBuffers[cb].byteSize;
	}
	m_DataBuffer = new UInt8[m_DataBufferSize];
	memset (m_DataBuffer, 0, m_DataBufferSize);

	// create constant buffers
	device.CreateComputeConstantBuffers (m_ConstantBuffers.size(), m_CBSizes, m_CBs);
}


int ComputeShader::FindKernel (const FastPropertyName& name) const
{
	for (size_t i = 0, n = m_Kernels.size(); i < n; ++i)
	{
		if (m_Kernels[i].name == name)
			return i;
	}
	return -1;
}


void ComputeShader::SetValueParam (const FastPropertyName& name, int byteCount, const void* data)
{
	for (size_t icb = 0, ncb = m_ConstantBuffers.size(); icb < ncb; ++icb)
	{
		const ComputeShaderCB& cb = m_ConstantBuffers[icb];
		for (size_t p = 0, np = cb.params.size(); p < np; ++p)
		{
			if (cb.params[p].name == name)
			{
				const int cbOffset = cb.params[p].offset;
				const int cbByteSize = cb.byteSize;
				if (cbOffset >= cbByteSize || cbOffset + byteCount > cbByteSize)
					return;

				m_CBDirty |= (1<<icb);
				memcpy (m_DataBuffer + m_CBOffsets[icb] + cbOffset, data, byteCount);
			}
		}
	}
}


void ComputeShader::SetTextureParam (unsigned kernelIdx, const FastPropertyName& name, TextureID tid)
{
	if (kernelIdx >= m_ProgramCount)
		return;

	const std::vector<ComputeShaderResource>& textures = m_Kernels[kernelIdx].textures;
	for (size_t i = 0, n = textures.size(); i < n; ++i)
	{
		if (textures[i].name == name)
		{
			m_Programs[kernelIdx].textures[i] = tid;
			break;
		}
	}

	const std::vector<ComputeShaderResource>& outBuffers = m_Kernels[kernelIdx].outBuffers;
	for (size_t i = 0, n = outBuffers.size(); i < n; ++i)
	{
		if (outBuffers[i].name == name)
		{
			m_Programs[kernelIdx].outTextures[i] = tid;
			m_Programs[kernelIdx].outBufBindPoints[i] |= 0x80000000; // set highest bit, indicating it's a texture param
			break;
		}
	}
}

void ComputeShader::SetBufferParam (unsigned kernelIdx, const FastPropertyName& name, ComputeBufferID handle)
{
	if (kernelIdx >= m_ProgramCount)
		return;

	const std::vector<ComputeShaderResource>& inBuffers = m_Kernels[kernelIdx].inBuffers;
	for (size_t i = 0, n = inBuffers.size(); i < n; ++i)
	{
		if (inBuffers[i].name == name)
		{
			m_Programs[kernelIdx].inBuffers[i] = handle;
			break;
		}
	}

	const std::vector<ComputeShaderResource>& outBuffers = m_Kernels[kernelIdx].outBuffers;
	for (size_t i = 0, n = outBuffers.size(); i < n; ++i)
	{
		if (outBuffers[i].name == name)
		{
			m_Programs[kernelIdx].outBuffers[i] = handle;
			m_Programs[kernelIdx].outBufBindPoints[i] &= 0x7FFFFFFF; // clear highest bit, indicating it's a buffer param
			break;
		}
	}
}


void ComputeShader::DispatchComputeShader (unsigned kernelIdx, int threadsX, int threadsY, int threadsZ)
{
	if (!gGraphicsCaps.hasComputeShader)
	{
		ErrorString ("Platform does not support compute shaders");
		return;
	}

	if (kernelIdx >= m_ProgramCount)
		return;

	GPU_AUTO_SECTION(kGPUSectionOther);
	PROFILER_AUTO(gDispatchComputeProfile, this)

	GfxDevice& device = GetGfxDevice();
	const unsigned cbCount = m_ConstantBuffers.size();
	device.UpdateComputeConstantBuffers (cbCount, m_CBs, m_CBDirty, m_DataBufferSize, m_DataBuffer, m_CBSizes, m_CBOffsets, m_Programs[kernelIdx].cbBindPoints);
	device.UpdateComputeResources (
		m_Kernels[kernelIdx].textures.size(), m_Programs[kernelIdx].textures, m_Programs[kernelIdx].texBindPoints,
		m_Kernels[kernelIdx].builtinSamplers.size(), m_Programs[kernelIdx].builtinSamplers,
		m_Kernels[kernelIdx].inBuffers.size(), m_Programs[kernelIdx].inBuffers, m_Programs[kernelIdx].inBufBindPoints,
		m_Kernels[kernelIdx].outBuffers.size(), m_Programs[kernelIdx].outBuffers, m_Programs[kernelIdx].outTextures, m_Programs[kernelIdx].outBufBindPoints
	);
	device.DispatchComputeProgram (m_Programs[kernelIdx].handle, threadsX, threadsY, threadsZ);
	GPU_TIMESTAMP();

	// CBs we have just used aren't dirty anymore
	for (unsigned i = 0; i < cbCount; ++i)
	{
		if (m_Programs[kernelIdx].cbBindPoints[i] >= 0)
		{
			UInt32 dirtyMask = (1<<i);
			m_CBDirty &= ~dirtyMask;
		}
	}
}



template<class TransferFunc>
void ComputeShaderParam::Transfer (TransferFunc& transfer)
{
	TRANSFER(name);
	TRANSFER_ENUM(type)
	TRANSFER(offset);
	TRANSFER(arraySize);
	TRANSFER(rowCount);
	TRANSFER(colCount);
}

template<class TransferFunc>
void ComputeShaderCB::Transfer (TransferFunc& transfer)
{
	TRANSFER(name);
	TRANSFER(byteSize);
	TRANSFER(params);
}

template<class TransferFunc>
void ComputeShaderResource::Transfer (TransferFunc& transfer)
{
	TRANSFER(name);
	TRANSFER(bindPoint);
}

template<class TransferFunc>
void ComputeShaderBuiltinSampler::Transfer (TransferFunc& transfer)
{
	transfer.Transfer((int&)sampler, "sampler");
	TRANSFER(bindPoint);
}

template<class TransferFunc>
void ComputeShaderKernel::Transfer (TransferFunc& transfer)
{
	TRANSFER(name);
	TRANSFER(cbs);
	TRANSFER(textures);
	TRANSFER(builtinSamplers);
	TRANSFER(inBuffers);
	TRANSFER(outBuffers);
	transfer.Transfer(code, "code", kHideInEditorMask);
}


template<class TransferFunc>
void ComputeShader::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);

	transfer.Transfer (m_Kernels, "kernels");
	transfer.Transfer (m_ConstantBuffers, "constantBuffers");

	#if UNITY_EDITOR
	if (!transfer.IsSerializingForGameRelease())
		transfer.Transfer (m_Errors.GetErrors(), "errors");
	#endif
}


IMPLEMENT_CLASS (ComputeShader)
IMPLEMENT_OBJECT_SERIALIZE (ComputeShader)


// --------------------------------------------------------------------------


typedef List< ListNode<ComputeBuffer> > ComputeBufferList;
static ComputeBufferList s_ComputeBuffers;


ComputeBuffer::ComputeBuffer (size_t count, size_t stride, UInt32 flags)
:	m_Count(count)
,	m_Stride(stride)
,	m_Flags(flags)
,	m_ComputeBuffersNode(this)
{
	s_ComputeBuffers.push_back(m_ComputeBuffersNode);
	ReloadToGfxDevice();
}

ComputeBuffer::~ComputeBuffer ()
{
	UnloadFromGfxDevice();
	m_ComputeBuffersNode.RemoveFromList();
}

void ComputeBuffer::UnloadFromGfxDevice()
{
	GfxDevice& device = GetGfxDevice();
	device.DestroyComputeBuffer (m_BufferHandle);
	device.FreeComputeBufferID (m_BufferHandle);
	m_BufferHandle.m_ID = 0;
}

void ComputeBuffer::ReloadToGfxDevice()
{
	GfxDevice& device = GetGfxDevice();
	m_BufferHandle = device.CreateComputeBufferID();
	device.CreateComputeBuffer (m_BufferHandle, m_Count, m_Stride, m_Flags);
}


static size_t ValidateSizeAgainstBufferAndStride (size_t size, size_t bufferSize, size_t stride)
{
	if (0 == stride)
		return 0;

	size = std::min (size, bufferSize);
	size /= stride;
	size *= stride;
	return size;
}

void ComputeBuffer::SetData (const void* data, size_t size)
{
	if (!data || !size || !m_BufferHandle.IsValid())
		return;

	// make sure size is not too large and multiple of stride
	size = ValidateSizeAgainstBufferAndStride (size, m_Count * m_Stride, m_Stride);
	GetGfxDevice().SetComputeBufferData (m_BufferHandle, data, size);
}


void ComputeBuffer::GetData (void* dest, size_t destSize)
{
	if (!dest || !destSize || !m_BufferHandle.IsValid())
		return;

	// make sure size is not too large and multiple of stride
	destSize = ValidateSizeAgainstBufferAndStride (destSize, m_Count * m_Stride, m_Stride);
	GetGfxDevice().GetComputeBufferData (m_BufferHandle, dest, destSize);
}



void ComputeBuffer::CopyCount (ComputeBuffer* src, ComputeBuffer* dst, UInt32 dstOffset)
{
	if (!src || !src->m_BufferHandle.IsValid())
		return;
	if (!dst || !dst->m_BufferHandle.IsValid())
		return;
	UInt32 srcTypeFlags = src->m_Flags & kCBFlagTypeMask;
	if (!(srcTypeFlags & kCBFlagAppend) && !(srcTypeFlags & kCBFlagCounter))
		return;

	GetGfxDevice().CopyComputeBufferCount (src->m_BufferHandle, dst->m_BufferHandle, dstOffset);
}


void ComputeBuffer::UnloadAllFromGfxDevice ()
{
	for (ComputeBufferList::iterator i = s_ComputeBuffers.begin(); i != s_ComputeBuffers.end(); ++i)
		(**i).UnloadFromGfxDevice();
}

void ComputeBuffer::ReloadAllToGfxDevice ()
{
	for (ComputeBufferList::iterator i = s_ComputeBuffers.begin(); i != s_ComputeBuffers.end(); ++i)
		(**i).ReloadToGfxDevice();
}
