// Copyright Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"
#include "Hash/CityHash.h"

static TAutoConsoleVariable<float> CVarPSOStallWarningThresholdInMs(
	TEXT("D3D12.PSO.StallWarningThresholdInMs"),
	100.0f,
	TEXT("Sets a threshold of when to logs messages about stalls due to PSO creation.\n")
	TEXT("Value is in milliseconds. (100 is the default)\n"),
	ECVF_ReadOnly);

/// @cond DOXYGEN_WARNINGS

FD3D12LowLevelGraphicsPipelineStateDesc GetLowLevelGraphicsPipelineStateDesc(const FGraphicsPipelineStateInitializer& Initializer, const FD3D12RootSignature* RootSignature)
{
	// Memzero because we hash using the entire struct and we need to clear any padding.
	FD3D12LowLevelGraphicsPipelineStateDesc Desc;
	FMemory::Memzero(&Desc, sizeof(Desc));

	Desc.pRootSignature = RootSignature;
	Desc.Desc.pRootSignature = RootSignature->GetRootSignature();

#if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
	Desc.Desc.BlendState = Initializer.BlendState ? FD3D12DynamicRHI::ResourceCast(Initializer.BlendState)->Desc : CD3DX12_BLEND_DESC(D3D12_DEFAULT);
#endif // #if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
#if !D3D12_USE_DERIVED_PSO
	Desc.Desc.SampleMask = 0xFFFFFFFF;
	Desc.Desc.RasterizerState = Initializer.RasterizerState ? FD3D12DynamicRHI::ResourceCast(Initializer.RasterizerState)->Desc : CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	Desc.Desc.DepthStencilState = Initializer.DepthStencilState ? CD3DX12_DEPTH_STENCIL_DESC1(FD3D12DynamicRHI::ResourceCast(Initializer.DepthStencilState)->Desc) : CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEFAULT);
#endif // !D3D12_USE_DERIVED_PSO

	if (Initializer.BoundShaderState.HullShaderRHI && Initializer.BoundShaderState.DomainShaderRHI)
	{
		Desc.Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	}
	else
	{
		Desc.Desc.PrimitiveTopologyType = D3D12PrimitiveTypeToTopologyType(TranslatePrimitiveType(Initializer.PrimitiveType));
	}

	TranslateRenderTargetFormats(Initializer, Desc.Desc.RTFormatArray, Desc.Desc.DSVFormat);

	Desc.Desc.SampleDesc.Count = Initializer.NumSamples;
	Desc.Desc.SampleDesc.Quality = GetMaxMSAAQuality(Initializer.NumSamples);

	if (FD3D12VertexDeclaration* InputLayout = (FD3D12VertexDeclaration*) Initializer.BoundShaderState.VertexDeclarationRHI)
	{
		Desc.Desc.InputLayout.NumElements        = InputLayout->VertexElements.Num();
		Desc.Desc.InputLayout.pInputElementDescs = InputLayout->VertexElements.GetData();
		Desc.InputLayoutHash = InputLayout->Hash;
	}

