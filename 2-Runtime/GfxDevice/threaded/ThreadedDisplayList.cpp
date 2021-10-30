#include "UnityPrefix.h"

#if ENABLE_MULTITHREADED_CODE

#define DEBUG_GPU_PARAMETER_PATCHING (!UNITY_RELEASE)

#include "Runtime/GfxDevice/threaded/ThreadedDisplayList.h"
#include "Runtime/GfxDevice/threaded/GfxDeviceClient.h"
#include "Runtime/GfxDevice/threaded/GfxDeviceWorker.h"
#include "Runtime/GfxDevice/threaded/GfxCommands.h"
#include "Runtime/Threads/ThreadedStreamBuffer.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/shaderlab.h"

DisplayListContext::DisplayListContext()
{
	ClearState();
}

void DisplayListContext::ClearState()
{
	recordFailed = false;
	hasSetShaders = false;
	memset(shadersActive, 0, sizeof(shadersActive));
	fogParamsOffset = kFogParamsNone;
}

void DisplayListContext::Reset()
{
	ClearState();
	commandQueue.ResetGrowable();
	patchInfo.Reset();
}

	
ThreadedDisplayList::ThreadedDisplayList(const void* data, size_t size, const DisplayListContext& context)
	: m_ListData(data, size, context.patchInfo)
{
	m_HasSetShaders = context.hasSetShaders;
	memcpy(m_ShadersActive, context.shadersActive, sizeof(m_ShadersActive));

	if (context.fogParamsOffset >= 0)
		m_FogParamsOffset = CopyClientData(context.fogParamsOffset, sizeof(GfxFogParams));
	else
		m_FogParamsOffset = context.fogParamsOffset;
}

ThreadedDisplayList::~ThreadedDisplayList()
{
}

void ThreadedDisplayList::Call()
{
	GfxDeviceClient& device = (GfxDeviceClient&)GetGfxDevice();
	if (device.IsSerializing())
	{
		ThreadedStreamBuffer& queue = *device.GetCommandQueue();
		AddRef(); // Release again when call is finished
		queue.WriteValueType<GfxCommand>(kGfxCmd_DisplayList_Call);
		queue.WriteValueType<ThreadedDisplayList*>(this);
		m_ListData.WriteParameters(queue);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		m_ListData.PatchImmediate();
		device.GetGfxDeviceWorker()->CallImmediate(this);
	}
	UpdateClientDevice(device);
}

void ThreadedDisplayList::Patch(ThreadedStreamBuffer& queue)
{
	m_ListData.Patch(queue);
}

int ThreadedDisplayList::CopyClientData(int offset, int size)
{
	int newOffset = m_ClientData.AppendData(&m_ListData.m_Buffer[offset], size);
	int relativeOffset = newOffset - offset;
	for (int pt = 0; pt < GfxPatch::kTypeCount; pt++)
	{
		GfxPatch::Type patchType = GfxPatch::Type(pt);
		int patchCount = m_ListData.m_PatchInfo.GetPatchCount(patchType);
		for (int i = 0; i < patchCount; i++)
		{
			const GfxPatch& srcPatch = m_ListData.m_PatchInfo.GetPatch(patchType, i);
			int patchOffset = srcPatch.patchOffset;
			if (patchOffset >= offset && patchOffset < offset + size)
			{
				GfxPatch patch = srcPatch;
				patch.patchOffset += relativeOffset;
				m_ClientData.m_PatchInfo.AddPatch(patchType, patch);
			}
		}
	}
	return newOffset;
}

void ThreadedDisplayList::UpdateClientDevice(GfxDeviceClient& device)
{
	if (!m_ClientData.m_Buffer.empty())
		m_ClientData.PatchImmediate();

	if (m_FogParamsOffset != DisplayListContext::kFogParamsNone)
	{
		if (m_FogParamsOffset != DisplayListContext::kFogParamsDisable)
		{
			const void* data = &m_ClientData.m_Buffer[m_FogParamsOffset];
			const GfxFogParams& fogParams = *static_cast<const GfxFogParams*>(data);
			device.UpdateFogEnabled(fogParams);
		}
		else
			device.UpdateFogDisabled();
	}

	if (m_HasSetShaders)
		device.UpdateShadersActive(m_ShadersActive);
}

