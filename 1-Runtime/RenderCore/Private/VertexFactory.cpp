// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VertexFactory.cpp: Vertex factory implementation
=============================================================================*/

#include "VertexFactory.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/DebugSerializationFlags.h"
#include "PipelineStateCache.h"
#include "ShaderCompilerCore.h"

IMPLEMENT_TYPE_LAYOUT(FVertexFactoryShaderParameters);

uint32 FVertexFactoryType::NumVertexFactories = 0;
bool FVertexFactoryType::bInitializedSerializationHistory = false;

static TLinkedList<FVertexFactoryType*>* GVFTypeList = nullptr;

static TArray<FVertexFactoryType*>& GetSortedMaterialVertexFactoryTypes()
{
	static TArray<FVertexFactoryType*> Types;
	return Types;
}

static TMap<FHashedName, FVertexFactoryType*>& GetVFTypeMap()
{
	static TMap<FHashedName, FVertexFactoryType*> VTTypeMap;
	return VTTypeMap;
}

/**
 * @return The global shader factory list.
 */
TLinkedList<FVertexFactoryType*>*& FVertexFactoryType::GetTypeList()
{
	return GVFTypeList;
}

const TArray<FVertexFactoryType*>& FVertexFactoryType::GetSortedMaterialTypes()
{
	return GetSortedMaterialVertexFactoryTypes();
}

/**
 * Finds a FVertexFactoryType by name.
 */
FVertexFactoryType* FVertexFactoryType::GetVFByName(const FHashedName& VFName)
{
	FVertexFactoryType** Type = GetVFTypeMap().Find(VFName);
	return Type ? *Type : nullptr;
}

void FVertexFactoryType::Initialize(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Cache serialization history for each VF type
		// This history is used to detect when shader serialization changes without a corresponding .usf change
		for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
		{
			FVertexFactoryType* Type = *It;
			GenerateReferencedUniformBuffers(Type->ShaderFilename, Type->Name, ShaderFileToUniformBufferVariables, Type->ReferencedUniformBufferStructsCache);
		}
	}

	bInitializedSerializationHistory = true;
}

void FVertexFactoryType::Uninitialize()
{
	bInitializedSerializationHistory = false;
}

FVertexFactoryType::FVertexFactoryType(
	const TCHAR* InName,
	const TCHAR* InShaderFilename,
	bool bInUsedWithMaterials,
	bool bInSupportsStaticLighting,
	bool bInSupportsDynamicLighting,
	bool bInSupportsPrecisePrevWorldPos,
	bool bInSupportsPositionOnly,
	bool bInSupportsCachingMeshDrawCommands,
	bool bInSupportsPrimitiveIdStream,
	ConstructParametersType InConstructParameters,
	GetParameterTypeLayoutType InGetParameterTypeLayout,
	GetParameterTypeElementShaderBindingsType InGetParameterTypeElementShaderBindings,
	ShouldCacheType InShouldCache,
	ModifyCompilationEnvironmentType InModifyCompilationEnvironment,
	ValidateCompiledResultType InValidateCompiledResult,
	SupportsTessellationShadersType InSupportsTessellationShaders
	):
	Name(InName),
	ShaderFilename(InShaderFilename),
	TypeName(InName),
	HashedName(TypeName),
	bUsedWithMaterials(bInUsedWithMaterials),
	bSupportsStaticLighting(bInSupportsStaticLighting),
	bSupportsDynamicLighting(bInSupportsDynamicLighting),
	bSupportsPrecisePrevWorldPos(bInSupportsPrecisePrevWorldPos),
	bSupportsPositionOnly(bInSupportsPositionOnly),
	bSupportsCachingMeshDrawCommands(bInSupportsCachingMeshDrawCommands),
	bSupportsPrimitiveIdStream(bInSupportsPrimitiveIdStream),
	ConstructParameters(InConstructParameters),
	GetParameterTypeLayout(InGetParameterTypeLayout),
	GetParameterTypeElementShaderBindings(InGetParameterTypeElementShaderBindings),
	ShouldCacheRef(InShouldCache),
	ModifyCompilationEnvironmentRef(InModifyCompilationEnvironment),
	ValidateCompiledResultRef(InValidateCompiledResult),
	SupportsTessellationShadersRef(InSupportsTessellationShaders),
	GlobalListLink(this)
{
	// Make sure the format of the source file path is right.
	check(CheckVirtualShaderFilePath(InShaderFilename));

	checkf(FPaths::GetExtension(InShaderFilename) == TEXT("ush"),
		TEXT("Incorrect virtual shader path extension for vertex factory shader header '%s': Only .ush files should be included."),
		InShaderFilename);

	bCachedUniformBufferStructDeclarations = false;

	// This will trigger if an IMPLEMENT_VERTEX_FACTORY_TYPE was in a module not loaded before InitializeShaderTypes
	// Vertex factory types need to be implemented in modules that are loaded before that
	checkf(!bInitializedSerializationHistory, TEXT("VF type was loaded after engine init, use ELoadingPhase::PostConfigInit on your module to cause it to load earlier."));

	// Add this vertex factory type to the global list.
	GlobalListLink.LinkHead(GetTypeList());
	GetVFTypeMap().Add(HashedName, this);

	if (bUsedWithMaterials)
	{
		TArray<FVertexFactoryType*>& SortedTypes = GetSortedMaterialVertexFactoryTypes();
		const int32 SortedIndex = Algo::LowerBoundBy(SortedTypes, HashedName, [](const FVertexFactoryType* InType) { return InType->GetHashedName(); });
		SortedTypes.Insert(this, SortedIndex);
	}

	++NumVertexFactories;
}