#define COPY_SHADER(Initial, Name) \
	if (FD3D12##Name##Shader* Shader = (FD3D12##Name##Shader*) Initializer.BoundShaderState.##Name##ShaderRHI) \
	{ \
		Desc.Desc.Initial##S = Shader->ShaderBytecode.GetShaderBytecode(); \
		Desc.Initial##SHash = Shader->ShaderBytecode.GetHash(); \
	}
	COPY_SHADER(V, Vertex);
	COPY_SHADER(P, Pixel);
	COPY_SHADER(D, Domain);
	COPY_SHADER(H, Hull);
	COPY_SHADER(G, Geometry);
#undef COPY_SHADER

#if PLATFORM_WINDOWS
#define EXT_SHADER(Initial, Name) \
	if (FD3D12##Name##Shader* Shader = (FD3D12##Name##Shader*) Initializer.BoundShaderState.##Name##ShaderRHI) \
	{ \
		if (Shader->VendorExtensions.Num() > 0) \
		{ \
			Desc.##Initial##SExtensions = &Shader->VendorExtensions; \
		} \
	}
	EXT_SHADER(V, Vertex);
	EXT_SHADER(P, Pixel);
	EXT_SHADER(D, Domain);
	EXT_SHADER(H, Hull);
	EXT_SHADER(G, Geometry);
#undef EXT_SHADER
#endif

#if PLATFORM_WINDOWS
	// TODO: [PSO API] For now, keep DBT enabled, if available, until it is added as part of a member to the Initializer's DepthStencilState
	Desc.Desc.DepthStencilState.DepthBoundsTestEnable = GSupportsDepthBoundsTest && Initializer.bDepthBounds;
#endif

	Desc.bFromPSOFileCache = Initializer.bFromPSOFileCache;

	return Desc;
}

FD3D12ComputePipelineStateDesc GetComputePipelineStateDesc(const FD3D12ComputeShader* ComputeShader)
{
	// Memzero because we hash using the entire struct and we need to clear any padding.
	FD3D12ComputePipelineStateDesc Desc;
	FMemory::Memzero(&Desc, sizeof(Desc));

	Desc.pRootSignature = ComputeShader->pRootSignature;
	Desc.Desc.pRootSignature = Desc.pRootSignature->GetRootSignature();
	Desc.Desc.CS = ComputeShader->ShaderBytecode.GetShaderBytecode();
	Desc.CSHash = ComputeShader->ShaderBytecode.GetHash();
#if PLATFORM_WINDOWS
	if (ComputeShader->VendorExtensions.Num() > 0)
	{
		Desc.Extensions = &ComputeShader->VendorExtensions;
	}
#endif

	return Desc;
}

FD3D12PipelineStateWorker::FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const ComputePipelineCreationArgs& InArgs)
	: FD3D12AdapterChild(Adapter)
	, bIsGraphics(false)
{
	CreationArgs.ComputeArgs = new ComputePipelineCreationArgs_POD();
	CreationArgs.ComputeArgs->Init(InArgs.Args);
};

FD3D12PipelineStateWorker::FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const GraphicsPipelineCreationArgs& InArgs)
	: FD3D12AdapterChild(Adapter)
	, bIsGraphics(true)
{
	CreationArgs.GraphicsArgs = new GraphicsPipelineCreationArgs_POD();
	CreationArgs.GraphicsArgs->Init(InArgs.Args);
};

/// @endcond

uint64 FD3D12PipelineStateCacheBase::HashData(const void* Data, int32 NumBytes)
{
	return CityHash64((const char*) Data, NumBytes);
}

