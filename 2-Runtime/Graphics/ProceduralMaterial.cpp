#include "UnityPrefix.h"
#include "ProceduralMaterial.h"
#include "SubstanceArchive.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Input/TimeManager.h"
#include "SubstanceSystem.h"
#include "Runtime/Graphics/Image.h"

#if ENABLE_SUBSTANCE
#include "Runtime/BaseClasses/IsPlaying.h"
#endif

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/SubstanceImporter.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#endif

size_t GetProceduralMemoryBudget(ProceduralCacheSize budget)
{
	switch(budget)
	{
		case ProceduralCacheSize_NoLimit:
			return 0;
		case ProceduralCacheSize_Heavy:
			return 512 * 1024 * 1024;
		case ProceduralCacheSize_Medium:
			return 256 * 1024 * 1024;
		case ProceduralCacheSize_Tiny:
			return 128 * 1024 * 1024;
		case ProceduralCacheSize_None:
		default:
			return 1;
	}
}

Mutex ProceduralMaterial::m_InputMutex;

IMPLEMENT_CLASS( ProceduralMaterial )
IMPLEMENT_OBJECT_SERIALIZE( ProceduralMaterial )

ProceduralMaterial::ProceduralMaterial( MemLabelId label, ObjectCreationMode mode ) :
	Super( label, mode ),
	m_SubstanceData( NULL ),
	m_Flags( 0 ),
	m_LoadingBehavior( ProceduralLoadingBehavior_Generate ),
	m_Width( 9 ),
	m_Height( 9 ),
#if UNITY_EDITOR
	m_isAlreadyLoadedInCurrentScene( false ),
#endif
	m_AnimationUpdateRate( 42 ), // 24fps
	m_AnimationTime( 0.0f ),
	m_PingedPackage( NULL ),
	m_PrototypeName( "" )
{
#if ENABLE_SUBSTANCE
	integrationTimeStamp = GetSubstanceSystem().integrationTimeStamp;
#endif
}

ProceduralMaterial::~ProceduralMaterial()
{
#if ENABLE_SUBSTANCE
	
	/////@TODO: This is an incredible hack. Waiting for another process to complete in the destructor is a big no no, especially when it involves a Thread::Sleep ()...
	UnlockObjectCreation();
	GetSubstanceSystem().NotifySubstanceDestruction(this);
	LockObjectCreation();
	
	Clean();

	// Delete texture inputs (TODO: try to remove std::vector<Image*>)
	for (std::vector<TextureInput>::iterator it=m_TextureInputs.begin();it!=m_TextureInputs.end();++it)
	{
		delete it->inputParameters;
		delete it->image;

		if (it->buffer)
		{
			UNITY_FREE(kMemSubstance, it->buffer);
		}
	}
#endif
}

void ProceduralMaterial::Clean()
{
#if ENABLE_SUBSTANCE
	if (m_SubstanceData!=NULL)
	{
		if (m_SubstanceData->instanceCount==1)
		{
			substanceHandleRelease(m_SubstanceData->substanceHandle);
			// Clones do not delete their SBSBIN data, this is done in the destructor of the SubstanceArchive
			// the clone was created from.
			if (!IsFlagEnabled(Flag_Clone))
			{
				UNITY_FREE( kMemSubstance, m_SubstanceData->substanceData );
			}
			UNITY_FREE( kMemSubstance, m_SubstanceData );
		}
		else
		{
			--m_SubstanceData->instanceCount;
		}
	}
	m_SubstanceData = NULL;
	// Awake inputs
	for (SubstanceInputs::iterator i=m_Inputs.begin();i != m_Inputs.end();i++)
	{
		i->EnableFlag(SubstanceInput::Flag_Awake, true);
	}
#endif
}

ProceduralMaterial* ProceduralMaterial::Clone()
{
	ProceduralMaterial* clone = CreateObjectFromCode<ProceduralMaterial>();
	clone->m_SubstancePackage = m_SubstancePackage;
	clone->m_PrototypeName = m_PrototypeName;
	clone->m_Width = m_Width;
	clone->m_Height = m_Height;
	clone->m_Textures = m_Textures;
	// Rename cloned textures
	for (int idx=0 ; idx<m_Textures.size() ; ++idx)
	{
		clone->m_Textures[idx]->SetName(m_Textures[idx]->GetName());
	}
	clone->m_AnimationUpdateRate = m_AnimationUpdateRate;
	clone->m_Inputs = m_Inputs;
	clone->m_Flags = ((m_Flags | Flag_Clone | Flag_AwakeClone) & (~Flag_ConstSize));
	clone->m_LoadingBehavior = m_LoadingBehavior;
	clone->AwakeDependencies(false);
	return clone;
}

void ProceduralMaterial::RebuildClone()
{
	EnableFlag(Flag_AwakeClone, false);

	// Clone textures
#if ENABLE_SUBSTANCE
	if (!IsWorldPlaying() || m_LoadingBehavior!=ProceduralLoadingBehavior_BakeAndDiscard)
	{
		for (Textures::iterator it=m_Textures.begin();it!=m_Textures.end();++it)
		{
			*it = (*it)->Clone(this);
		}

		// Awake inputs
		for (SubstanceInputs::iterator i=m_Inputs.begin();i != m_Inputs.end();i++)
		{
			i->EnableFlag(SubstanceInput::Flag_Awake, true);
		}

		AwakeDependencies(false);

		// Force generation if required
		if (m_LoadingBehavior==ProceduralLoadingBehavior_None)
			EnableFlag(ProceduralMaterial::Flag_ForceGenerate);

		GetSubstanceSystem().QueueLoading(this);
	}
#endif
}

