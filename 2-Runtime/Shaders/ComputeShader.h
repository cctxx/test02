#pragma once

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "External/shaderlab/Library/FastPropertyName.h"
#include "External/shaderlab/Library/ShaderLabErrors.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Utilities/NonCopyable.h"
#include <vector>

using ShaderLab::FastPropertyName;


struct ComputeProgramStruct;


enum ComputeShaderParamType {
	kCSParamFloat = 0,
	kCSParamInt = 1,
	kCSParamUInt = 2,
	kCSParamForce32BitEnum = 0x7FFFFFFF
};

struct ComputeShaderParam {
	DECLARE_SERIALIZE_NO_PPTR (ComputeShaderParam)

	FastPropertyName name;
	ComputeShaderParamType type; 
	int offset;
	int arraySize;
	int rowCount;
	int colCount;
};

struct ComputeShaderCB {
	DECLARE_SERIALIZE_NO_PPTR (ComputeShaderCB)

	FastPropertyName name;
	int byteSize;
	std::vector<ComputeShaderParam> params;
};

struct ComputeShaderResource {
	DECLARE_SERIALIZE_NO_PPTR (ComputeShaderResource)
	FastPropertyName name;
	int bindPoint;
};

struct ComputeShaderBuiltinSampler {
	DECLARE_SERIALIZE_NO_PPTR (ComputeShaderBuiltinSampler)
	BuiltinSamplerState	sampler; 
	int bindPoint;
};

struct ComputeShaderKernel {
	DECLARE_SERIALIZE_NO_PPTR (ComputeShaderKernel)

	FastPropertyName name;
	std::vector<ComputeShaderResource> cbs;
	// For textures, bind point is two 16 bit fields:
	// lower 16 bits - texture bind point
	// upper 16 bits - sampler bind point, or 0xFFFF if no sampler needed
	std::vector<ComputeShaderResource> textures;
	std::vector<ComputeShaderBuiltinSampler> builtinSamplers;
	std::vector<ComputeShaderResource> inBuffers;
	std::vector<ComputeShaderResource> outBuffers;
	dynamic_array<UInt8> code;
};



class ComputeShader : public NamedObject {
public:
	typedef std::vector<ComputeShaderKernel> KernelArray;
	typedef std::vector<ComputeShaderCB> CBArray;

public:
	REGISTER_DERIVED_CLASS (ComputeShader, NamedObject)
	DECLARE_OBJECT_SERIALIZE (ComputeShader)
	
	ComputeShader (MemLabelId label, ObjectCreationMode mode);
	// ~ComputeShader (); declared-by-macro

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	KernelArray& GetKernels() { return m_Kernels; }
	CBArray& GetConstantBuffers() { return m_ConstantBuffers; }

	#if UNITY_EDITOR
	const ShaderErrors& GetErrors() const { return m_Errors; }
	ShaderErrors& GetErrors() { return m_Errors; }
	#endif

	int FindKernel (const FastPropertyName& name) const;

	void SetValueParam (const FastPropertyName& name, int byteCount, const void* data);
	void SetTextureParam (unsigned kernelIdx, const FastPropertyName& name, TextureID tid);
	void SetBufferParam (unsigned kernelIdx, const FastPropertyName& name, ComputeBufferID handle);

	void DispatchComputeShader (unsigned kernelIdx, int threadsX, int threadsY, int threadsZ);

	void UnloadFromGfxDevice() { DestroyRuntimeData(); }
	void ReloadToGfxDevice() { CreateRuntimeData(); }

private:
	void DestroyRuntimeData ();
	void CreateRuntimeData ();

private:
	KernelArray	m_Kernels;
	CBArray		m_ConstantBuffers;

	ComputeProgramStruct* m_Programs;
	int		m_ProgramCount;

	UInt8*	m_DataBuffer;
	int		m_DataBufferSize;
	UInt32	m_CBDirty; // bit per CB
	UInt32	m_CBOffsets[kMaxSupportedConstantBuffers]; // offset of each CB into data
	UInt32	m_CBSizes[kMaxSupportedConstantBuffers];
	ConstantBufferHandle m_CBs[kMaxSupportedConstantBuffers];

	#if UNITY_EDITOR
	ShaderErrors m_Errors;
	#endif
};



// --------------------------------------------------------------------------



class ComputeBuffer : public NonCopyable
{
public:
	ComputeBuffer (size_t count, size_t stride, UInt32 flags);
	~ComputeBuffer ();

	ComputeBufferID GetBufferHandle() const { return m_BufferHandle; }
	size_t GetTotalSize() const { return m_Count * m_Stride; }
	size_t GetCount() const { return m_Count; }
	size_t GetStride() const { return m_Stride; }

	void SetData (const void* data, size_t size);
	void GetData (void* dest, size_t destSize);

	static void CopyCount (ComputeBuffer* src, ComputeBuffer* dst, UInt32 dstOffset);

	static void UnloadAllFromGfxDevice();
	static void ReloadAllToGfxDevice();

private:
	void UnloadFromGfxDevice();
	void ReloadToGfxDevice();

private:
	size_t	m_Count;
	size_t	m_Stride;
	UInt32	m_Flags;
	ComputeBufferID	m_BufferHandle;
	ListNode<ComputeBuffer> m_ComputeBuffersNode;
};