uint64 FD3D12PipelineStateCacheBase::HashPSODesc(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
{
	struct GraphicsPSOData
	{
		ShaderBytecodeHash VSHash;
		ShaderBytecodeHash HSHash;
		ShaderBytecodeHash DSHash;
		ShaderBytecodeHash GSHash;
		ShaderBytecodeHash PSHash;
		uint32 InputLayoutHash;

#if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
		uint8 AlphaToCoverageEnable;
		uint8 IndependentBlendEnable;
#endif // #if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
#if !D3D12_USE_DERIVED_PSO
		uint32 SampleMask;
		D3D12_RASTERIZER_DESC RasterizerState;
		D3D12_DEPTH_STENCIL_DESC1 DepthStencilState;
#endif // #if !D3D12_USE_DERIVED_PSO

		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
		D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
		DXGI_FORMAT DSVFormat;
		DXGI_SAMPLE_DESC SampleDesc;
		uint32 NodeMask;

		D3D12_PIPELINE_STATE_FLAGS Flags;
	};

	struct RenderTargetData
	{
		DXGI_FORMAT Format;
#if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
		D3D12_RENDER_TARGET_BLEND_DESC BlendDesc;
#endif // #if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
	};


	const int32 NumRenderTargets = Desc.Desc.RTFormatArray.NumRenderTargets;

	const size_t GraphicsPSODataSize = sizeof(GraphicsPSOData);
	const size_t RenderTargetDataSize = NumRenderTargets * sizeof(RenderTargetData);
	const size_t TotalDataSize = GraphicsPSODataSize + RenderTargetDataSize;

	char Data[GraphicsPSODataSize + 8 * sizeof(RenderTargetData)];
	FMemory::Memzero(Data, TotalDataSize);

	GraphicsPSOData* PSOData = (GraphicsPSOData*) Data;
	RenderTargetData* RTData = (RenderTargetData*) (Data + GraphicsPSODataSize);

	PSOData->VSHash          = Desc.VSHash;
	PSOData->HSHash          = Desc.HSHash;
	PSOData->DSHash          = Desc.DSHash;
	PSOData->GSHash          = Desc.GSHash;
	PSOData->PSHash          = Desc.PSHash;
	PSOData->InputLayoutHash = Desc.InputLayoutHash;

#if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
	PSOData->AlphaToCoverageEnable  = Desc.Desc.BlendState.AlphaToCoverageEnable;
	PSOData->IndependentBlendEnable = Desc.Desc.BlendState.IndependentBlendEnable;
#endif // #if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
#if !D3D12_USE_DERIVED_PSO
	PSOData->SampleMask             = Desc.Desc.SampleMask;
	PSOData->RasterizerState        = Desc.Desc.RasterizerState;
	PSOData->DepthStencilState      = Desc.Desc.DepthStencilState;
#endif // #if !D3D12_USE_DERIVED_PSO
	PSOData->IBStripCutValue        = Desc.Desc.IBStripCutValue;
	PSOData->PrimitiveTopologyType  = Desc.Desc.PrimitiveTopologyType;
	PSOData->DSVFormat              = Desc.Desc.DSVFormat;
	PSOData->SampleDesc             = Desc.Desc.SampleDesc;
	PSOData->NodeMask               = Desc.Desc.NodeMask;
	PSOData->Flags                  = Desc.Desc.Flags;

	for (int32 RT = 0; RT < NumRenderTargets; RT++)
	{
		RTData[RT].Format    = Desc.Desc.RTFormatArray.RTFormats[RT];
#if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
		RTData[RT].BlendDesc = Desc.Desc.BlendState.RenderTarget[RT];
#endif // #if !D3D12_USE_DERIVED_PSO || D3D12_USE_DERIVED_PSO_SHADER_EXPORTS
	}

	return HashData(Data, TotalDataSize);
}

uint64 FD3D12PipelineStateCacheBase::HashPSODesc(const FD3D12ComputePipelineStateDesc& Desc)
{
	struct ComputePSOData
	{
		ShaderBytecodeHash CSHash;
		UINT NodeMask;
		D3D12_PIPELINE_STATE_FLAGS Flags;
	};
	ComputePSOData Data;
	FMemory::Memzero(Data);
	Data.CSHash   = Desc.CSHash;
	Data.NodeMask = Desc.Desc.NodeMask;
	Data.Flags    = Desc.Desc.Flags;

	return HashData(&Data, sizeof(Data));
}

FD3D12PipelineStateCacheBase::FD3D12PipelineStateCacheBase(FD3D12Adapter* InParent)
	: FD3D12AdapterChild(InParent)
{
}

FD3D12PipelineStateCacheBase::~FD3D12PipelineStateCacheBase()
{
	CleanupPipelineStateCaches();
}


FD3D12PipelineState::FD3D12PipelineState(FD3D12Adapter* Parent)
	: FD3D12AdapterChild(Parent)
	, FD3D12MultiNodeGPUObject(FRHIGPUMask::All(), FRHIGPUMask::All()) //Create on all, visible on all
	, Worker(nullptr)
	, InitState(PSOInitState::Uninitialized)
{
	INC_DWORD_STAT(STAT_D3D12NumPSOs);
}

FD3D12PipelineState::~FD3D12PipelineState()
{
	if (Worker)
	{
		Worker->EnsureCompletion(true);
		delete Worker;
		Worker = nullptr;
	}

	DEC_DWORD_STAT(STAT_D3D12NumPSOs);
}

ID3D12PipelineState* FD3D12PipelineState::InternalGetPipelineState()
{
	// Only one thread should finalize any tasks and set the cached pipeline state.
	// Other threads can wait efficiently behind the lock. Using Sleep() can wake threads 1ms too late.
	FRWScopeLock Lock(GetPipelineStateMutex, SLT_Write);

	if (Worker)
	{
		check(InitState == PSOInitState::Uninitialized);

		Worker->EnsureCompletion(true);
		check(Worker->IsWorkDone());

		PipelineState = Worker->GetTask().PSO.GetReference();

		// Cleanup the worker.
		delete Worker;
		Worker = nullptr;

		InitState = (PipelineState.GetReference() != nullptr)? PSOInitState::Initialized : PSOInitState::CreationFailed;
	}
	else
	{
		// Busy-wait for the PSO. This avoids giving up our time slice.
		if (InitState == PSOInitState::Uninitialized)
		{
			double StartTime = FPlatformTime::Seconds();
			double BusyWaitWarningTime = CVarPSOStallWarningThresholdInMs.GetValueOnAnyThread() * 0.001;
			while (InitState == PSOInitState::Uninitialized)
			{
				const double Time = FPlatformTime::Seconds();

				if (Time - StartTime > BusyWaitWarningTime)
				{
					UE_LOG(LogD3D12RHI, Warning, TEXT("Waited for PSO creation for %fms"), BusyWaitWarningTime * 1000.0);
					BusyWaitWarningTime *= 2.0;
				}
			}
		}
	}

	return PipelineState.GetReference();
}

FD3D12GraphicsPipelineState::FD3D12GraphicsPipelineState(
	const FGraphicsPipelineStateInitializer& Initializer,
	const FD3D12RootSignature* InRootSignature,
	FD3D12PipelineState* InPipelineState)
	: PipelineStateInitializer(Initializer)
	, RootSignature(InRootSignature)
	, PipelineState(InPipelineState)
{
	// hold on to bound RHI resources
	PipelineStateInitializer.BoundShaderState.AddRefResources();

	if (Initializer.BoundShaderState.VertexDeclarationRHI)
		FMemory::Memcpy(StreamStrides, ((FD3D12VertexDeclaration*) Initializer.BoundShaderState.VertexDeclarationRHI)->StreamStrides, sizeof(StreamStrides));
	else
		FMemory::Memzero(StreamStrides, sizeof(StreamStrides));
	bShaderNeedsGlobalConstantBuffer[SF_Vertex] = GetVertexShader() && GetVertexShader()->ResourceCounts.bGlobalUniformBufferUsed;
	bShaderNeedsGlobalConstantBuffer[SF_Pixel] = GetPixelShader() && GetPixelShader()->ResourceCounts.bGlobalUniformBufferUsed;
	bShaderNeedsGlobalConstantBuffer[SF_Hull] = GetHullShader() && GetHullShader()->ResourceCounts.bGlobalUniformBufferUsed;
	bShaderNeedsGlobalConstantBuffer[SF_Domain] = GetDomainShader() && GetDomainShader()->ResourceCounts.bGlobalUniformBufferUsed;
	bShaderNeedsGlobalConstantBuffer[SF_Geometry] = GetGeometryShader() && GetGeometryShader()->ResourceCounts.bGlobalUniformBufferUsed;
}

FD3D12GraphicsPipelineState::~FD3D12GraphicsPipelineState()
{
	// At this point the object is not safe to use in the PSO cache.
	// Currently, the PSO cache manages the lifetime but we could potentially
	// stop doing an AddRef() and remove the PipelineState from any caches at this point.

#if D3D12_USE_DERIVED_PSO
	// On XboxOne the pipeline state is the derived object.
	delete PipelineState;
	PipelineState = nullptr;
#endif // D3D12_USE_DERIVED_PSO

	// release bound RHI resources
	PipelineStateInitializer.BoundShaderState.ReleaseResources();
}

FD3D12ComputePipelineState::~FD3D12ComputePipelineState()
{
	// At this point the object is not safe to use in the PSO cache.
	// Currently, the PSO cache manages the lifetime but we could potentially
	// stop doing an AddRef() and remove the PipelineState from any caches at this point.
}

void FD3D12PipelineStateCacheBase::CleanupPipelineStateCaches()
{
#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	// The runtime caches manage the lifetime of their FD3D12GraphicsPipelineState and FD3D12ComputePipelineState.
	// We need to release them.
	{
		FRWScopeLock Lock(InitializerToGraphicsPipelineMapMutex, FRWScopeLockType::SLT_Write);
		for (auto Pair : InitializerToGraphicsPipelineMap)
		{
			FD3D12GraphicsPipelineState* GraphicsPipelineState = Pair.Value;
			ensure(GIsRHIInitialized || (!GIsRHIInitialized && GraphicsPipelineState->GetRefCount() == 1));
			GraphicsPipelineState->Release();
		}
		InitializerToGraphicsPipelineMap.Reset();
	}

	{
		FRWScopeLock Lock(ComputeShaderToComputePipelineMapMutex, FRWScopeLockType::SLT_Write);
		for (auto Pair : ComputeShaderToComputePipelineMap)
		{
			FD3D12ComputePipelineState* ComputePipelineState = Pair.Value;
			ensure(GIsRHIInitialized || (!GIsRHIInitialized && ComputePipelineState->GetRefCount() == 1));
			ComputePipelineState->Release();
		}
		ComputeShaderToComputePipelineMap.Reset();
	}
#endif
	{
		FRWScopeLock Lock(LowLevelGraphicsPipelineStateCacheMutex, FRWScopeLockType::SLT_Write);
		for (auto Iter = LowLevelGraphicsPipelineStateCache.CreateConstIterator(); Iter; ++Iter)
		{
			const FD3D12PipelineState* const PipelineState = Iter.Value();
			delete PipelineState;
		}
		LowLevelGraphicsPipelineStateCache.Empty();
	}

	{
		FRWScopeLock Lock(ComputePipelineStateCacheMutex, FRWScopeLockType::SLT_Write);
		for (auto Iter = ComputePipelineStateCache.CreateConstIterator(); Iter; ++Iter)
		{
			const FD3D12PipelineState* const PipelineState = Iter.Value();
			delete PipelineState;
		}
		ComputePipelineStateCache.Empty();
	}
}

#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
FD3D12GraphicsPipelineState* FD3D12PipelineStateCacheBase::AddToRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, const FD3D12RootSignature* RootSignature, FD3D12PipelineState* PipelineState)
{
	// Lifetime managed by the runtime cache. AddRef() so the upper level doesn't delete the FD3D12GraphicsPipelineState objects while they're still in the runtime cache.
	// One alternative is to remove the object from the runtime cache in the FD3D12GraphicsPipelineState destructor.
	FD3D12GraphicsPipelineState* const GraphicsPipelineState = new FD3D12GraphicsPipelineState(Initializer, RootSignature, PipelineState);
	GraphicsPipelineState->AddRef();

	check(GraphicsPipelineState && InitializerHash != 0);
	check(GraphicsPipelineState->PipelineState != nullptr);

	{
		FRWScopeLock Lock(InitializerToGraphicsPipelineMapMutex, FRWScopeLockType::SLT_Write);
		InitializerToGraphicsPipelineMap.Add(FInitializerToGPSOMapKey(&GraphicsPipelineState->PipelineStateInitializer, InitializerHash), GraphicsPipelineState);
	}

	INC_DWORD_STAT(STAT_PSOGraphicsNumHighlevelCacheEntries);
	return GraphicsPipelineState;
}
#endif