#if UNITY_EDITOR
void ProceduralMaterial::Init( SubstanceArchive& substancePackage, const UnityStr& prototypeName, const SubstanceInputs& inputs, const Textures& textures )
{
	Assert(m_SubstancePackage.GetInstanceID () == 0);

	m_SubstancePackage = &substancePackage;
	m_PrototypeName = prototypeName;
	m_Inputs = inputs;
	m_Textures = textures;

	EnableFlag(Flag_ConstSize);

	// Update Flag_Animated
	EnableFlag(Flag_Animated, HasSubstanceProperty("$time"));
}

const char* ProceduralMaterial::GetSubstancePackageName()
{
	if (m_PingedPackage)
	{
		return m_PingedPackage->GetName();
	}
	else
	{
		return m_SubstancePackage->GetName();
	}
}

#endif

SubstanceHandle* ProceduralMaterial::GetSubstanceHandle()
{
	if (m_SubstanceData!=NULL)
		return m_SubstanceData->substanceHandle;
	return NULL;
}

void ProceduralMaterial::SetSize(int width, int height)
{
    m_Width = width;
    m_Height = height;

    Mutex::AutoLock locker(m_InputMutex);
	SubstanceInput* input = FindSubstanceInput("$outputsize");
	if (input!=NULL)
	{
        input->value.scalar[0] = (float)m_Width;
        input->value.scalar[1] = (float)m_Height;
	}
}

void ProceduralMaterial::AwakeFromLoadThreaded()
{
	Super::AwakeFromLoadThreaded();
	AwakeDependencies(true);
#if ENABLE_SUBSTANCE
#if UNITY_EDITOR
	EnableFlag(Flag_Awake);
#endif
	// Neither Baked (keep or discard) nor DoNothing substances must be generated at this time
	ProceduralLoadingBehavior behavior = GetLoadingBehavior();
	if (behavior != ProceduralLoadingBehavior_BakeAndKeep &&
	    behavior != ProceduralLoadingBehavior_BakeAndDiscard &&
	    behavior != ProceduralLoadingBehavior_None)
	{
		GetSubstanceSystem().QueueLoading(this);
	}
#endif
}

void ProceduralMaterial::AwakeFromLoad( AwakeFromLoadMode awakeMode )
{
	Super::AwakeFromLoad( awakeMode );

	if ((awakeMode & kDidLoadThreaded)==0)
	{
		AwakeDependencies(false);

#if ENABLE_SUBSTANCE

#if UNITY_EDITOR
		EnableFlag(Flag_Awake);
		if (awakeMode!=kInstantiateOrCreateFromCodeAwakeFromLoad
		    && SubstanceImporter::OnLoadSubstance(*this)
		    && !(IsWorldPlaying() && m_LoadingBehavior==ProceduralLoadingBehavior_None))
#else
		// Don't link and render the DoNothing substances when loading them
		// This is done when calling RebuildTextures instead
		if (m_LoadingBehavior!=ProceduralLoadingBehavior_None)
#endif
		{
			GetSubstanceSystem().QueueSubstance(this);
		}

#endif
	}

#if ENABLE_SUBSTANCE
	GetSubstanceSystem().NotifySubstanceCreation(this);
#endif
}

#if ENABLE_SUBSTANCE
void ProceduralMaterial::ApplyInputs (bool& it_has_changed, bool asHint, std::set<unsigned int>& modifiedOutputsUID)
{
	int textureInputIndex(0);

	// Handle input alteration
	for (SubstanceInputs::iterator i=m_Inputs.begin();i != m_Inputs.end();i++)
	{
		SubstanceInput& input = *i;

		// Skip const inputs at runtime
#if !UNITY_EDITOR
		if (IsFlagEnabled(Flag_ConstSize)
			&& (input.name=="$outputsize" || input.name=="$randomseed"))
		{
			continue;
		}
#endif

		// Check texture instance hasn't changed, in the editor in case of texture changes
#if UNITY_EDITOR
		if (!input.IsFlagEnabled(SubstanceInput::Flag_Modified) && (input.internalType==Substance_IType_Image)
			&& textureInputIndex<m_TextureInputs.size())
		{
			Texture2D* checkTexture = dynamic_pptr_cast<Texture2D*>(
				InstanceIDToObjectThreadSafe(input.value.texture.GetInstanceID()));
			if (m_TextureInputs[textureInputIndex].texture!=checkTexture)
				m_TextureInputs[textureInputIndex].texture = checkTexture;
		}
#endif

		if (asHint)
		{
			// Push hint if needed
			if (input.IsFlagEnabled(SubstanceInput::Flag_Modified) || input.IsFlagEnabled(SubstanceInput::Flag_Cached))
			{
				if (!input.IsFlagEnabled(SubstanceInput::Flag_SkipHint))
				{
					if (substanceHandlePushSetInput( m_SubstanceData->substanceHandle, Substance_PushOpt_HintOnly, input.internalIndex, input.internalType, 0, 0 )!=0)
						ErrorStringObject("Failed to apply substance input as hint", this);
				}
				it_has_changed = true;
				input.EnableFlag(SubstanceInput::Flag_Modified, false);
			}

			if (input.IsFlagEnabled(SubstanceInput::Flag_Awake))
			{
				it_has_changed = true;
				input.EnableFlag(SubstanceInput::Flag_Awake, false);
			}
		}
		else if (input.IsFlagEnabled(SubstanceInput::Flag_Modified) || input.IsFlagEnabled(SubstanceInput::Flag_Awake))
		{
			int error = 0;

			// Apply Float values
			if (IsSubstanceAnyFloatType(input.internalType))
			{
				error = substanceHandlePushSetInput( m_SubstanceData->substanceHandle, Substance_PushOpt_NotAHint, input.internalIndex, input.internalType, input.value.scalar, 0 );
			}
			// Apply integer values
			else if (IsSubstanceAnyIntType(input.internalType))
			{
				int intValue[4];
				intValue[0] = (int)input.value.scalar[0];
				intValue[1] = (int)input.value.scalar[1];
				intValue[2] = (int)input.value.scalar[2];
				intValue[3] = (int)input.value.scalar[3];
				error = substanceHandlePushSetInput( m_SubstanceData->substanceHandle, Substance_PushOpt_NotAHint, input.internalIndex, input.internalType, intValue, 0 );
			}
			// Apply image values
			else if (input.internalType == Substance_IType_Image)
			{
				if (textureInputIndex>=m_TextureInputs.size())
				{
					ErrorStringObject("failed to apply substance input image", this);
				}
				else
				{
					error = substanceHandlePushSetInput( m_SubstanceData->substanceHandle, Substance_PushOpt_NotAHint, input.internalIndex, input.internalType, m_TextureInputs[textureInputIndex].inputParameters, 0 );
				}
			}
			else
			{
				ErrorStringObject("unsupported substance input type", this);
			}
			if (error != 0)
				ErrorStringObject("Failed to apply substance input", this);

			// Add modified output
			modifiedOutputsUID.insert(input.alteredTexturesUID.begin(), input.alteredTexturesUID.end());
		}

		// Keep the texture input index up to date
		if (input.internalType == Substance_IType_Image)
		{
			++textureInputIndex;
		}
	}
}