void ThreadedDisplayList::DoLockstep()
{
	GfxDeviceClient& device = (GfxDeviceClient&)GetGfxDevice();
	device.DoLockstep();
}

ThreadedDisplayList::PatchableData::PatchableData(const void* data, size_t size, const GfxPatchInfo& patchInfo)
	: m_PatchInfo(patchInfo)
{
	m_Buffer.resize_uninitialized(size);
	memcpy(m_Buffer.begin(), data, size);
}

ThreadedDisplayList::PatchableData::PatchableData()
{
}

void ThreadedDisplayList::PatchableData::CheckParametersValid()
{
#if DEBUG_GPU_PARAMETER_PATCHING
	BuiltinShaderParamValues& builtinParams = GetGfxDevice().GetBuiltinParamValues();
	using namespace ShaderLab;
	
	FastPropertyName name;
	size_t floatCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeFloat);
	for (size_t i = 0; i < floatCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeFloat, i);
		const float* src = reinterpret_cast<const float*>(patch.source);
		name.index = patch.nameIndex;
		if (name.IsBuiltin())
		{
			Assert(name.IsBuiltinVector());
			const float& val = *builtinParams.GetVectorParam(BuiltinShaderVectorParam(name.BuiltinIndex())).GetPtr();
			Assert(&val == src);
		}
		else
		{
			const float& val = g_GlobalProperties->GetFloat(name);
			Assert(src == NULL || &val == src);
		}
	}
	
	size_t vectorCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeVector);
	for (size_t i = 0; i < vectorCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeVector, i);
		const Vector4f* src = reinterpret_cast<const Vector4f*>(patch.source);
		name.index = patch.nameIndex;
		if (name.IsBuiltin())
		{
			Assert(name.IsBuiltinVector());
			const Vector4f& vec = builtinParams.GetVectorParam(BuiltinShaderVectorParam(name.BuiltinIndex()));
			Assert(&vec == src);
		}
		else
		{
			const Vector4f& vec = g_GlobalProperties->GetVector(name);
			Assert(src == NULL || &vec == src);
		}
	}

	size_t matrixCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeMatrix);
	for (size_t i = 0; i < matrixCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeMatrix, i);
		const Matrix4x4f* src = reinterpret_cast<const Matrix4x4f*>(patch.source);
		name.index = patch.nameIndex;
		if (name.IsBuiltin())
		{
			Assert(name.IsBuiltinMatrix());
			const Matrix4x4f* mat = &builtinParams.GetMatrixParam(BuiltinShaderMatrixParam(name.BuiltinIndex()));
			Assert(mat == src);
		}
		else
		{
			int count = 0;
			const Matrix4x4f* mat = reinterpret_cast<const Matrix4x4f*>(g_GlobalProperties->GetValueProp (name, &count));
			Assert (src == NULL || mat == src);
		}
	}

	const size_t bufferCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeBuffer);
	for (size_t i = 0; i < bufferCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeBuffer, i);
		const ComputeBufferID* src = reinterpret_cast<const ComputeBufferID*>(patch.source);
		name.index = patch.nameIndex;
		Assert (!name.IsBuiltin());
		const ComputeBufferID& buf = g_GlobalProperties->GetComputeBuffer(name);
		Assert(src == NULL || &buf == src);
	}
#endif
}