FD3D12PipelineState* FD3D12PipelineStateCacheBase::FindInLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
{
	check(Desc.CombinedHash != 0);

	{
		FRWScopeLock Lock(LowLevelGraphicsPipelineStateCacheMutex, FRWScopeLockType::SLT_ReadOnly);
		FD3D12PipelineState** Found = LowLevelGraphicsPipelineStateCache.Find(Desc);
		if (Found)
		{
			INC_DWORD_STAT(STAT_PSOGraphicsLowlevelCacheHit);
			return *Found;
		}
	}

	INC_DWORD_STAT(STAT_PSOGraphicsLowlevelCacheMiss);
	return nullptr;
}

FD3D12PipelineState* FD3D12PipelineStateCacheBase::CreateAndAddToLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
{
	// Add PSO to low level cache.
	FD3D12PipelineState* PipelineState = nullptr;
	AddToLowLevelCache(Desc, &PipelineState, [this](FD3D12PipelineState** PipelineState, const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
	{ 
		OnPSOCreated(*PipelineState, Desc);
	});

	return PipelineState;
}

void FD3D12PipelineStateCacheBase::AddToLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc, FD3D12PipelineState** OutPipelineState, const FPostCreateGraphicCallback& PostCreateCallback)
{
	check(Desc.CombinedHash != 0);

	// Double check the desc doesn't already exist while the lock is taken.
	// This avoids having multiple threads try to create the same PSO.
	{
		FRWScopeLock Lock(LowLevelGraphicsPipelineStateCacheMutex, FRWScopeLockType::SLT_Write);
		FD3D12PipelineState** const PipelineState = LowLevelGraphicsPipelineStateCache.Find(Desc);
		if (PipelineState)
		{
			// This desc already exists.
			*OutPipelineState = *PipelineState;
			return;
		}

		// Add the FD3D12PipelineState object to the cache, but don't actually create the underlying PSO yet while the lock is taken.
		// This allows multiple threads to create different PSOs at the same time.
		INC_DWORD_STAT(STAT_PSOGraphicsNumLowlevelCacheEntries);
		FD3D12PipelineState* NewPipelineState = new FD3D12PipelineState(GetParentAdapter());
		LowLevelGraphicsPipelineStateCache.Add(Desc, NewPipelineState);

		*OutPipelineState = NewPipelineState;
	}

	// Create the underlying PSO and then perform any other additional tasks like cleaning up/adding to caches, etc.
	PostCreateCallback(OutPipelineState, Desc);
}