void ProceduralMaterial::ApplyOutputs (bool& it_has_changed, bool asHint, std::set<unsigned int>& modifiedOutputsUID, const std::set<unsigned int>& cachedTextureIDs)
{
	// Add the invalid outputs
	if (!asHint)
	{
		for (PingedTextures::iterator it=m_PingedTextures.begin();it!=m_PingedTextures.end();++it)
		{
			if (*it!=NULL && !(*it)->IsValid())
			{
				modifiedOutputsUID.insert((*it)->GetSubstanceBaseTextureUID());
				it_has_changed = true;
			}
		}
	}

	// Push outputs
	unsigned int flags = Substance_OutOpt_TextureId | Substance_OutOpt_CopyNeeded | (asHint?Substance_PushOpt_HintOnly:0);
	GetSubstanceSystem ().processedTextures.clear();
	std::vector<unsigned int> textureIDs;
	for (PingedTextures::iterator i=m_PingedTextures.begin();i!=m_PingedTextures.end();++i)
	{
		ProceduralTexture* texture = *i;
		if (texture!=NULL && modifiedOutputsUID.find(texture->GetSubstanceBaseTextureUID())!=modifiedOutputsUID.end()
			&& cachedTextureIDs.find(texture->GetSubstanceBaseTextureUID())==cachedTextureIDs.end())
        {
			GetSubstanceSystem ().processedTextures[texture->GetSubstanceTextureUID()] = texture;
			textureIDs.push_back(texture->GetSubstanceTextureUID());
		}
	}

	if (textureIDs.size()==0)
	{
		modifiedOutputsUID.clear();
		it_has_changed = false;
	}

	if (textureIDs.size()>0 && substanceHandlePushOutputs( m_SubstanceData->substanceHandle, flags, &textureIDs[0], textureIDs.size(), 0 )!=0)
	{
		ErrorStringObject("Failed to apply substance texture outputs", this);
	}
}
#endif

void ProceduralMaterial::RebuildTextures()
{
	if (IsFlagEnabled(Flag_AwakeClone))
	{
		RebuildClone();
	}
#if ENABLE_SUBSTANCE
	else if (!IsWorldPlaying() || m_LoadingBehavior!=ProceduralLoadingBehavior_BakeAndDiscard)
	{
		GetSubstanceSystem().QueueSubstance(this);
	}
#endif
}

void ProceduralMaterial::RebuildTexturesImmediately()
{
	ASSERT_RUNNING_ON_MAIN_THREAD;
	RebuildTextures();
#if ENABLE_SUBSTANCE
	GetSubstanceSystem().WaitFinished(this);
#endif
}

void ProceduralMaterial::ReloadAll (bool unload, bool load)
{
#if ENABLE_SUBSTANCE
	std::vector<SInt32> objects;
	Object::FindAllDerivedObjects (ClassID (ProceduralMaterial), &objects);
	std::sort(objects.begin(), objects.end());

	if (objects.empty())
		return;

	GetSubstanceSystem().WaitFinished();
	for (int i=0;i<objects.size ();i++)
	{
		ProceduralMaterial& mat = *PPtr<ProceduralMaterial> (objects[i]);
		Textures& textures = mat.GetTextures();
		for (Textures::iterator it=textures.begin() ; it!=textures.end() ; ++it)
		{
#if UNITY_EDITOR
			(*it)->EnableFlag(ProceduralTexture::Flag_Cached, false);
#endif
			(*it)->Invalidate();
		}
#if UNITY_EDITOR
		mat.EnableFlag(ProceduralMaterial::Flag_Awake);
#endif
		if (mat.m_LoadingBehavior==ProceduralLoadingBehavior_BakeAndDiscard || (unload && !load))
		{
			for (Textures::iterator it=textures.begin() ; it!=textures.end() ; ++it)
			{
				if (unload)
					(*it)->UnloadFromGfxDevice(false);
				if (load)
					(*it)->UploadToGfxDevice();
			}
		}
		else
		{
			mat.RebuildTextures ();
		}
	}
	SubstanceSystem::Context context(ProceduralProcessorUsage_All);
	GetSubstanceSystem().WaitFinished();
#endif
}