void ThreadedDisplayList::PatchableData::WriteParameters(ThreadedStreamBuffer& queue)
{
	CheckParametersValid();

	size_t floatCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeFloat);
	for (size_t i = 0; i < floatCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeFloat, i);
		float* dest = queue.GetWritePointer<float>();
		PatchFloat(patch, dest);
	}
	
	size_t vectorCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeVector);
	for (size_t i = 0; i < vectorCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeVector, i);
		Vector4f* dest = queue.GetWritePointer<Vector4f>();
		PatchVector(patch, dest);
	}

	size_t matrixCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeMatrix);
	for (size_t i = 0; i < matrixCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeMatrix, i);
		Matrix4x4f* dest = queue.GetWritePointer<Matrix4x4f>();
		PatchMatrix(patch, dest);
	}

	size_t texEnvCount = m_PatchInfo.GetTexEnvPatchCount();
	for (size_t i = 0; i < texEnvCount; i++)
	{
		const GfxTexEnvPatch& patch = m_PatchInfo.GetTexEnvPatch(i);
		if (patch.patchFlags & GfxTexEnvPatch::kPatchProperties)
		{
			TexEnvProperties* dest = queue.GetWritePointer<TexEnvProperties>();
			PatchTexEnvProperties(patch, dest);
		}
		if (patch.patchFlags & GfxTexEnvPatch::kPatchMatrix)
		{
			Matrix4x4f* dest = queue.GetWritePointer<Matrix4x4f>();
			PatchTexEnvMatrix(patch, dest);
		}
	}

	const size_t bufferCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeBuffer);
	for (size_t i = 0; i < bufferCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeBuffer, i);
		ComputeBufferID* dest = queue.GetWritePointer<ComputeBufferID>();
		PatchBuffer(patch, dest);
	}
}

void ThreadedDisplayList::PatchableData::PatchImmediate()
{
	CheckParametersValid();	

	size_t floatCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeFloat);
	for (size_t i = 0; i < floatCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeFloat, i);
		float* dest = reinterpret_cast<float*>(&m_Buffer[patch.patchOffset]);
		PatchFloat(patch, dest);
	}
	
	size_t vectorCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeVector);
	for (size_t i = 0; i < vectorCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeVector, i);
		Vector4f* dest = reinterpret_cast<Vector4f*>(&m_Buffer[patch.patchOffset]);
		PatchVector(patch, dest);
	}

	size_t matrixCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeMatrix);
	for (size_t i = 0; i < matrixCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeMatrix, i);
		Matrix4x4f* dest = reinterpret_cast<Matrix4x4f*>(&m_Buffer[patch.patchOffset]);
		PatchMatrix(patch, dest);
	}

	size_t texEnvCount = m_PatchInfo.GetTexEnvPatchCount();
	for (size_t i = 0; i < texEnvCount; i++)
	{
		const GfxTexEnvPatch& patch = m_PatchInfo.GetTexEnvPatch(i);
		TexEnvData* dest = reinterpret_cast<TexEnvData*>(&m_Buffer[patch.patchOffset]);

		if (patch.patchFlags & GfxTexEnvPatch::kPatchProperties)
			PatchTexEnvProperties(patch, dest);

		if (patch.patchFlags & GfxTexEnvPatch::kPatchMatrix)
			PatchTexEnvMatrix(patch, &dest->matrix);
	}

	size_t bufferCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeBuffer);
	for (size_t i = 0; i < bufferCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeBuffer, i);
		ComputeBufferID* dest = reinterpret_cast<ComputeBufferID*>(&m_Buffer[patch.patchOffset]);
		PatchBuffer(patch, dest);
	}
}