#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
FD3D12ComputePipelineState* FD3D12PipelineStateCacheBase::AddToRuntimeCache(FD3D12ComputeShader* ComputeShader, FD3D12PipelineState* PipelineState)
{
	// Lifetime managed by the runtime cache. AddRef() so the upper level doesn't delete the FD3D12ComputePipelineState objects while they're still in the runtime cache.
	// One alternative is to remove the object from the runtime cache in the FD3D12ComputePipelineState destructor.
	FD3D12ComputePipelineState* const ComputePipelineState = new FD3D12ComputePipelineState(ComputeShader, PipelineState);
	ComputePipelineState->AddRef();

	check(ComputePipelineState && ComputeShader != nullptr);
	check(ComputePipelineState->PipelineState != nullptr);

	{
		FRWScopeLock Lock(ComputeShaderToComputePipelineMapMutex, FRWScopeLockType::SLT_Write);
		ComputeShaderToComputePipelineMap.Add(ComputeShader, ComputePipelineState);
	}

	INC_DWORD_STAT(STAT_PSOComputeNumHighlevelCacheEntries);
	return ComputePipelineState;
}
#endif

FD3D12PipelineState* FD3D12PipelineStateCacheBase::FindInLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc)
{
	check(Desc.CombinedHash != 0);

	{
		FRWScopeLock Lock(ComputePipelineStateCacheMutex, FRWScopeLockType::SLT_ReadOnly);
		FD3D12PipelineState** Found = ComputePipelineStateCache.Find(Desc);
		if (Found)
		{
			INC_DWORD_STAT(STAT_PSOComputeLowlevelCacheHit);
			return *Found;
		}
	}

	INC_DWORD_STAT(STAT_PSOComputeLowlevelCacheMiss);
	return nullptr;
}