template<class _class_>
void AwakeProceduralObject(PPtr<_class_>& pptr, _class_*& object, bool awakeThreaded)
{
    if (awakeThreaded)
    {
    #if SUPPORT_THREADS
        Assert (!Thread::CurrentThreadIsMainThread());
    #endif

        // Awake the object
        object = dynamic_pptr_cast<_class_*> (InstanceIDToObjectThreadSafe(pptr.GetInstanceID()));
    }
    else
    {
    #if SUPPORT_THREADS
        Assert (Thread::CurrentThreadIsMainThread());
    #endif

        // We can safely load it synchroneously
		object = pptr;
	}

    // Clear the PPtr if the object no more exist (removed/deprecated)
	if (object==NULL)
		pptr.SetInstanceID(0);
}

void ProceduralMaterial::AwakeDependencies(bool awakeThreaded)
{
	// Simplest validity check
	if (m_Textures.size()==0)
    {
        EnableFlag(Flag_Broken);
		return;
    }

#if ENABLE_SUBSTANCE
    // Awake package
    AwakeProceduralObject(m_SubstancePackage, m_PingedPackage, awakeThreaded);
    if (m_PingedPackage==NULL && m_LoadingBehavior!=ProceduralLoadingBehavior_BakeAndDiscard)
    {
        EnableFlag(Flag_Broken);
        return;
    }

    // Awake texture inputs
    unsigned int input_count(0);
    for (SubstanceInputs::iterator it=m_Inputs.begin();it!=m_Inputs.end();++it)
    {
	    if (it->internalType==Substance_IType_Image)
	    {
		    if (input_count>=m_TextureInputs.size())
		    {
			    m_TextureInputs.push_back( TextureInput() );
			    m_TextureInputs.back().inputParameters = new SubstanceTextureInput();
			    memset(m_TextureInputs.back().inputParameters, 0, sizeof(SubstanceTextureInput));
			    m_TextureInputs.back().image = new Image();
			    m_TextureInputs.back().buffer = NULL;
		    }

            AwakeProceduralObject(it->value.texture, m_TextureInputs[input_count].texture, awakeThreaded);
            ++input_count;
	    }
    }
#endif

	// Awake textures
	if (m_PingedTextures.size()!=m_Textures.size())
	{
		int size = m_PingedTextures.size();
		m_PingedTextures.resize(m_Textures.size());
		if (size<m_PingedTextures.size())
			memset(&m_PingedTextures[size], 0, sizeof(ProceduralTexture*)*(m_Textures.size()-size));
	}

	int texture_index(0);
	for (ProceduralMaterial::Textures::iterator i=m_Textures.begin();i!=m_Textures.end();++i,++texture_index)
	{
        AwakeProceduralObject(*i, m_PingedTextures[texture_index], awakeThreaded);
		if (m_PingedTextures[texture_index]==NULL)
        {
            EnableFlag(Flag_Broken);
			return;
        }
    	m_PingedTextures[texture_index]->SetOwner(this);
	}
}

#if ENABLE_SUBSTANCE

bool ProceduralMaterial::ProcessTexturesThreaded(const std::map<ProceduralTexture*, SubstanceTexture>& textures)
{
    if (IsFlagEnabled(Flag_Broken))
        return true;

	GetSubstanceSystem().UpdateMemoryBudget();

	std::set<unsigned int> cachedTextureIDs;
#if ENABLE_CACHING && !UNITY_EDITOR
	if (PreProcess(cachedTextureIDs))
	{
		return true;
	}
#else
	// Force rebuild if it's cached and no input is modified
	InvalidateIfCachedOrInvalidTextures();
#endif

	// Apply substance parameters (this is the exact ordering the engine needs)
	std::set<unsigned int> modifiedOutputsUID;
	bool it_has_changed(false);
	// Apply inputs values
	ApplyInputs (it_has_changed, false, modifiedOutputsUID);

	// For readable textures, force the rebuild by pushing all output UIDs,
	// even if no input was changed (Bnecessary for a clean workflow for case 538383 / GetPixels32())
	if (IsFlagEnabled(Flag_Readable))
	{
		for (PingedTextures::iterator i=m_PingedTextures.begin() ; i!=m_PingedTextures.end() ; ++i)
		{
			ProceduralTexture* texture = *i;
			modifiedOutputsUID.insert(texture->GetSubstanceBaseTextureUID());
		}
	}

	// Apply outputs uid
	ApplyOutputs(it_has_changed, false, modifiedOutputsUID, cachedTextureIDs);
	// Apply input hints
	ApplyInputs (it_has_changed, true, modifiedOutputsUID);
	// Apply outputs hints
	ApplyOutputs(it_has_changed, true, modifiedOutputsUID, cachedTextureIDs);

	if (!IsFlagEnabled(Flag_Readable) && !it_has_changed)
	{
		// Flush render list
		substanceHandleFlush( m_SubstanceData->substanceHandle );
		return false;
	}

	if (substanceHandleStart( m_SubstanceData->substanceHandle, Substance_Sync_Synchronous )!=0)
		ErrorStringObject("Failed to start substance computation", this);

	// Flush render list
	substanceHandleFlush( m_SubstanceData->substanceHandle );

#if ENABLE_CACHING && !UNITY_EDITOR
	PostProcess(textures, cachedTextureIDs);
#endif
	return true;
}

