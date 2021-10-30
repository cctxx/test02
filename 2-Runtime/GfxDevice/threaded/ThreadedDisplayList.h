#ifndef THREADED_DISPLAY_LIST_H
#define THREADED_DISPLAY_LIST_H

#if ENABLE_MULTITHREADED_CODE

#include "Runtime/Threads/ThreadedStreamBuffer.h"
#include "Runtime/GfxDevice/GfxDisplayList.h"
#include "Runtime/GfxDevice/GfxPatchInfo.h"
#include "Runtime/GfxDevice/GpuProgram.h"

class GfxDeviceClient;

struct DisplayListContext
{
	DisplayListContext();	
	void ClearState();
	void Reset();

	enum
	{
		kFogParamsNone = -1,
		kFogParamsDisable = -2
	};

	ThreadedStreamBuffer commandQueue;
	GfxPatchInfo patchInfo;
	bool recordFailed;
	bool hasSetShaders;
	bool shadersActive[kShaderTypeCount];
	int fogParamsOffset;
};


class ThreadedDisplayList : public GfxDisplayList
{
public:
	ThreadedDisplayList(const void* data, size_t size, const DisplayListContext& context);
	~ThreadedDisplayList();

	void Call();

	UInt8*			GetData()		{ return m_ListData.m_Buffer.begin(); }
	const UInt8*	GetData() const	{ return m_ListData.m_Buffer.begin(); }
	size_t			GetSize() const	{ return m_ListData.m_Buffer.size(); }

	void Patch(ThreadedStreamBuffer& queue);

private:
	int CopyClientData(int offset, int size);
	void UpdateClientDevice(GfxDeviceClient& device);
	void DoLockstep();

	class PatchableData
	{
	public:
		PatchableData(const void* data, size_t size,
			const GfxPatchInfo& patchInfo);
		PatchableData();

		void CheckParametersValid();
		void WriteParameters(ThreadedStreamBuffer& queue);
		void PatchImmediate();
		void Patch(ThreadedStreamBuffer& queue);
		void PatchFloat(const GfxPatch& patch, float* dest);
		void PatchVector(const GfxPatch& patch, Vector4f* dest);
		void PatchMatrix(const GfxPatch& patch, Matrix4x4f* dest);
		void PatchBuffer(const GfxPatch& patch, ComputeBufferID* dest);
		void PatchTexEnvProperties(const GfxTexEnvPatch& patch, TexEnvProperties* dest);
		void PatchTexEnvMatrix(const GfxTexEnvPatch& patch, Matrix4x4f* dest);
		int AppendData(const void* data, int size);

		dynamic_array<UInt8> m_Buffer;
		GfxPatchInfo m_PatchInfo;
	};

	PatchableData m_ListData;
	PatchableData m_ClientData;
	bool m_HasSetShaders;
	bool m_ShadersActive[kShaderTypeCount];
	int m_FogParamsOffset;
};


#endif
#endif