FD3D12PipelineState* FD3D12PipelineStateCacheBase::CreateAndAddToLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc)
{
	// Add PSO to low level cache.
	FD3D12PipelineState* PipelineState = nullptr;

	AddToLowLevelCache(Desc, &PipelineState, [&](FD3D12PipelineState* PipelineState, const FD3D12ComputePipelineStateDesc& Desc)
	{ 
		OnPSOCreated(PipelineState, Desc); 
	});

	return PipelineState;
}

void FD3D12PipelineStateCacheBase::AddToLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc, FD3D12PipelineState** OutPipelineState, const FPostCreateComputeCallback& PostCreateCallback)
{
	check(Desc.CombinedHash != 0);

	// Double check the desc doesn't already exist while the lock is taken.
	// This avoids having multiple threads try to create the same PSO.
	{
		FRWScopeLock Lock(ComputePipelineStateCacheMutex, FRWScopeLockType::SLT_Write);
		FD3D12PipelineState** const PipelineState = ComputePipelineStateCache.Find(Desc);
		if (PipelineState)
		{
			// This desc already exists.
			*OutPipelineState = *PipelineState;
			return;
		}

		// Add the FD3D12PipelineState object to the cache, but don't actually create the underlying PSO yet while the lock is taken.
		// This allows multiple threads to create different PSOs at the same time.
		INC_DWORD_STAT(STAT_PSOComputeNumLowlevelCacheEntries);
		FD3D12PipelineState* NewPipelineState = new FD3D12PipelineState(GetParentAdapter());
		ComputePipelineStateCache.Add(Desc, NewPipelineState);

		*OutPipelineState = NewPipelineState;
	}

	// Create the underlying PSO and then perform any other additional tasks like cleaning up/adding to caches, etc.
	PostCreateCallback(*OutPipelineState, Desc);
}