void ProceduralMaterial::ApplyTextureInput (int substanceInputIndex, const SubstanceTextureInput& requiredTextureInput)
{
	// Find which input needs update
	bool found(false);
	int textureInputIndex(0);
	SubstanceInput* input;
	for (SubstanceInputs::iterator i=m_Inputs.begin();i != m_Inputs.end();i++)
	{
		input = &*i;
		if (input->internalIndex==substanceInputIndex)
		{
			found = true;
			break;
		}
		if (input->internalType==Substance_IType_Image)
		{
			++textureInputIndex;
		}
	}
	if (!found || textureInputIndex>=m_TextureInputs.size())
	{
		ErrorStringObject("Failed to push Substance texture input", this);
		return;
	}

	// Check format
	TextureFormat format;
	bool need16bitsConvert(false);
	if (requiredTextureInput.pixelFormat==Substance_PF_RGBA)
		format = kTexFormatRGBA32;
	else if (requiredTextureInput.pixelFormat==Substance_PF_RGB)
		format = kTexFormatRGB24;
	else if (requiredTextureInput.pixelFormat==(Substance_PF_16b | Substance_PF_RGBA))
	{
		format = kTexFormatRGBA32;
		need16bitsConvert = true;
	}
	else if (requiredTextureInput.pixelFormat==(Substance_PF_16b | Substance_PF_RGB))
	{
		format = kTexFormatRGB24;
		need16bitsConvert = true;
	}
	else if (requiredTextureInput.pixelFormat==Substance_PF_DXT1)
		format = kTexFormatDXT1;
	else if (requiredTextureInput.pixelFormat==Substance_PF_DXT3)
		format = kTexFormatDXT3;
	else if (requiredTextureInput.pixelFormat==Substance_PF_DXT5)
		format = kTexFormatDXT5;
	else if (requiredTextureInput.pixelFormat==Substance_PF_L)
		format = kTexFormatAlpha8;
	else
	{
		ErrorStringObject("Failed to push Substance texture input : unsupported format", this);
		return;
	}

	if (m_TextureInputs.size()<=textureInputIndex)
	{
		ErrorStringObject("Failed to push Substance texture input : unexpected error", this);
		return;
	}

	// Initialize the texture input if required
	TextureInput& textureInput = m_TextureInputs[textureInputIndex];
	if (textureInput.inputParameters->level0Width!=requiredTextureInput.level0Width
		|| textureInput.inputParameters->level0Height!=requiredTextureInput.level0Height
		|| textureInput.inputParameters->pixelFormat!=requiredTextureInput.pixelFormat)
	{
		// Fill format description
		memcpy(textureInput.inputParameters, &requiredTextureInput, sizeof(SubstanceTextureInput));
		textureInput.inputParameters->mTexture.level0Width = textureInput.inputParameters->level0Width;
		textureInput.inputParameters->mTexture.level0Height = textureInput.inputParameters->level0Height;
		textureInput.inputParameters->mTexture.mipmapCount = textureInput.inputParameters->mipmapCount;
		textureInput.inputParameters->mTexture.pixelFormat = textureInput.inputParameters->pixelFormat;
		textureInput.inputParameters->mTexture.channelsOrder = 0;
		textureInput.image->SetImage(textureInput.inputParameters->level0Width, textureInput.inputParameters->level0Height, format, true);

		if (textureInput.buffer!=NULL)
		{
			UNITY_FREE(kMemSubstance, textureInput.buffer);
			textureInput.buffer = NULL;
		}

		if (need16bitsConvert)
		{
			size_t pitch = ((requiredTextureInput.pixelFormat | Substance_PF_RGBA)?4:3)*sizeof(unsigned short);
			textureInput.buffer = UNITY_MALLOC_ALIGNED_NULL(kMemSubstance, pitch*textureInput.inputParameters->level0Width*textureInput.inputParameters->level0Height, 16);
			if (!textureInput.buffer)
			{
				ErrorString("Could not allocate memory for textureInput.buffer (ApplyTextureInput)");
				return;
			}
			textureInput.inputParameters->mTexture.buffer = textureInput.buffer;
		}
		else
		{
			textureInput.inputParameters->mTexture.buffer = textureInput.image->GetImageData();
		}
	}

	Texture2D* texture = textureInput.texture;
	if (texture==NULL)
	{
		// Default to white texture
		textureInput.image->ClearImage(ColorRGBAf(1.0f, 1.0f, 1.0f, 1.0f));
	}
	else
	{
		// Try to retrieve current texture
		if (texture->ExtractImage(textureInput.image))
		{
			textureInput.image->FlipImageY();
		}
		else
		{
			ErrorStringObject("Incorrect ProceduralMaterial input", this);
			if (texture->GetRawImageData()==NULL)
			{
				ErrorStringObject("ProceduralMaterial: Unexpected error (Texture input is not in RAM), try a reimport", texture);
			}
			else
			{
				ErrorStringObject("ProceduralMaterial: Texture input is compressed in undecompressable format, you should switch it to RAW, then reimport the material", texture);
			}

			textureInput.image->ClearImage(ColorRGBAf(1.0f, 0.0f, 0.0f, 1.0f));
		}
	}

	// Actually we can't force the input texture format using the Substance API,
	// so here it requires 8bits -> 16bits conversion.
	if (need16bitsConvert)
	{
		size_t pitch = (requiredTextureInput.pixelFormat | Substance_PF_RGBA)?4:3;
		unsigned short * output = (unsigned short*)textureInput.buffer;
		unsigned char * input = textureInput.image->GetImageData();
		unsigned char * inputEnd = input + pitch*textureInput.image->GetWidth()*textureInput.image->GetHeight();
		while(input!=inputEnd)
		{
			*(output++) = (unsigned short)*(input++)*257;
		}
	}
}
#endif

void ProceduralMaterial::UpdateAnimation(float time)
{
	if (m_AnimationUpdateRate>0)
	{
		if (time<m_AnimationTime
			|| time>m_AnimationTime+m_AnimationUpdateRate/1000.0f)
		{
			m_AnimationTime = time;
			SetSubstanceFloat("$time", time);
			RebuildTextures();
		}
	}
}

std::vector<std::string> ProceduralMaterial::GetSubstanceProperties() const
{
	std::vector<std::string> properties;
	for (SubstanceInputs::const_iterator i=m_Inputs.begin();i!=m_Inputs.end();++i)
	{
		properties.push_back(i->name);
	}
	return properties;
}

