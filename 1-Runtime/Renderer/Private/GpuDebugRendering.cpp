// Copyright Epic Games, Inc. All Rights Reserved.

#include "GpuDebugRendering.h"
#include "SceneRendering.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Containers/ResourceArray.h"
#include "CommonRenderResources.h"

namespace ShaderDrawDebug 
{
	// Console variables
	static int32 GShaderDrawDebug_Enable = 1;
	static FAutoConsoleVariableRef CVarShaderDrawEnable(
		TEXT("r.ShaderDrawDebug"),
		GShaderDrawDebug_Enable,
		TEXT("ShaderDrawDebug debugging toggle.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static int32 GShaderDrawDebug_MaxElementCount;
	static FAutoConsoleVariableRef CVarShaderDrawMaxElementCount(
		TEXT("r.ShaderDrawDebug.MaxElementCount"),
		GShaderDrawDebug_MaxElementCount,
		TEXT("ShaderDraw output buffer size in element.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarShaderDrawLock(
		TEXT("r.ShaderDrawDebug.Lock"),
		0,
		TEXT("Lock the shader draw buffer.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	bool IsShaderDrawDebugEnabled()
	{
#if WITH_EDITOR
		return GShaderDrawDebug_Enable > 0;
#else
		return false;
#endif
	}

	bool IsShaderDrawLocked()
	{
#if WITH_EDITOR
		return CVarShaderDrawLock.GetValueOnAnyThread() > 0;
#else
		return false;
#endif
	}

	static bool IsShaderDrawDebugEnabled(const EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsPCPlatform(Platform) && !IsOpenGLPlatform(Platform);
	}

	void SetEnabled(bool bInEnabled)
	{
#if WITH_EDITOR
		GShaderDrawDebug_Enable = bInEnabled ? 1 : 0;
#endif		
	}

	void SetMaxElementCount(uint32 MaxCount)
	{
#if WITH_EDITOR
		GShaderDrawDebug_MaxElementCount = FMath::Max(1024, int32(MaxCount));
#endif		
	}

	uint32 GetMaxElementCount()
	{
#if WITH_EDITOR
		return uint32(FMath::Max(1, GShaderDrawDebug_MaxElementCount));
#else
		return 0;
#endif			
	}

	bool IsShaderDrawDebugEnabled(const FViewInfo& View)
	{
		return IsShaderDrawDebugEnabled() && IsShaderDrawDebugEnabled(View.GetShaderPlatform());
	}

	// Note: Unaligned structures used for structured buffers is an unsupported and/or sparsely
	//         supported feature in VK (VK_EXT_scalar_block_layout) and Metal. Consequently, we
	//         do manual packing in order to accommodate.
	struct FPackedShaderDrawElement
	{
		// This is not packed as fp16 to be able to debug large scale data while preserving accuracy at short range.
		float Pos0_ColorX[4];		// float3 pos0 + packed color0
		float Pos1_ColorY[4];		// float3 pos1 + packed color1
	};

	// This needs to be allocated per view, or move into a more persistent place
	struct FLockedData
	{
		FRWBufferStructured Buffer;
		FRWBuffer IndirectBuffer;
		bool bIsLocked = false;
	};

	static FLockedData LockedData = {};

	//////////////////////////////////////////////////////////////////////////

	class FShaderDrawDebugClearCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FShaderDrawDebugClearCS);
		SHADER_USE_PARAMETER_STRUCT(FShaderDrawDebugClearCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, DataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, IndirectBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsShaderDrawDebugEnabled(Parameters.Platform);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING"), 1);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_CLEAR_CS"), 1);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderDrawDebugClearCS, "/Engine/Private/ShaderDrawDebug.usf", "ShaderDrawDebugClearCS", SF_Compute);


	//////////////////////////////////////////////////////////////////////////

	class FShaderDrawDebugCopyCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FShaderDrawDebugCopyCS);
		SHADER_USE_PARAMETER_STRUCT(FShaderDrawDebugCopyCS, FGlobalShader);

		class FBufferType : SHADER_PERMUTATION_INT("PERMUTATION_BUFFER_TYPE", 2);
		using FPermutationDomain = TShaderPermutationDomain<FBufferType>;

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(uint32, NumElements)
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, InBuffer)
			SHADER_PARAMETER_UAV(RWBuffer, OutBuffer)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, InStructuredBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer, OutStructuredBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsShaderDrawDebugEnabled(Parameters.Platform);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING"), 1);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_COPY_CS"), 1);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderDrawDebugCopyCS, "/Engine/Private/ShaderDrawDebug.usf", "ShaderDrawDebugCopyCS", SF_Compute);

	//////////////////////////////////////////////////////////////////////////

	class FShaderDrawDebugVS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FShaderDrawDebugVS);
		SHADER_USE_PARAMETER_STRUCT(FShaderDrawDebugVS, FGlobalShader);

		class FInputType : SHADER_PERMUTATION_INT("PERMUTATION_INPUT_TYPE", 2);
		using FPermutationDomain = TShaderPermutationDomain<FInputType>;

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
			SHADER_PARAMETER_SRV(StructuredBuffer, LockedShaderDrawDebugPrimitive)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, ShaderDrawDebugPrimitive)
			SHADER_PARAMETER_RDG_BUFFER(StructuredBuffer, IndirectBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsShaderDrawDebugEnabled(Parameters.Platform);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING"), 1);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_VS"), 1);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_PS"), 0);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderDrawDebugVS, "/Engine/Private/ShaderDrawDebug.usf", "ShaderDrawDebugVS", SF_Vertex);

	//////////////////////////////////////////////////////////////////////////

	class FShaderDrawDebugPS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FShaderDrawDebugPS);
		SHADER_USE_PARAMETER_STRUCT(FShaderDrawDebugPS, FGlobalShader);
		
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
			SHADER_PARAMETER(FIntPoint, DepthTextureResolution)
			SHADER_PARAMETER(FVector2D, DepthTextureInvResolution)
			SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

		using FPermutationDomain = TShaderPermutationDomain<>;

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsShaderDrawDebugEnabled(Parameters.Platform);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING"), 1);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_VS"), 0);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_PS"), 1);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderDrawDebugPS, "/Engine/Private/ShaderDrawDebug.usf", "ShaderDrawDebugPS", SF_Pixel);

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderDrawVSPSParameters , )
		SHADER_PARAMETER_STRUCT_INCLUDE(FShaderDrawDebugVS::FParameters, ShaderDrawVSParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FShaderDrawDebugPS::FParameters, ShaderDrawPSParameters)
	END_SHADER_PARAMETER_STRUCT()

	//////////////////////////////////////////////////////////////////////////

	void BeginView(FRHICommandListImmediate& InRHICmdList, FViewInfo& View)
	{
		if (!IsShaderDrawDebugEnabled(View) || !IsShaderDrawDebugEnabled())
		{
			return;
		}

		FRDGBuilder GraphBuilder(InRHICmdList);
		FRDGBufferRef DataBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FPackedShaderDrawElement), GetMaxElementCount()), TEXT("ShaderDrawDataBuffer"));
		FRDGBufferRef IndirectBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndirectParameters>(1), TEXT("ShaderDrawDataIndirectBuffer"));

		FShaderDrawDebugClearCS::FParameters* Parameters = GraphBuilder.AllocParameters<FShaderDrawDebugClearCS::FParameters>();
		Parameters->DataBuffer = GraphBuilder.CreateUAV(DataBuffer);
		Parameters->IndirectBuffer = GraphBuilder.CreateUAV(IndirectBuffer);

		TShaderMapRef<FShaderDrawDebugClearCS> ComputeShader(View.ShaderMap);

		// Note: we do not call ClearUnusedGraphResources here as we want to for the allocation of DataBuffer
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ShaderDrawClear"),
			Parameters,
			ERDGPassFlags::Compute,
			[Parameters, ComputeShader](FRHICommandList& RHICmdList)
		{
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *Parameters, FIntVector(1,1,1));
		});

		GraphBuilder.QueueBufferExtraction(DataBuffer, &View.ShaderDrawData.Buffer, ERHIAccess::UAVCompute);
		GraphBuilder.QueueBufferExtraction(IndirectBuffer, &View.ShaderDrawData.IndirectBuffer, ERHIAccess::UAVCompute);

		GraphBuilder.Execute();

		if (IsShaderDrawLocked() && !LockedData.bIsLocked)
		{
			LockedData.Buffer.Initialize(sizeof(FPackedShaderDrawElement), GetMaxElementCount(), 0U, TEXT("ShaderDrawDataBuffer"));
			LockedData.IndirectBuffer.Initialize(sizeof(uint32), 4, PF_R32_UINT, BUF_DrawIndirect, TEXT("ShaderDrawDataIndirectBuffer"));
		}

		View.ShaderDrawData.CursorPosition = View.CursorPos;
	}

	void DrawView(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef OutputTexture, FRDGTextureRef DepthTexture)
	{
		if (!IsShaderDrawDebugEnabled(View))
		{
			return;
		}

		auto RunPass = [&](
			bool bUseRdgInput,
			FRDGBufferRef DataBuffer, 
			FRDGBufferRef IndirectBuffer, 
			FShaderResourceViewRHIRef LockedDataBuffer,
			FRHIVertexBuffer* LockedIndirectBuffer)
		{
			FShaderDrawDebugVS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FShaderDrawDebugVS::FInputType>(bUseRdgInput ? 0 : 1);
			TShaderMapRef<FShaderDrawDebugVS> VertexShader(View.ShaderMap, PermutationVector);
			TShaderMapRef<FShaderDrawDebugPS> PixelShader(View.ShaderMap);

			FShaderDrawVSPSParameters * PassParameters = GraphBuilder.AllocParameters<FShaderDrawVSPSParameters >();
			PassParameters->ShaderDrawPSParameters.RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ELoad);
			PassParameters->ShaderDrawPSParameters.RenderTargets.DepthStencil = FDepthStencilBinding(DepthTexture, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilNop);
			PassParameters->ShaderDrawPSParameters.DepthTexture = DepthTexture;
			PassParameters->ShaderDrawPSParameters.DepthTextureResolution    = FIntPoint(DepthTexture->Desc.Extent.X, DepthTexture->Desc.Extent.Y);
			PassParameters->ShaderDrawPSParameters.DepthTextureInvResolution = FVector2D(1.f/DepthTexture->Desc.Extent.X, 1.f / DepthTexture->Desc.Extent.Y);
			PassParameters->ShaderDrawPSParameters.DepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->ShaderDrawVSParameters.View = View.ViewUniformBuffer;
			if (bUseRdgInput)
			{
				PassParameters->ShaderDrawVSParameters.ShaderDrawDebugPrimitive = GraphBuilder.CreateSRV(DataBuffer);
				PassParameters->ShaderDrawVSParameters.IndirectBuffer = IndirectBuffer;
			}
			else
			{
				PassParameters->ShaderDrawVSParameters.LockedShaderDrawDebugPrimitive = LockedDataBuffer;
			}

			ValidateShaderParameters(PixelShader, PassParameters->ShaderDrawPSParameters);
			ClearUnusedGraphResources(PixelShader, &PassParameters->ShaderDrawPSParameters, { IndirectBuffer });
			ValidateShaderParameters(VertexShader, PassParameters->ShaderDrawVSParameters);
			ClearUnusedGraphResources(VertexShader, &PassParameters->ShaderDrawVSParameters, { IndirectBuffer });

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ShaderDrawDebug"),
				PassParameters,
				ERDGPassFlags::Raster,
				[VertexShader, PixelShader, PassParameters, IndirectBuffer, LockedIndirectBuffer, bUseRdgInput](FRHICommandListImmediate& RHICmdListImmediate)
			{
				// Marks the indirect draw parameter as used by the pass, given it's not used directly by any of the shaders.
				if (bUseRdgInput)
				{
					PassParameters->ShaderDrawVSParameters.IndirectBuffer->MarkResourceAsUsed();
				}

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdListImmediate.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI(); // Premultiplied-alpha composition
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, true>::GetRHI();
				GraphicsPSOInit.PrimitiveType = PT_LineList;
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdListImmediate, GraphicsPSOInit);

				SetShaderParameters(RHICmdListImmediate, VertexShader, VertexShader.GetVertexShader(), PassParameters->ShaderDrawVSParameters);
				SetShaderParameters(RHICmdListImmediate, PixelShader, PixelShader.GetPixelShader(), PassParameters->ShaderDrawPSParameters);


				if (bUseRdgInput)
				{
					// Marks the indirect draw parameter as used by the pass, given it's not used directly by any of the shaders.
					FRHIVertexBuffer* IndirectBufferRHI = PassParameters->ShaderDrawVSParameters.IndirectBuffer->GetIndirectRHICallBuffer();
					check(IndirectBufferRHI != nullptr);
					RHICmdListImmediate.DrawPrimitiveIndirect(IndirectBufferRHI, 0);
				}
				else
				{
					RHICmdListImmediate.DrawPrimitiveIndirect(LockedIndirectBuffer, 0);
				}
			});
		};

		FRDGBufferRef DataBuffer = GraphBuilder.RegisterExternalBuffer(View.ShaderDrawData.Buffer);
		FRDGBufferRef IndirectBuffer = GraphBuilder.RegisterExternalBuffer(View.ShaderDrawData.IndirectBuffer);
		{
			RunPass(true, DataBuffer, IndirectBuffer, nullptr, nullptr);
		}

		if (LockedData.bIsLocked)
		{
			FShaderResourceViewRHIRef LockedDataBuffer = LockedData.Buffer.SRV;
			FRHIVertexBuffer* LockedIndirectBuffer = LockedData.IndirectBuffer.Buffer;
			RunPass(false, nullptr, nullptr, LockedDataBuffer, LockedIndirectBuffer);
		}

		if (IsShaderDrawLocked() && !LockedData.bIsLocked)
		{
			{
				const uint32 NumElements = LockedData.Buffer.NumBytes / sizeof(FPackedShaderDrawElement);

				FShaderDrawDebugCopyCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FShaderDrawDebugCopyCS::FBufferType>(0);
				TShaderMapRef<FShaderDrawDebugCopyCS> ComputeShader(View.ShaderMap, PermutationVector);
				FShaderDrawDebugCopyCS::FParameters* Parameters = GraphBuilder.AllocParameters<FShaderDrawDebugCopyCS::FParameters>();
				Parameters->NumElements = NumElements;
				Parameters->InStructuredBuffer = GraphBuilder.CreateSRV(DataBuffer);
				Parameters->OutStructuredBuffer = LockedData.Buffer.UAV;
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ShaderDrawDebugCopy"), ComputeShader, Parameters, FIntVector(FMath::CeilToInt(NumElements / 1024.f), 1, 1));
			}
			{
				FShaderDrawDebugCopyCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FShaderDrawDebugCopyCS::FBufferType>(1);
				TShaderMapRef<FShaderDrawDebugCopyCS> ComputeShader(View.ShaderMap, PermutationVector);

				FShaderDrawDebugCopyCS::FParameters* Parameters = GraphBuilder.AllocParameters<FShaderDrawDebugCopyCS::FParameters>();
				Parameters->InBuffer = GraphBuilder.CreateSRV(IndirectBuffer);
				Parameters->OutBuffer = LockedData.IndirectBuffer.UAV;
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ShaderDrawDebugCopy"), ComputeShader, Parameters, FIntVector(1, 1, 1));
			}
		}
	}

	void EndView(FViewInfo& View)
	{
		if (!IsShaderDrawDebugEnabled(View))
		{
			return;
		}

		if (IsShaderDrawLocked() && !LockedData.bIsLocked)
		{
			LockedData.bIsLocked = true;
		}

		if (!IsShaderDrawLocked() && LockedData.bIsLocked)
		{
			LockedData.Buffer.Release();
			LockedData.IndirectBuffer.Release();
			LockedData.bIsLocked = false;
		}
	}

	void SetParameters(FRDGBuilder& GraphBuilder, const FShaderDrawDebugData& Data, FShaderDrawDebugParameters& OutParameters)
	{
		FRDGBufferRef DataBuffer = GraphBuilder.RegisterExternalBuffer(Data.Buffer);
		FRDGBufferRef IndirectBuffer = GraphBuilder.RegisterExternalBuffer(Data.IndirectBuffer);

		OutParameters.ShaderDrawCursorPos = Data.CursorPosition;
		OutParameters.ShaderDrawMaxElementCount = GetMaxElementCount();
		OutParameters.OutShaderDrawPrimitive = GraphBuilder.CreateUAV(DataBuffer);
		OutParameters.OutputShaderDrawIndirect = GraphBuilder.CreateUAV(IndirectBuffer);
	}
}
