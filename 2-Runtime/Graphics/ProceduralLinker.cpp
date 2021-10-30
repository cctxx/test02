#include "UnityPrefix.h"
#include "ProceduralMaterial.h"
#include "SubstanceArchive.h"
#include "SubstanceSystem.h"

#if ENABLE_SUBSTANCE
bool ProceduralShuffleOutputs (SubstanceLinkerHandle* linkerHandle, ProceduralMaterial::PingedTextures& textures, SubstanceInputs& inputs)
{
	for (ProceduralMaterial::PingedTextures::iterator it=textures.begin();it!=textures.end();++it)
	{
		unsigned int rI = 0, gI = 1, bI = 2, aI = 3;
		ProceduralTexture* texture = *it;
		ProceduralTexture* alpha = NULL;
		SubstanceLinkerShuffle shuffle;
		unsigned int baseOutputUid;
		unsigned int shuffledOutputUid;

		texture->SetSubstancePreShuffleUID(texture->GetSubstanceTextureUID());
        texture->EnableFlag(ProceduralTexture::Flag_Binded, false);

		if (texture->GetSubstanceFormat()==kTexFormatARGB32)
		{
			rI = 1;
			gI = 2;
			bI = 3;
			aI = 0;
		}

		baseOutputUid = texture->GetSubstanceTextureUID();

		// RGB normal map
		if (texture->GetUsageMode() == kTexUsageNormalmapPlain)
		{
			shuffle.useLevels = 1;
			shuffle.channels[rI].outputUID = baseOutputUid;
			shuffle.channels[rI].channelIndex = 0;
			shuffle.channels[rI].levelMin = 0.0f;
			shuffle.channels[rI].levelMax = 1.0f;
			shuffle.channels[gI].outputUID = baseOutputUid;
			shuffle.channels[gI].channelIndex = 1;
			shuffle.channels[gI].levelMin = 1.0f;
			shuffle.channels[gI].levelMax = 0.0f;
			shuffle.channels[bI].outputUID = baseOutputUid;
			shuffle.channels[bI].channelIndex = 2;
			shuffle.channels[bI].levelMin = 0.0f;
			shuffle.channels[bI].levelMax = 1.0f;
			shuffle.channels[aI].outputUID = baseOutputUid;
			shuffle.channels[aI].channelIndex = 3;
			shuffle.channels[aI].levelMin = 0.0f;
			shuffle.channels[aI].levelMax = 1.0f;
		}
		// platform uses optimized normal map (Z reconstruction)
		else if (texture->GetUsageMode() == kTexUsageNormalmapDXT5nm)
		{
			shuffle.useLevels = 1;
			shuffle.channels[rI].outputUID = baseOutputUid;
			shuffle.channels[rI].channelIndex = 1;
			shuffle.channels[rI].levelMin = 1.0f;
			shuffle.channels[rI].levelMax = 0.0f;
			shuffle.channels[gI].outputUID = baseOutputUid;
			shuffle.channels[gI].channelIndex = 1;
			shuffle.channels[gI].levelMin = 1.0f;
			shuffle.channels[gI].levelMax = 0.0f;
			shuffle.channels[bI].outputUID = baseOutputUid;
			shuffle.channels[bI].channelIndex = 1;
			shuffle.channels[bI].levelMin = 1.0f;
			shuffle.channels[bI].levelMax = 0.0f;
			shuffle.channels[aI].outputUID = baseOutputUid;
			shuffle.channels[aI].channelIndex = 0;
			shuffle.channels[aI].levelMin = 0.0f;
			shuffle.channels[aI].levelMax = 1.0f;
		}
		else
		{
			shuffle.useLevels = 0;
			shuffle.channels[rI].outputUID = baseOutputUid;
			shuffle.channels[rI].channelIndex = 0;
			shuffle.channels[gI].outputUID = baseOutputUid;
			shuffle.channels[gI].channelIndex = 1;
			shuffle.channels[bI].outputUID = baseOutputUid;
			shuffle.channels[bI].channelIndex = 2;
			shuffle.channels[aI].outputUID = baseOutputUid;
			shuffle.channels[aI].channelIndex = texture->GetType()==Substance_OType_Specular?0:3;

			if (texture->GetAlphaSource()!=Substance_OType_Unknown)
			{
				for (ProceduralMaterial::PingedTextures::iterator i=textures.begin();i!=textures.end();++i)
				{
					ProceduralTexture* alphaTexture = *i;
					if (alphaTexture->GetType() == texture->GetAlphaSource())
					{
						alpha = alphaTexture;
						shuffle.channels[aI].outputUID = alphaTexture->GetSubstanceTextureUID();
						shuffle.channels[aI].channelIndex = 0;
						break;
					}
				}
			}
		}

		unsigned int substance_format;
		switch( texture->GetSubstanceFormat() )
		{
			case kTexFormatDXT5:
				substance_format = Substance_PF_DXT5;
				break;
			case kTexFormatETC_RGB4:
				substance_format = Substance_PF_ETC1;
				break;
			case kTexFormatPVRTC_RGBA4:
				substance_format = Substance_PF_PVRTC4;
				break;
			default:
				substance_format = Substance_PF_RGBA;
		}
		if (substanceLinkerHandleCreateOutput(linkerHandle, &shuffledOutputUid,
			substance_format, 0, Substance_Linker_Flip_Vertical, &shuffle)!=0)
		{
			return false;
		}

		texture->SetSubstanceShuffledUID(shuffledOutputUid);

		for (SubstanceInputs::iterator i=inputs.begin();i!=inputs.end();++i)
		{
			// look if main texture is used
			{
				std::set<unsigned int>::iterator ai = std::find(
					i->alteredTexturesUID.begin(), i->alteredTexturesUID.end(), texture->GetSubstanceBaseTextureUID());
				if (ai!=i->alteredTexturesUID.end())
				{
					texture->EnableFlag(ProceduralTexture::Flag_Binded, true);
				}
			}

			// look if alpha texture is used
			if (alpha!=NULL)
			{
				std::set<unsigned int>::iterator ai = std::find(
					i->alteredTexturesUID.begin(), i->alteredTexturesUID.end(), alpha->GetSubstanceBaseTextureUID());
				if (ai!=i->alteredTexturesUID.end())
				{
					i->alteredTexturesUID.insert(texture->GetSubstanceBaseTextureUID());
					texture->EnableFlag(ProceduralTexture::Flag_Binded, true);
				}
			}
		}
	}
	return true;
}