bool ProceduralMaterial::HasSubstanceProperty( const std::string& inputName ) const
{
	Mutex::AutoLock locker(m_InputMutex);
	return FindSubstanceInput(inputName)!=NULL;
}

bool ProceduralMaterial::GetSubstanceBoolean( const std::string& inputName ) const
{
	Mutex::AutoLock locker(m_InputMutex);
	const SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL)
	{
		return input->value.scalar[0]>0.5f;
	}
	return false;
}

void ProceduralMaterial::SetSubstanceBoolean( const std::string& inputName, bool value )
{
	SetSubstanceFloat( inputName, value?1.0f:0.0f );
}

float ProceduralMaterial::GetSubstanceFloat( const std::string& inputName ) const
{
	Mutex::AutoLock locker(m_InputMutex);
	const SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL)
		return input->value.scalar[0];
	return 0.0F;
}

void ProceduralMaterial::SetSubstanceFloat( const std::string& inputName, float value )
{
	SetSubstanceVector(inputName, Vector4f(value, 0.0f, 0.0f, 0.0f));
}

Vector4f ProceduralMaterial::GetSubstanceVector( const std::string& inputName ) const
{
	Mutex::AutoLock locker(m_InputMutex);
	const SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL)
	{
		Vector4f value;
		value.Set(input->value.scalar);
		return value;
	}
	return Vector4f(0.0F, 0.0F, 0.0F, 0.0F);
}

void ProceduralMaterial::SetSubstanceVector( const std::string& inputName, const Vector4f& value )
{
	Mutex::AutoLock locker(m_InputMutex);
	const SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL)
	{
		SubstanceValue inputValue;
		memcpy(inputValue.scalar, value.GetPtr(), sizeof(float)*4);
		SetDirty();
#if ENABLE_SUBSTANCE
		GetSubstanceSystem().QueueInput(this, inputName, inputValue);
#endif
	}
}

ColorRGBAf ProceduralMaterial::GetSubstanceColor( const std::string& inputName ) const
{
	Mutex::AutoLock locker(m_InputMutex);
	const SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL)
	{
		ColorRGBAf value;
		memcpy(value.GetPtr(), input->value.scalar, sizeof(float)*4);
		return value;
	}

	return ColorRGBAf(0.0F, 0.0F, 0.0F, 0.0F);
}

void ProceduralMaterial::SetSubstanceColor( const std::string& inputName, const ColorRGBAf& value )
{
	SetSubstanceVector(inputName, Vector4f(value.r, value.g, value.b, value.a));
}

int ProceduralMaterial::GetSubstanceEnum( const string& inputName )
{
	Mutex::AutoLock locker(m_InputMutex);
	SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL)
	{
		int index(0);
		for (std::vector<SubstanceEnumItem>::iterator it=input->enumValues.begin();it!=input->enumValues.end();++it)
		{
			if ((int)input->value.scalar[0]==it->value)
			{
				return index;
			}

			++index;
		}
	}
	return -1;
}

void ProceduralMaterial::SetSubstanceEnum( const string& inputName, int value )
{
	Mutex::AutoLock locker(m_InputMutex);
	SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL && value>=0 && value<input->enumValues.size())
	{
		SubstanceEnumItem& item = input->enumValues[value];
		SubstanceValue inputValue;
		inputValue.scalar[0] = (float)item.value;
		SetDirty();
#if ENABLE_SUBSTANCE
		GetSubstanceSystem().QueueInput(this, inputName, inputValue);
#endif
	}
}

Texture2D* ProceduralMaterial::GetSubstanceTexture( const string& inputName ) const
{
	Mutex::AutoLock locker(m_InputMutex);
	const SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL && input->internalType == Substance_IType_Image)
		return input->value.texture;
	return NULL;
}

void ProceduralMaterial::SetSubstanceTexture( const string& inputName, Texture2D* value )
{
	Mutex::AutoLock locker(m_InputMutex);
	const SubstanceInput* input(NULL);
	int texture_index(0);
	for (SubstanceInputs::iterator i=m_Inputs.begin();i!=m_Inputs.end();++i)
	{
		if (i->name==inputName)
		{
			input = &*i;
			break;
		}
		if (i->type==ProceduralPropertyType_Texture)
			++texture_index;
	}

	if (input!=NULL && input->type==ProceduralPropertyType_Texture
		&& texture_index<m_TextureInputs.size())
	{
		m_TextureInputs[texture_index].texture = value;
		SubstanceValue inputValue;
		inputValue.texture = value;
		SetDirty();
#if ENABLE_SUBSTANCE
		GetSubstanceSystem().QueueInput(this, inputName, inputValue);
#endif
	}
}

#if ENABLE_SUBSTANCE
void ProceduralMaterial::Callback_SetSubstanceInput( const string& inputName, SubstanceValue& inputValue)
{
	Mutex::AutoLock locker(m_InputMutex);
	SubstanceInput* input = FindSubstanceInput(inputName);

	//AreSubstanceInputValuesEqual(input->internalType, input->value, inputValue))
	// Its up to the user to set value if it hasn't changed, he may want to really set the value
	// even if it hasn't changed, to cache it for instance.
	if (input==NULL)
		return;

	ClampSubstanceInputValues(*input, inputValue);

	if (input->type==ProceduralPropertyType_Texture)
	{
		input->value.texture = inputValue.texture;
	}
	else
	{
		memcpy(input->value.scalar, inputValue.scalar,
			sizeof(float)*GetRequiredInputComponentCount(input->internalType));
	}

	input->EnableFlag(SubstanceInput::Flag_Modified);

#if !UNITY_EDITOR
	if (IsFlagEnabled(Flag_ConstSize) && (input->name=="$outputsize" || input->name=="$randomseed"))
	{
		EnableFlag(Flag_ConstSize, false);
		Clean();

		// Initialize fresh substance data
		std::vector<ProceduralMaterial*> materials;
		materials.push_back(this);
		PackSubstances(materials);
	}
#endif
}
#endif