#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
FD3D12GraphicsPipelineState* FD3D12PipelineStateCacheBase::FindInRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32& OutHash)
{
	OutHash = HashData(&Initializer, sizeof(Initializer));

	{
		FRWScopeLock Lock(InitializerToGraphicsPipelineMapMutex, FRWScopeLockType::SLT_ReadOnly);
		FD3D12GraphicsPipelineState** GraphicsPipelineState = InitializerToGraphicsPipelineMap.Find(FInitializerToGPSOMapKey(&Initializer, OutHash));
		if (GraphicsPipelineState)
		{
			INC_DWORD_STAT(STAT_PSOGraphicsHighlevelCacheHit);
			return *GraphicsPipelineState;
		}
	}

	INC_DWORD_STAT(STAT_PSOGraphicsHighlevelCacheMiss);
	return nullptr;
}
#endif

FD3D12GraphicsPipelineState* FD3D12PipelineStateCacheBase::FindInLoadedCache(
	const FGraphicsPipelineStateInitializer& Initializer,
#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	uint32 InitializerHash,
#endif
	const FD3D12RootSignature* RootSignature,
	FD3D12LowLevelGraphicsPipelineStateDesc& OutLowLevelDesc)
{
	// TODO: For now PSOs will be created on every node of the LDA chain.
	OutLowLevelDesc = GetLowLevelGraphicsPipelineStateDesc(Initializer, RootSignature);
	OutLowLevelDesc.Desc.NodeMask = FRHIGPUMask::All().GetNative();

	OutLowLevelDesc.CombinedHash = FD3D12PipelineStateCacheBase::HashPSODesc(OutLowLevelDesc);

	// First try to find the PSO in the low level cache that can be populated from disk.
	FD3D12PipelineState* PipelineState = FindInLowLevelCache(OutLowLevelDesc);
#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	if (PipelineState)
	{
		// Add the PSO to the runtime cache for better performance next time.
		return AddToRuntimeCache(Initializer, InitializerHash, RootSignature, PipelineState);
	}

	// TODO: Try to load from a PipelineLibrary now instead of at Create time.

	return nullptr;
#else
	if (PipelineState && PipelineState->IsValid())
	{
		return new FD3D12GraphicsPipelineState(Initializer, RootSignature, PipelineState);
	}

	return nullptr;
#endif
}