FVertexFactoryType::~FVertexFactoryType()
{
	GlobalListLink.Unlink();
	verify(GetVFTypeMap().Remove(HashedName) == 1);

	if (bUsedWithMaterials)
	{
		TArray<FVertexFactoryType*>& SortedTypes = GetSortedMaterialVertexFactoryTypes();
		const int32 SortedIndex = Algo::BinarySearchBy(SortedTypes, HashedName, [](const FVertexFactoryType* InType) { return InType->GetHashedName(); });
		check(SortedIndex != INDEX_NONE);
		SortedTypes.RemoveAt(SortedIndex);
	}

	check(NumVertexFactories > 0u);
	--NumVertexFactories;
}

/** Calculates a Hash based on this vertex factory type's source code and includes */
const FSHAHash& FVertexFactoryType::GetSourceHash(EShaderPlatform ShaderPlatform) const
{
	return GetShaderFileHash(GetShaderFilename(), ShaderPlatform);
}

FArchive& operator<<(FArchive& Ar,FVertexFactoryType*& TypeRef)
{
	if(Ar.IsSaving())
	{
		FName TypeName = TypeRef ? FName(TypeRef->GetName()) : NAME_None;
		Ar << TypeName;
	}
	else if(Ar.IsLoading())
	{
		FName TypeName = NAME_None;
		Ar << TypeName;
		TypeRef = FindVertexFactoryType(TypeName);
	}
	return Ar;
}

FVertexFactoryType* FindVertexFactoryType(const FHashedName& TypeName)
{
	return FVertexFactoryType::GetVFByName(TypeName);
}

void FVertexFactory::GetStreams(ERHIFeatureLevel::Type InFeatureLevel, EVertexInputStreamType VertexStreamType, FVertexInputStreamArray& OutVertexStreams) const
{
	check(IsInitialized());
	if (VertexStreamType == EVertexInputStreamType::Default)
	{

		bool bSupportsVertexFetch = SupportsManualVertexFetch(InFeatureLevel);

		for (int32 StreamIndex = 0;StreamIndex < Streams.Num();StreamIndex++)
		{
			const FVertexStream& Stream = Streams[StreamIndex];

			if (!(EnumHasAnyFlags(EVertexStreamUsage::ManualFetch, Stream.VertexStreamUsage) && bSupportsVertexFetch))
			{
				if (!Stream.VertexBuffer)
				{
					OutVertexStreams.Add(FVertexInputStream(StreamIndex, 0, nullptr));
				}
				else
				{
					if (EnumHasAnyFlags(EVertexStreamUsage::Overridden, Stream.VertexStreamUsage) && !Stream.VertexBuffer->IsInitialized())
					{
						OutVertexStreams.Add(FVertexInputStream(StreamIndex, 0, nullptr));
					}
					else
					{
						checkf(Stream.VertexBuffer->IsInitialized(), TEXT("Vertex buffer was not initialized! Stream %u, Stride %u, Name %s"), StreamIndex, Stream.Stride, *Stream.VertexBuffer->GetFriendlyName());
						OutVertexStreams.Add(FVertexInputStream(StreamIndex, Stream.Offset, Stream.VertexBuffer->VertexBufferRHI));
					}
				}
			}
		}
	}
	else if (VertexStreamType == EVertexInputStreamType::PositionOnly)
	{
		// Set the predefined vertex streams.
		for (int32 StreamIndex = 0; StreamIndex < PositionStream.Num(); StreamIndex++)
		{
			const FVertexStream& Stream = PositionStream[StreamIndex];
			check(Stream.VertexBuffer->IsInitialized());
			OutVertexStreams.Add(FVertexInputStream(StreamIndex, Stream.Offset, Stream.VertexBuffer->VertexBufferRHI));
		}
	}
	else if (VertexStreamType == EVertexInputStreamType::PositionAndNormalOnly)
	{
		// Set the predefined vertex streams.
		for (int32 StreamIndex = 0; StreamIndex < PositionAndNormalStream.Num(); StreamIndex++)
		{
			const FVertexStream& Stream = PositionAndNormalStream[StreamIndex];
			check(Stream.VertexBuffer->IsInitialized());
			OutVertexStreams.Add(FVertexInputStream(StreamIndex, Stream.Offset, Stream.VertexBuffer->VertexBufferRHI));
		}
	}
	else
	{
		// NOT_IMPLEMENTED
	}
}