const SubstanceInput* ProceduralMaterial::FindSubstanceInput( const string& inputName ) const
{
	for (SubstanceInputs::const_iterator i=m_Inputs.begin();i!=m_Inputs.end();++i)
	{
		if (i->name==inputName)
			return &*i;
	}
	return NULL;
}

SubstanceInput* ProceduralMaterial::FindSubstanceInput( const string& inputName )
{
	for (SubstanceInputs::iterator i=m_Inputs.begin();i!=m_Inputs.end();++i)
	{
		if (i->name==inputName)
			return &*i;
	}
	return NULL;
}

bool ProceduralMaterial::IsSubstancePropertyCached( const string& inputName ) const
{
	const SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL)
		return input->IsFlagEnabled(SubstanceInput::Flag_Cached);
	return false;
}

void ProceduralMaterial::CacheSubstanceProperty( const string& inputName, bool value )
{
	SubstanceInput* input = FindSubstanceInput(inputName);
	if (input!=NULL)
		input->EnableFlag(SubstanceInput::Flag_Cached, value);
}

void ProceduralMaterial::ClearCache()
{
#if ENABLE_SUBSTANCE
	for (SubstanceInputs::iterator it=m_Inputs.begin() ; it!=m_Inputs.end() ; ++it)
		it->EnableFlag(SubstanceInput::Flag_Cached, false);
	GetSubstanceSystem().QueryClearCache(this);
#endif
}

void ProceduralMaterial::SetProceduralMemoryBudget(ProceduralCacheSize budget)
{
	SetProceduralMemorySleepBudget(budget);

	switch(budget)
	{
		case ProceduralCacheSize_Tiny:		SetProceduralMemoryWorkBudget(ProceduralCacheSize_Medium); break;
		case ProceduralCacheSize_Medium:	SetProceduralMemoryWorkBudget(ProceduralCacheSize_Heavy); break;
		case ProceduralCacheSize_Heavy:		SetProceduralMemoryWorkBudget(ProceduralCacheSize_NoLimit); break;
		case ProceduralCacheSize_NoLimit:	SetProceduralMemoryWorkBudget(ProceduralCacheSize_NoLimit); break;
		default:
		case ProceduralCacheSize_None:		SetProceduralMemoryWorkBudget(ProceduralCacheSize_Tiny); break;
	}
}

ProceduralCacheSize ProceduralMaterial::GetProceduralMemoryBudget() const
{
	if (m_SubstanceData==NULL)
		return ProceduralCacheSize_None;
	return m_SubstanceData->memoryWorkBudget;
}

void ProceduralMaterial::SetProceduralMemoryWorkBudget(ProceduralCacheSize budget)
{
	if (m_SubstanceData!=NULL)
		m_SubstanceData->memoryWorkBudget = budget;
}

ProceduralCacheSize ProceduralMaterial::GetProceduralMemoryWorkBudget() const
{
	if (m_SubstanceData==NULL)
		return ProceduralCacheSize_None;
	return m_SubstanceData->memoryWorkBudget;
}

void ProceduralMaterial::SetProceduralMemorySleepBudget(ProceduralCacheSize budget)
{
	if (m_SubstanceData!=NULL)
		m_SubstanceData->memorySleepBudget = budget;
}

ProceduralCacheSize ProceduralMaterial::GetProceduralMemorySleepBudget() const
{
	if (m_SubstanceData==NULL)
		return ProceduralCacheSize_None;
	return m_SubstanceData->memorySleepBudget;
}

#if UNITY_EDITOR
// Strips substance data during serialization when building a player
struct TemporarilyStripSubstanceData
{
	ProceduralMaterial*    material;
	PPtr<SubstanceArchive> substancePackage;
	SubstanceInputs        inputs;

	TemporarilyStripSubstanceData (ProceduralMaterial& mat, bool isBuildingPlayer)
	{
		bool shouldDiscardSubstanceData;
		shouldDiscardSubstanceData  = !IsSubstanceSupportedOnPlatform(GetEditorUserBuildSettings().GetActiveBuildTarget());
		shouldDiscardSubstanceData |= mat.GetLoadingBehavior() == ProceduralLoadingBehavior_BakeAndDiscard;

		// Should we discard the substance data?
		if (isBuildingPlayer && shouldDiscardSubstanceData)
		{
			material = &mat;

			// Clear m_SubstancePackage & m_Inputs and back it up so we can revert them after serialization
			swap(substancePackage, mat.m_SubstancePackage);
			swap(inputs, mat.m_Inputs);
		}
		else
		{
			material = NULL;
		}
	}

	~TemporarilyStripSubstanceData ()
	{
		if (material != NULL)
		{
			swap(material->m_Inputs, inputs);
			swap(material->m_SubstancePackage, substancePackage);
		}
	}
};
#endif