ProceduralMaterial* ProceduralMaterial::m_PackedSubstance = NULL;

void SUBSTANCE_CALLBACK	ProceduralHandleUIDConflict(SubstanceLinkerHandle* handle, SubstanceLinkerUIDCollisionType conflict, unsigned int previousUID, unsigned int newUID)
{
    SubstanceInputs& inputs = ProceduralMaterial::m_PackedSubstance->GetSubstanceInputs();
	ProceduralMaterial::PingedTextures& textures = ProceduralMaterial::m_PackedSubstance->GetPingedTextures();
	if (conflict==Substance_Linker_UIDCollision_Input)
    {
	    for (SubstanceInputs::iterator it=inputs.begin() ; it!=inputs.end() ; ++it)
	    {
		    if (previousUID==it->internalIdentifier)
		    {
			    it->shuffledIdentifier = newUID;
                break;
		    }
	    }
    }
    else if (conflict==Substance_Linker_UIDCollision_Output)
    {
	    for (ProceduralMaterial::PingedTextures::iterator it=textures.begin() ; it!=textures.end() ; ++it)
	    {
		    ProceduralTexture* texture = *it;
		    if (texture!=NULL && previousUID==texture->GetSubstanceBaseTextureUID())
		    {
			    texture->SetSubstanceShuffledUID(newUID);
                break;
		    }
	    }
    }
}