void ThreadedDisplayList::PatchableData::Patch(ThreadedStreamBuffer& queue)
{
	FastPropertyName name;

	size_t floatCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeFloat);
	for (size_t i = 0; i < floatCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeFloat, i);
		float val = queue.ReadValueType<float>();
		*reinterpret_cast<float*>(&m_Buffer[patch.patchOffset]) = val;
	}

	size_t vectorCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeVector);
	for (size_t i = 0; i < vectorCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeVector, i);
		const Vector4f& vec = queue.ReadValueType<Vector4f>();
		*reinterpret_cast<Vector4f*>(&m_Buffer[patch.patchOffset]) = vec;
	}

	size_t matrixCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeMatrix);
	for (size_t i = 0; i < matrixCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeMatrix, i);
		const Matrix4x4f& mat = queue.ReadValueType<Matrix4x4f>();
		*reinterpret_cast<Matrix4x4f*>(&m_Buffer[patch.patchOffset]) = mat;
	}

	size_t texEnvCount = m_PatchInfo.GetTexEnvPatchCount();
	for (size_t i = 0; i < texEnvCount; i++)
	{
		using namespace ShaderLab;
		const GfxTexEnvPatch& patch = m_PatchInfo.GetTexEnvPatch(i);
		TexEnvData& data = *reinterpret_cast<TexEnvData*>(&m_Buffer[patch.patchOffset]);
		if (patch.patchFlags & GfxTexEnvPatch::kPatchProperties)
		{
			TexEnvProperties& dest = data;
			// Patch doesn't know if matrix is identity
			// Fortunately we can just keep the old value
			bool savedIdentityMatrix = dest.identityMatrix;
			dest = queue.ReadValueType<TexEnvProperties>();
			dest.identityMatrix = savedIdentityMatrix;
		}
		if (patch.patchFlags & GfxTexEnvPatch::kPatchMatrix)
			data.matrix = queue.ReadValueType<Matrix4x4f>();
	}

	const size_t bufferCount = m_PatchInfo.GetPatchCount(GfxPatch::kTypeBuffer);
	for (size_t i = 0; i < bufferCount; i++)
	{
		const GfxPatch& patch = m_PatchInfo.GetPatch(GfxPatch::kTypeBuffer, i);
		const ComputeBufferID& buf = queue.ReadValueType<ComputeBufferID>();
		*reinterpret_cast<ComputeBufferID*>(&m_Buffer[patch.patchOffset]) = buf;
	}
}

void ThreadedDisplayList::PatchableData::PatchFloat(const GfxPatch& patch, float* dest)
{
	using namespace ShaderLab;
	FastPropertyName name;
	name.index = patch.nameIndex;
	DebugAssert(patch.source || (name.IsValid() && !name.IsBuiltin()));
	const float* src = static_cast<const float*>(patch.source);
	*dest = src ? *src : g_GlobalProperties->GetFloat(name);
}

void ThreadedDisplayList::PatchableData::PatchVector(const GfxPatch& patch, Vector4f* dest)
{
	using namespace ShaderLab;
	FastPropertyName name;
	name.index = patch.nameIndex;
	DebugAssert(patch.source || (name.IsValid() && !name.IsBuiltin()));
	const Vector4f* src = static_cast<const Vector4f*>(patch.source);
	*dest = src ? *src : g_GlobalProperties->GetVector(name);
}

void ThreadedDisplayList::PatchableData::PatchMatrix(const GfxPatch& patch, Matrix4x4f* dest)
{
	using namespace ShaderLab;
	FastPropertyName name;
	name.index = patch.nameIndex;
	DebugAssert(patch.source || (name.IsValid() && !name.IsBuiltin()));
	const Matrix4x4f* src = static_cast<const Matrix4x4f*>(patch.source);
	if (!src)
	{
		int count = 0;
		src = reinterpret_cast<const Matrix4x4f*>(g_GlobalProperties->GetValueProp (name, &count));
		if (count < 16)
			src = NULL;
	}
	*dest = src ? *src : Matrix4x4f::identity;
}

void ThreadedDisplayList::PatchableData::PatchBuffer(const GfxPatch& patch, ComputeBufferID* dest)
{
	using namespace ShaderLab;
	FastPropertyName name;
	name.index = patch.nameIndex;
	DebugAssert(patch.source || (name.IsValid() && !name.IsBuiltin()));
	const ComputeBufferID* src = static_cast<const ComputeBufferID*>(patch.source);
	*dest = src ? *src : g_GlobalProperties->GetComputeBuffer(name);
}

void ThreadedDisplayList::PatchableData::PatchTexEnvProperties(const GfxTexEnvPatch& patch, TexEnvProperties* dest)
{
	patch.texEnv->PrepareProperties(patch.nameIndex, dest);
}

void ThreadedDisplayList::PatchableData::PatchTexEnvMatrix(const GfxTexEnvPatch& patch, Matrix4x4f* dest)
{
	bool identity;
	patch.texEnv->PrepareMatrix(patch.matrixName, ShaderLab::g_GlobalProperties, dest, identity);
}

int ThreadedDisplayList::PatchableData::AppendData(const void* data, int size)
{
	int offset = m_Buffer.size();
	m_Buffer.resize_uninitialized(offset + size);
	memcpy(&m_Buffer[offset], data, size);
	return offset;
}


#endif