template<class T> void ProceduralMaterial::Transfer( T& transfer )
{
	Super::Transfer( transfer );

	// Serialize maximum sizes
	if (transfer.IsVersionSmallerOrEqual (2))
	{
		int m_MaximumSize;
		TRANSFER( m_MaximumSize );
		m_Width = m_MaximumSize;
		m_Height = m_MaximumSize;
	}
	else
	{
		TRANSFER( m_Width );
		TRANSFER( m_Height );
	}

	TRANSFER( m_Textures );
	TRANSFER( m_Flags );

	// Serialize load behavior
    if (transfer.IsReading ())
	{
		// Handle deprecated GenerateAtLoad flag, replaced by LoadingBehavior
	    m_LoadingBehavior = IsFlagEnabled(Flag_DeprecatedGenerateAtLoad)?ProceduralLoadingBehavior_Generate:ProceduralLoadingBehavior_None;
		EnableFlag(Flag_DeprecatedGenerateAtLoad, false);
	}
	transfer.Transfer(reinterpret_cast<int&> (m_LoadingBehavior), "m_LoadingBehavior");

	#if UNITY_EDITOR
	// Strip unused data when building/collecting assets
	TemporarilyStripSubstanceData stripData (*this, transfer.GetFlags () & kBuildPlayerOnlySerializeBuildProperties);
	#endif

	TRANSFER( m_SubstancePackage );
	TRANSFER( m_Inputs );

	TRANSFER( m_PrototypeName );
	if (m_PrototypeName=="")
	{
		m_PrototypeName = GetName();
	}

	TRANSFER( m_AnimationUpdateRate );

	TRANSFER(m_Hash);
}

bool ProceduralMaterial::IsProcessing() const
{
#if ENABLE_SUBSTANCE
	return GetSubstanceSystem().IsSubstanceProcessing(this);
#else
	return false;
#endif
}

void ProceduralMaterial::SetProceduralProcessorUsage(ProceduralProcessorUsage processorUsage)
{
#if ENABLE_SUBSTANCE
	GetSubstanceSystem().SetProcessorUsage(processorUsage);
#endif
}

ProceduralProcessorUsage ProceduralMaterial::GetProceduralProcessorUsage()
{
#if ENABLE_SUBSTANCE
	return GetSubstanceSystem().GetProcessorUsage();
#else
	return ProceduralProcessorUsage_Unsupported;
#endif
}

void ProceduralMaterial::StopProcessing()
{
#if ENABLE_SUBSTANCE
	GetSubstanceSystem().ClearProcessingQueue();
#endif
}

#if UNITY_EDITOR

void ProceduralMaterial::InvalidateIfCachedOrInvalidTextures()
{
	// Check if some textures are cached
	bool cachedOrInvalid=false;
	for (PingedTextures::iterator i=m_PingedTextures.begin();i!=m_PingedTextures.end();++i)
	{
		ProceduralTexture* texture = *i;
		if (texture!=NULL
		    && (texture->IsFlagEnabled(ProceduralTexture::Flag_Cached)
		        || !texture->IsValid()))
		{
			cachedOrInvalid = true;
			break;
		}
	}

	if (cachedOrInvalid)
	{
		// Check if an input has been modified
		bool modified = false;
		for (SubstanceInputs::iterator i=m_Inputs.begin();i != m_Inputs.end();++i)
		{
			SubstanceInput& input = *i;
			if (input.IsFlagEnabled(SubstanceInput::Flag_Modified))
			{
				modified = true;
				break;
			}
		}
		// Force rebuild since no input is modified
		if (!modified)
		{
			for (SubstanceInputs::iterator i=m_Inputs.begin();i != m_Inputs.end();++i)
			{
				SubstanceInput& input = *i;
				input.EnableFlag(SubstanceInput::Flag_Awake);
			}
		}
	}
}

bool IsSubstanceSupportedOnPlatform(BuildTargetPlatform platform)
{
return (platform == kBuildWebPlayerLZMA
	|| platform == kBuildWebPlayerLZMAStreamed
	|| platform == kBuildStandaloneOSXIntel
	|| platform == kBuildStandaloneOSXIntel64
	|| platform == kBuildStandaloneOSXUniversal
	|| platform == kBuildStandaloneWinPlayer
	|| platform == kBuildStandaloneWin64Player
	|| platform == kBuildStandaloneLinux
	|| platform == kBuildStandaloneLinux64
	|| platform == kBuildStandaloneLinuxUniversal
	|| platform == kBuild_Android
	|| platform == kBuild_iPhone
	|| platform == kBuildNaCl
	);
}
#endif

bool IsSubstanceSupported()
{
#if UNITY_EDITOR
	BuildTargetPlatform platform = GetEditorUserBuildSettings().GetActiveBuildTarget();
	return IsSubstanceSupportedOnPlatform(platform);
#endif

#if ENABLE_SUBSTANCE
	return true;
#else
	return false;
#endif
}

TextureFormat GetSubstanceTextureFormat(SubstanceOutputFormat outputFormat, bool requireCompressed)
{
#if ENABLE_SUBSTANCE
#if UNITY_EDITOR
	BuildTargetPlatform platform = GetEditorUserBuildSettings().GetActiveBuildTarget();
	if (!requireCompressed || IsSubstanceSupportedOnPlatform(platform))
#endif
	{
		TextureFormat format(kTexFormatRGBA32);

		if (outputFormat==Substance_OFormat_Compressed)
		{
#if UNITY_IPHONE
			format = kTexFormatPVRTC_RGBA4;
#elif UNITY_ANDROID
			if (gGraphicsCaps.supportsTextureFormat[kTexFormatDXT5])
			{
				format = kTexFormatDXT5;
			}
			else if (gGraphicsCaps.supportsTextureFormat[kTexFormatPVRTC_RGBA4])
			{
				format = kTexFormatPVRTC_RGBA4;
			}
			else
			{
				// Lowest common denominator = ETC
				// But this will cancel the alpha, need to think of something else for non-DXT non-PVR platforms
				// format = kTexFormatETC_RGB4;
			}
#else
			format = kTexFormatDXT5;
#endif
		}

		return format;
	}
#endif

	return kTexFormatRGBA32;
}

SubstanceEngineIDEnum GetSubstanceEngineID()
{
#if defined(__ppc__)
	return Substance_EngineID_xenos;
#endif
	return Substance_EngineID_sse2;
}