void ProceduralMaterial::PackSubstances(std::vector<ProceduralMaterial*>& materials)
{
    if (materials.size()==0)
		return;

	// Create linker context
	SubstanceEngineIDEnum currentEngine(GetSubstanceEngineID());
	SubstanceLinkerContext*	linkerContext;
	SubstanceLinkerHandle* linkerHandle;
	if (substanceLinkerContextInit(&linkerContext, SUBSTANCE_LINKER_API_VERSION, currentEngine)!=0
        || substanceLinkerContextSetCallback(linkerContext, Substance_Linker_Callback_UIDCollision, (void*)&ProceduralHandleUIDConflict)!=0
        || substanceLinkerHandleInit(&linkerHandle, linkerContext)!=0)
	{
		ErrorString("Failed to initialize substance linker");
        for (std::vector<ProceduralMaterial*>::iterator it=materials.begin();it!=materials.end();++it)
	        (*it)->EnableFlag(Flag_Broken);
		return;
	}

	// Make the list of materials to pack
	std::vector<ProceduralMaterial*> packedMaterials;
	packedMaterials.reserve(materials.size());

	// Try to push all substances assembly
	for (std::vector<ProceduralMaterial*>::iterator it=materials.begin();it!=materials.end();++it)
	{
        if ((*it)->IsFlagEnabled(Flag_Broken))
            continue;

        SubstanceArchive* package = (*it)->m_PingedPackage;
		Assert(package!=NULL);

		unsigned size = package->GetBufferSize();
		UInt8* bufferData = package->GetBufferData();

		m_PackedSubstance = *it;

		// Prepare texture UIDs
		{
			for (ProceduralMaterial::PingedTextures::iterator i=m_PackedSubstance->m_PingedTextures.begin();i!=m_PackedSubstance->m_PingedTextures.end();++i)
				(*i)->SetSubstanceShuffledUID((*i)->GetSubstanceBaseTextureUID());
		}

		// Prepare input UIDs
		{
			// Check if it's the old format
			if (m_PackedSubstance->m_Inputs.size()>0
				&& m_PackedSubstance->m_Inputs[0].internalIdentifier==0)
			{
				SubstanceLinkerContext *context;
				SubstanceLinkerHandle* linkHandle;
				size_t substanceDataSize = 0;
				UInt8* buffer = NULL;
				if (substanceLinkerContextInit(&context, SUBSTANCE_LINKER_API_VERSION, currentEngine)==0
					&& substanceLinkerHandleInit(&linkHandle, context)==0
					&& substanceLinkerHandlePushAssemblyMemory(linkHandle, (const char*)bufferData, size)==0
					&& substanceLinkerHandleLink(linkHandle, (const unsigned char**)&buffer, &substanceDataSize)==0)
				{
					SubstanceHandle* substanceHandle = NULL;
					if (substanceHandleInit( &substanceHandle, GetSubstanceSystem().GetContext(), buffer, substanceDataSize, NULL )==0)
					{
						for (SubstanceInputs::iterator i=m_PackedSubstance->m_Inputs.begin();i!=m_PackedSubstance->m_Inputs.end();++i)
						{
							SubstanceInputDesc inputDescription;
							if (substanceHandleGetInputDesc(substanceHandle, i->internalIndex, &inputDescription)==0)
								i->internalIdentifier = inputDescription.inputId;
						}
						substanceHandleRelease(substanceHandle);
					}

					substanceLinkerHandleRelease(linkHandle);
					substanceLinkerContextRelease(context);
				}
                else
                {
                    ErrorStringObject("Failed to initialize substance linker", m_PackedSubstance);
                    m_PackedSubstance->EnableFlag(Flag_Broken);
                    continue;
                }
			}

			// Prepare shuffled ID
			for (SubstanceInputs::iterator i=m_PackedSubstance->m_Inputs.begin();i!=m_PackedSubstance->m_Inputs.end();++i)
			{
				i->shuffledIdentifier = i->internalIdentifier;
			}
		}

		// Push assembly if it has not already been pushed
		// If we're creating another instance of a graph that has already been pushed, then we still need to push the SBSASM,
		// otherwise we'll just get copies of the previous instance's outputs.
		if ((!package->m_isPushed) || (package->m_generatedGraphs.count(m_PackedSubstance->m_PrototypeName) == 1))
		{
			if (substanceLinkerHandlePushAssemblyMemory(linkerHandle, (const char*)bufferData, size)!=0)
			{
				ErrorStringObject("Failed to pack substance (substanceLinkerHandlePushAssemblyMemory)", *it);
				m_PackedSubstance->EnableFlag(Flag_Broken);
				continue;
			}
			package->m_isPushed = true;
		}
		package->m_generatedGraphs.insert(m_PackedSubstance->m_PrototypeName);

		// Set output formats & create shuffled outputs if needed
		if (!ProceduralShuffleOutputs(linkerHandle, (*it)->m_PingedTextures, (*it)->m_Inputs))
		{
			ErrorStringObject("Failed to pack substance (ShuffleOutputs)", *it);
			m_PackedSubstance->EnableFlag(Flag_Broken);
            continue;
		}

		// Set inputs constness
		#if !UNITY_EDITOR
			if ((*it)->IsFlagEnabled(Flag_ConstSize))
			{
				SubstanceInputs& inputs = (*it)->m_Inputs;
                bool broken=false;
				for (SubstanceInputs::iterator i=inputs.begin();i!=inputs.end();++i)
				{
					if (i->name=="$outputsize" || i->name=="$randomseed")
					{
						SubstanceLinkerInputValue value;
						value.integer[0] = (int)i->value.scalar[0];
						value.integer[1] = (int)i->value.scalar[1];
						value.integer[2] = (int)i->value.scalar[2];
						value.integer[3] = (int)i->value.scalar[3];
						if (substanceLinkerConstifyInput(linkerHandle, i->shuffledIdentifier, &value)!=0)
						    break;
					}
				}
                if (broken)
                {
                    ErrorStringObject("Failed to pack substance (substanceLinkerConstifyInput)", *it);
					m_PackedSubstance->EnableFlag(Flag_Broken);
                    continue;
                }
			}
		#endif

		packedMaterials.push_back(*it);
	}

	// Select all used outputs
	if (packedMaterials.size()>0 && substanceLinkerHandleSelectOutputs(linkerHandle, Substance_Linker_Select_UnselectAll, 0)!=0)
	{
		ErrorString("Failed to pack substances (substanceLinkerHandleSelectOutputs)");
        for (int i=packedMaterials.size()-1;i>=0;--i)
            packedMaterials[i]->EnableFlag(Flag_Broken);
		return;
	}
	for (int i=packedMaterials.size()-1;i>=0;--i)
	{
		ProceduralMaterial* material = packedMaterials[i];
		for (ProceduralMaterial::PingedTextures::iterator it=material->m_PingedTextures.begin();it!=material->m_PingedTextures.end();++it)
		{
			ProceduralTexture* texture = *it;
			if (substanceLinkerHandleSelectOutputs(linkerHandle, Substance_Linker_Select_Select, texture->GetSubstancePreShuffleUID())!=0)
			{
				ErrorStringObject("Failed to pack substance (Substance_Linker_Select_Select)", material);
                material->EnableFlag(Flag_Broken);
				packedMaterials.erase(packedMaterials.begin()+i);
				break;
			}
		}
	}

	// Nothing to pack ?
	if (packedMaterials.size()==0)
	{
		substanceLinkerHandleRelease(linkerHandle);
		substanceLinkerContextRelease(linkerContext);
		return;
	}

	// Initialize substance handle
	SubstanceData* pack = (SubstanceData*) UNITY_MALLOC_ALIGNED_NULL(kMemSubstance, sizeof(SubstanceData), 32);
	if (!pack)
	{
		ErrorString("Could not allocate memory for Substance data (PackSubstances)");
		for (int i=packedMaterials.size()-1;i>=0;--i)
			packedMaterials[i]->EnableFlag(Flag_Broken);
		return;
	}

	pack->substanceHandle = NULL;
	pack->instanceCount = 0;
	pack->memorySleepBudget = ProceduralCacheSize_None;
	pack->memoryWorkBudget = ProceduralCacheSize_Tiny;


	// Do we need to link the SBSASM?
	// Only case where we DON'T need to link is when the three following conditions are met:
	// - we are only linking a single substance
	// - we are linking a clone
	// - we have linked the same standalone substance clone once and cached the SBSBIN data
	// If one of the above conditions is not met, we need to link.
	size_t substanceDataSize = 0;
	UInt8* buffer = NULL;
	const ProceduralMaterial* currentMat = packedMaterials[0];
	const UnityStr& prototypeName = currentMat->m_PrototypeName;
	if (!((packedMaterials.size() == 1) 
	      && (currentMat->IsFlagEnabled(Flag_Clone))
	      && (currentMat->m_PingedPackage->IsCloneDataAvailable(prototypeName))))
	{
		if (substanceLinkerHandleLink(linkerHandle, (const unsigned char**)&buffer, &substanceDataSize )!=0)
		{
			ErrorStringObject("Failed to pack substances (substanceLinkerHandleLink)", currentMat);
			for (int i=packedMaterials.size()-1;i>=0;--i)
				packedMaterials[i]->EnableFlag(Flag_Broken);
			return;
		}
	}

	// Handle the "cloning of a single substance" situation
	if ((packedMaterials.size() == 1) && (currentMat->IsFlagEnabled(Flag_Clone)))
	{
		// In that case, we link and cache the SBSBIN inside the SubstancePackage.
		// When the same substance is cloned again, this vanilla SBSBIN data is reused to avoid reallocating memory
		SubstanceArchive* package = currentMat->m_PingedPackage;
		if (package->IsCloneDataAvailable(prototypeName))
		{
			pack->substanceData = package->GetLinkedBinaryData(prototypeName);
		}
		else
		{
			if (package->SaveLinkedBinaryData(prototypeName, (const UInt8*) buffer, (const int) substanceDataSize))
			{
				pack->substanceData = package->GetLinkedBinaryData(prototypeName);
			}
			else
			{
				WarningStringMsg("Failed to save SBSBIN data for material %s", currentMat->GetName());
				pack->substanceData = (UInt8*) UNITY_MALLOC_ALIGNED_NULL(kMemSubstance, substanceDataSize, 32);
				if (pack->substanceData)
				{
					memcpy(pack->substanceData, buffer, substanceDataSize);
				}
				else
				{
					ErrorString("Could not allocate memory for Substance linked data");
					for (int i=packedMaterials.size()-1;i>=0;--i)
							packedMaterials[i]->EnableFlag(Flag_Broken);
					return;
				}
			}
		}
	}
	else
	{
		// Either we're packing more than one substance, or this is not a clone, in
		// which case the SBSBIN is not reusable because of constified inputs.
		pack->substanceData = (UInt8*) UNITY_MALLOC_ALIGNED_NULL(kMemSubstance, substanceDataSize, 32);
		if (pack->substanceData)
		{
			memcpy(pack->substanceData, buffer, substanceDataSize);
		}
		else
		{
			ErrorString("Could not allocate memory for Substance linked data");
			for (int i=packedMaterials.size()-1;i>=0;--i)
					packedMaterials[i]->EnableFlag(Flag_Broken);
			return;
		}
	}

	// Reset the isPushed flags and the list of generated graphs of the packages that have been pushed and linked
	for (std::vector<ProceduralMaterial*>::iterator it=materials.begin();it!=materials.end();++it)
	{
		SubstanceArchive *package = (*it)->m_PingedPackage;
		package->m_isPushed = false;
		package->m_generatedGraphs.clear();
	}

	// Release linker
	substanceLinkerHandleRelease(linkerHandle);
	substanceLinkerContextRelease(linkerContext);

	// Create substance handle
	if (substanceHandleInit(&pack->substanceHandle, GetSubstanceSystem().GetContext(), pack->substanceData, substanceDataSize, NULL)!=0)
	{
		ErrorStringObject("Failed to pack substances (substanceHandleInit)", packedMaterials[0]);
		for (int i=packedMaterials.size()-1;i>=0;--i)
            packedMaterials[i]->EnableFlag(Flag_Broken);
		return;
	}

	// Bind all substances
	for (std::vector<ProceduralMaterial*>::iterator it=packedMaterials.begin();it!=packedMaterials.end();++it)
	{
		bool broken(false);

		// Update input identifiers (todo: improve this if doable)
		for (SubstanceInputs::iterator i=(*it)->m_Inputs.begin();i!=(*it)->m_Inputs.end();++i)
		{
			i->internalIndex = -1;
			int id=0;
			SubstanceInputDesc inputDescription;
			while (substanceHandleGetInputDesc(pack->substanceHandle, id, &inputDescription)==0)
			{
				if (i->shuffledIdentifier==inputDescription.inputId)
				{
					i->internalIndex = id;
					break;
				}
				++id;
			}
			if (i->internalIndex==-1)
			{
#if !UNITY_EDITOR
				// Don't break since the input may be removed
				if ((*it)->IsFlagEnabled(Flag_ConstSize) && (i->name=="$outputsize" || i->name=="$randomseed"))
				{
					continue;
				}
#endif
				broken = true;
			}
		}

		if (broken)
		{
			ErrorStringObject("Failed to pack substances (SubstanceInputDesc)", *it);
			(*it)->EnableFlag(Flag_Broken);
		    continue;
		}

		(*it)->m_SubstanceData = pack;
		++pack->instanceCount;
	}

	if (pack->instanceCount==0)
	{
		substanceHandleRelease(pack->substanceHandle);
		UNITY_FREE(kMemSubstance, pack->substanceData);
		UNITY_FREE(kMemSubstance, pack);
	}
}

#endif	// ENABLE_SUBSTANCE