void FVertexFactory::OffsetInstanceStreams(uint32 InstanceOffset, EVertexInputStreamType VertexStreamType, FVertexInputStreamArray& VertexStreams) const
{
	const TArrayView<const FVertexStream>& StreamArray = 
		  VertexStreamType == EVertexInputStreamType::PositionOnly ?			MakeArrayView(PositionStream) : 
		( VertexStreamType == EVertexInputStreamType::PositionAndNormalOnly ?	MakeArrayView(PositionAndNormalStream) : 
		/*VertexStreamType == EVertexInputStreamType::Default*/					MakeArrayView(Streams));

	for (int32 StreamIndex = 0; StreamIndex < StreamArray.Num(); StreamIndex++)
	{
		const FVertexStream& Stream = StreamArray[StreamIndex];

		if (EnumHasAnyFlags(EVertexStreamUsage::Instancing, Stream.VertexStreamUsage))
		{
			for (int32 BindingIndex = 0; BindingIndex < VertexStreams.Num(); BindingIndex++)
			{
				if (VertexStreams[BindingIndex].StreamIndex == StreamIndex)
				{
					VertexStreams[BindingIndex].Offset = Stream.Offset + Stream.Stride * InstanceOffset;
				}
			}
		}
	}
}

void FVertexFactory::ReleaseRHI()
{
	Declaration.SafeRelease();
	PositionDeclaration.SafeRelease();
	PositionAndNormalDeclaration.SafeRelease();
	Streams.Empty();
	PositionStream.Empty();
	PositionAndNormalStream.Empty();
}

FVertexElement FVertexFactory::AccessStreamComponent(const FVertexStreamComponent& Component, uint8 AttributeIndex)
{
	FVertexStream VertexStream;
	VertexStream.VertexBuffer = Component.VertexBuffer;
	VertexStream.Stride = Component.Stride;
	VertexStream.Offset = Component.StreamOffset;
	VertexStream.VertexStreamUsage = Component.VertexStreamUsage;

	return FVertexElement(Streams.AddUnique(VertexStream),Component.Offset,Component.Type,AttributeIndex,VertexStream.Stride, EnumHasAnyFlags(EVertexStreamUsage::Instancing, VertexStream.VertexStreamUsage));
}

FVertexElement FVertexFactory::AccessStreamComponent(const FVertexStreamComponent& Component,uint8 AttributeIndex, EVertexInputStreamType InputStreamType)
{
	FVertexStream VertexStream;
	VertexStream.VertexBuffer = Component.VertexBuffer;
	VertexStream.Stride = Component.Stride;
	VertexStream.Offset = Component.StreamOffset;
	VertexStream.VertexStreamUsage = Component.VertexStreamUsage;

	if (InputStreamType == EVertexInputStreamType::PositionOnly)
		return FVertexElement(PositionStream.AddUnique(VertexStream), Component.Offset, Component.Type, AttributeIndex, VertexStream.Stride, EnumHasAnyFlags(EVertexStreamUsage::Instancing, VertexStream.VertexStreamUsage));
	else if (InputStreamType == EVertexInputStreamType::PositionAndNormalOnly)
		return FVertexElement(PositionAndNormalStream.AddUnique(VertexStream), Component.Offset, Component.Type, AttributeIndex, VertexStream.Stride, EnumHasAnyFlags(EVertexStreamUsage::Instancing, VertexStream.VertexStreamUsage));
	else /* (InputStreamType == EVertexInputStreamType::Default) */
		return FVertexElement(Streams.AddUnique(VertexStream), Component.Offset, Component.Type, AttributeIndex, VertexStream.Stride, EnumHasAnyFlags(EVertexStreamUsage::Instancing, VertexStream.VertexStreamUsage));
}

void FVertexFactory::InitDeclaration(const FVertexDeclarationElementList& Elements, EVertexInputStreamType StreamType)
{
	
	if (StreamType == EVertexInputStreamType::PositionOnly)
	{
		PositionDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	else if (StreamType == EVertexInputStreamType::PositionAndNormalOnly)
	{
		PositionAndNormalDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	else // (StreamType == EVertexInputStreamType::Default)
	{
		// Create the vertex declaration for rendering the factory normally.
		Declaration = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
}

void FPrimitiveIdDummyBuffer::InitRHI() 
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo;
		
	void* LockedData = nullptr;
	VertexBufferRHI = RHICreateAndLockVertexBuffer(sizeof(uint32), BUF_Static | BUF_ShaderResource, CreateInfo, LockedData);
	uint32* Vertices = (uint32*)LockedData;
	Vertices[0] = 0;
	RHIUnlockVertexBuffer(VertexBufferRHI);
	VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R32_UINT);
}

TGlobalResource<FPrimitiveIdDummyBuffer> GPrimitiveIdDummy;