FD3D12GraphicsPipelineState* FD3D12PipelineStateCacheBase::CreateAndAdd(
	const FGraphicsPipelineStateInitializer& Initializer,
#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	uint32 InitializerHash,
#endif
	const FD3D12RootSignature* RootSignature,
	const FD3D12LowLevelGraphicsPipelineStateDesc& LowLevelDesc)
{
	FD3D12PipelineState* const PipelineState = CreateAndAddToLowLevelCache(LowLevelDesc);
#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	if (PipelineState == nullptr)
	{
		return nullptr;
	}

	// Add the PSO to the runtime cache for better performance next time.
	return AddToRuntimeCache(Initializer, InitializerHash, RootSignature, PipelineState);
#else
	if (PipelineState && PipelineState->IsValid())
	{
		return new FD3D12GraphicsPipelineState(Initializer, RootSignature, PipelineState);
	}

	return nullptr;
#endif
}

#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
FD3D12ComputePipelineState* FD3D12PipelineStateCacheBase::FindInRuntimeCache(const FD3D12ComputeShader* ComputeShader)
{
	{
		FRWScopeLock Lock(ComputeShaderToComputePipelineMapMutex, FRWScopeLockType::SLT_ReadOnly);
		FD3D12ComputePipelineState** ComputePipelineState = ComputeShaderToComputePipelineMap.Find(ComputeShader);
		if (ComputePipelineState)
		{
			INC_DWORD_STAT(STAT_PSOComputeHighlevelCacheHit);
			return *ComputePipelineState;
		}
	}

	INC_DWORD_STAT(STAT_PSOComputeHighlevelCacheMiss);
	return nullptr;
}
#endif

FD3D12ComputePipelineState* FD3D12PipelineStateCacheBase::FindInLoadedCache(FD3D12ComputeShader* ComputeShader, FD3D12ComputePipelineStateDesc& OutLowLevelDesc)
{
	// TODO: For now PSOs will be created on every node of the LDA chain.
	OutLowLevelDesc = GetComputePipelineStateDesc(ComputeShader);
	OutLowLevelDesc.Desc.NodeMask = FRHIGPUMask::All().GetNative();
	OutLowLevelDesc.CombinedHash = FD3D12PipelineStateCacheBase::HashPSODesc(OutLowLevelDesc);

	// First try to find the PSO in the low level cache that can be populated from disk.
	FD3D12PipelineState* PipelineState = FindInLowLevelCache(OutLowLevelDesc);
#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	if (PipelineState)
	{
		// Add the PSO to the runtime cache for better performance next time.
		return AddToRuntimeCache(ComputeShader, PipelineState);
	}

	// TODO: Try to load from a PipelineLibrary now instead of at Create time.

	return nullptr;
#else
	if (PipelineState && PipelineState->IsValid())
	{
		return new FD3D12ComputePipelineState(ComputeShader, PipelineState);
	}

	return nullptr;
#endif
}

FD3D12ComputePipelineState* FD3D12PipelineStateCacheBase::CreateAndAdd(FD3D12ComputeShader* ComputeShader, const FD3D12ComputePipelineStateDesc& LowLevelDesc)
{
	FD3D12PipelineState* const PipelineState = CreateAndAddToLowLevelCache(LowLevelDesc);
#if D3D12RHI_USE_HIGH_LEVEL_PSO_CACHE
	// Add the PSO to the runtime cache for better performance next time.
	return AddToRuntimeCache(ComputeShader, PipelineState);
#else
	if (PipelineState && PipelineState->IsValid())
	{
		return new FD3D12ComputePipelineState(ComputeShader, PipelineState);
	}

	return nullptr;
#endif
}
