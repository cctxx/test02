// SUBSTANCE HOOK

#include "UnityPrefix.h"
#include "SubstanceImporter.h"
#include "AssetDatabase.h"
#include "AssetImportState.h"
#include "Runtime/Modules/LoadDylib.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Graphics/S3Decompression.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/DXTCompression.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Serialize/SwapEndianArray.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Shaders/Shader.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Runtime/Graphics/ProceduralMaterial.h"
#include "Runtime/Graphics/ProceduralTexture.h"
#include "Editor/Src/AssetPipeline/SubstanceCache.h"
#include "Editor/Src/AssetPipeline/SubstanceLoader.h"
#include "Runtime/Graphics/SubstanceSystem.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/AssetPipeline/ImageConverter.h"
#include "Editor/Src/AssetPipeline/ImageOperations.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Editor/Src/AssetPipeline/ObjectHashGenerator.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Serialize/SerializedFile.h"

#include <sstream>
#include <cmath>

#define SKIP_PLAYMODE() if (m_IsPlaying) return

using namespace std;

#define DEBUG_SUBSTANCE_IMPORTER 0

// Increasing the version number causes an automatic reimport when the last imported version is a different.
// Only increase this number if an older version of the importer generated bad data or the importer applies data optimizations that can only be done in the importer.
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
const int kSubstanceImporterVersion = 10;

static void BindTexturesToShader( ProceduralMaterial& _Material );
static void ApplyMaterialInformation (ProceduralMaterial& material, const ProceduralMaterialInformation& information);
static void ApplyMaterialSettings( ProceduralMaterial& material, const MaterialInstanceSettings& settings );
static ProceduralOutputType SubstanceUsageNameToProceduralPropertyType (const std::string& usageArbitraryCase);
static TextureUsageMode SubstanceUsageNameToTextureUsage (BuildTargetPlatform target, const std::string& usageArbitraryCase);

struct EqualTargetByName
{
	string buildTargetName;

	EqualTargetByName(string t)
	{
		buildTargetName = t;
	}

	bool operator()(const SubstanceBuildTargetSettings& bts)
	{
		return bts.m_BuildTarget == buildTargetName;
	}
};

SubstanceImporter::SubstanceImporter( MemLabelId label, ObjectCreationMode _Mode ) :
	Super( label, _Mode )
{
}

SubstanceImporter::~SubstanceImporter()
{
}

void SubstanceImporter::AwakeFromLoad( AwakeFromLoadMode awakeMode )
{
	AssetImporter::AwakeFromLoad( awakeMode );
}

static int FindInputImportSetting (const std::vector<InputImportSettings>& inputs, const std::string& name)
{
	for (int i=0;i<inputs.size();i++)
	{
		if (inputs[i].name == name)
			return i;
	}
	return -1;
}

void SubstanceImporter::ApplyInputImportSettings (ProceduralMaterial& material, const MaterialInstanceSettings& settings, SubstanceInputs& outInputs)
{
	for (SubstanceInputs::iterator i=outInputs.begin();i!= outInputs.end();i++)
	{
		SubstanceInput& outputValue = *i;
		int index = FindInputImportSetting(settings.inputs, outputValue.name);
		if (index != -1)
		{
			const InputImportSettings& inputSetting = settings.inputs[index];

			memcpy(outputValue.value.scalar, inputSetting.value.scalar, sizeof(float) * 4);
			outputValue.value.texture = inputSetting.value.texture;
			ClampSubstanceInputValues(outputValue, outputValue.value);
		}
		else
		{
			InputImportSettings& inputSetting = FindOrCreateInputImportSetting(material, outputValue.name);
			ClampSubstanceInputValues(outputValue, outputValue.value);
			memcpy(inputSetting.value.scalar, outputValue.value.scalar, sizeof(float) * 4);
			inputSetting.value.texture = outputValue.value.texture;
		}
	}
}

int IsLoadingBehaviourBaked(const int loadingBehaviour)
{
	bool baked = (loadingBehaviour == ProceduralLoadingBehavior_BakeAndDiscard) ||
	             (loadingBehaviour == ProceduralLoadingBehavior_BakeAndKeep);
	          
	return baked ? 1 : 0;
}

ProceduralMaterial* SubstanceImporter::GenerateSubstanceMaterial (
	TiXmlDocument& xmlData, SubstanceArchive& packageSBS, SubstanceHandle* substanceHandle, const std::string& prototypeName,
	const std::string& materialName, MaterialInstanceSettings* settings, MaterialImportOutput* import, BuildTargetPlatform targetPlatform, bool forceUncompressed)
{
	SubstanceInputs inputs;
	std::vector<SubstanceTextureInformation> outputs;
	ProceduralMaterial::Textures textures;

	// Inputs & outputs by parsing xml file
	if (!GenerateImportInformation(xmlData, substanceHandle, prototypeName, inputs, outputs))
	{
		LogImportError("Failed to import Substance: " + materialName);
		return NULL;
	}

	// Create material and textures
	ProceduralMaterial* material = ProduceAssetObjectHashBased<ProceduralMaterial> (materialName);
	material->SetHideFlagsObjectOnly(0);

	material->EnableFlag(ProceduralMaterial::Flag_Uncompressed, forceUncompressed);
	material->EnableFlag(ProceduralMaterial::Flag_Import, true);

	if (material == NULL)
	{
		LogImportWarning(materialName + " could not be generated because of a hash conflict. Please choose a different name for the texture in the sbs file.");
		return NULL;
	}

	// Override inputs using material instance data
	if (settings)
		ApplyInputImportSettings (*material, *settings, inputs);

	material->SetNameCpp(materialName);
	MaterialInstanceSettings& materialSettings = FindOrCreateMaterialSettings( material->GetName(), prototypeName );

	///@TODO: Use PPtr for shaderName
	// Apply shader
	Shader*	shader = NULL;
	if (settings)
		shader = GetScriptMapper().FindShader( settings->shaderName );

	if (shader == NULL)
	{
		materialSettings.shaderName = "Bumped Specular";
		shader = GetScriptMapper().FindShader( "Bumped Specular" );
	}

	material->SetShader(shader);

	ProceduralMaterialInformation materialInformation(
		settings?settings->materialInformation:materialSettings.materialInformation);

	// Check which textures are used, if required
	bool generateAll(material->GetShader()==NULL || materialInformation.m_GenerateAllOutputs!=0);
	std::vector<bool> usedTexture(outputs.size(), generateAll);
	if (!generateAll)
	{
		std::vector<bool> usedType((int)ProceduralOutputType_Count, false);
		for (int textureIndex=0; textureIndex<outputs.size(); ++textureIndex )
		{
			const ShaderLab::PropertySheet& properties = material->GetProperties();
			const ShaderLab::PropertySheet::TexEnvs& textureProperties = properties.GetTexEnvsMap();
			ShaderLab::FastPropertyName shaderProperty = ShaderLab::Property("_MainTex");
			ProceduralOutputType type = SubstanceUsageNameToProceduralPropertyType(outputs[textureIndex].usage);
			if (textureIndex>0 || textureProperties.count(shaderProperty)==0)
			{
				if (usedType[(int)type])
				{
					continue;
				}
				string shaderLabName = ProceduralPropertyTypeToUnityShaderPropertyName(type);
				if (textureProperties.count(ShaderLab::Property(shaderLabName))==0)
				{
					continue;
				}
			}

			usedTexture[textureIndex] = true;
			usedType[(int)type] = true;

			// Set alpha source as used, if required
			string textureName = materialName + "_" + outputs[textureIndex].name;
			TextureImportSettings* textureSettings = FindTextureSettings(*material, materialSettings, textureName);
			if ((textureSettings!=NULL && textureSettings->alphaSource!=Substance_OType_Unknown)
				|| (textureSettings==NULL && textureIndex==0))
			{
				for (int alphaIndex=0; alphaIndex<outputs.size(); ++alphaIndex)
				{
					ProceduralOutputType alphaUsage = SubstanceUsageNameToProceduralPropertyType(outputs[alphaIndex].usage);
					if ((textureSettings!=NULL && alphaUsage==textureSettings->alphaSource)
						|| (textureSettings==NULL && alphaUsage==Substance_OType_Specular))
					{
						usedTexture[alphaIndex] = true;
						break;
					}
				}
			}
		}
	}

	// Generate required texture assets
	for (int textureIndex=0; textureIndex<outputs.size(); ++textureIndex )
	{
		if (!usedTexture[textureIndex])
			continue;

		string textureName = materialName + "_" + outputs[textureIndex].name;
		ProceduralTexture*pTexture = ProduceAssetObjectHashBased<ProceduralTexture> (textureName);

		if (pTexture == NULL)
		{
			LogImportWarning(textureName + " could not be generated because of a hash conflict. Please choose a different name for the texture in the sbs file.");
			continue;
		}

		pTexture->SetHideFlagsObjectOnly(0);

		bool isDiffuse(SubstanceUsageNameToProceduralPropertyType(outputs[textureIndex].usage)==Substance_OType_Diffuse);
		const MaterialInstanceSettings* curSettings;
		TextureImportSettings* textureSettings;
		bool isFirstImport = settings==NULL || FindTextureSettings(*material, materialSettings, textureName)==NULL;
		textureSettings = &FindOrCreateTextureSettings(*material, materialSettings, textureName);

		if (isFirstImport && isDiffuse)
		{
			textureSettings->alphaSource = Substance_OType_Specular;
		}

		curSettings = (settings==NULL)?&materialSettings:settings;

		if (curSettings->buildTargetSettings.size()==0)
			SetPlatformTextureSettings(material->GetName(), "", 512, 512, Substance_OFormat_Compressed, ProceduralLoadingBehavior_Generate);
		SubstanceBuildTargetSettings sgs = GetPlatformSettings(materialName, targetPlatform);

		ProceduralOutputType substanceType = SubstanceUsageNameToProceduralPropertyType(outputs[textureIndex].usage);

		if (substanceType == Substance_OType_Diffuse
			|| substanceType == Substance_OType_Emissive
			|| substanceType == Substance_OType_Specular)
		{
			pTexture->SetStoredColorSpace (kTexColorSpaceSRGB);
		}

		pTexture->SetNameCpp(textureName);
		pTexture->Init( *material, outputs[textureIndex].UID, substanceType, (SubstanceOutputFormat)sgs.m_TextureFormat, textureSettings->alphaSource, true );
		pTexture->SetAnisoLevel(textureSettings->aniso);
		pTexture->SetFilterMode(textureSettings->filterMode);
		pTexture->SetWrapMode(textureSettings->wrapMode);
		pTexture->SetUsageMode( SubstanceUsageNameToTextureUsage(targetPlatform, outputs[textureIndex].usage));
		textures.push_back(pTexture);
	}

    material->Init( packageSBS, prototypeName, inputs, textures );

	if (settings)
	{
		ApplyMaterialSettings( *material, *(MaterialInstanceSettings*)settings );
	}

    BindTexturesToShader(*material);

	if (settings)
	{
        settings->materialProperties = material->GetSavedProperties();
		ApplyMaterialInformation (*material, ((MaterialInstanceSettings*)settings)->materialInformation);
	}

    // Update material texture size
    int width, height;
    SubstanceBuildTargetSettings sgs = GetPlatformSettings(material->GetName(), GetEditorUserBuildSettings().GetActiveBuildTarget());
    width = ceilf(Log2(sgs.m_TextureWidth));
	if (width<5) width=5;
	if (width>11) width=11;
	int correctWidth = pow(2.0f, width);
	height = ceilf(Log2(sgs.m_TextureHeight));
    if (height<5) height=5;
    if (height>11) height=11;
	int correctHeight = pow(2.0f, height);
	material->SetSize(width, height);
	// Update settings if they are incorrect
	if (settings && (sgs.m_TextureWidth!=correctWidth) || (sgs.m_TextureHeight!=correctHeight))
	{
		ErrorStringObject ("ProceduralMaterial has invalid size settings. Fixing!", material);
		sgs.m_TextureWidth = correctWidth;
		sgs.m_TextureHeight = correctHeight;
		std::vector<SubstanceBuildTargetSettings>::iterator it = find_if(
			settings->buildTargetSettings.begin(), settings->buildTargetSettings.end(),
			EqualTargetByName(GetBuildTargetGroupName(GetEditorUserBuildSettings().GetActiveBuildTarget(), false)));
		if (it == settings->buildTargetSettings.end())
			it = find_if(settings->buildTargetSettings.begin(), settings->buildTargetSettings.end(), EqualTargetByName(""));
		if (it != settings->buildTargetSettings.end())
		{
			it->m_TextureWidth = correctWidth;
			it->m_TextureHeight = correctHeight;
			SetDirty();
		}
	}
    for (ProceduralMaterial::Textures::iterator it=material->GetTextures().begin();it!=material->GetTextures().end();++it)
		(*it)->SetSubstanceFormat(GetSubstanceTextureFormat((SubstanceOutputFormat)sgs.m_TextureFormat, !forceUncompressed));

	// Store current import settings
	if (!forceUncompressed && !IsSubstanceSupportedOnPlatform(targetPlatform))
	{
		sgs.m_LoadingBehavior = ProceduralLoadingBehavior_BakeAndDiscard;
	}
	if (forceUncompressed && sgs.m_LoadingBehavior==ProceduralLoadingBehavior_BakeAndDiscard)
	{
		sgs.m_LoadingBehavior = ProceduralLoadingBehavior_BakeAndKeep;
	}

	import->currentSettings = sgs;
	import->baked = IsLoadingBehaviourBaked(sgs.m_LoadingBehavior);
	material->SetLoadingBehavior((ProceduralLoadingBehavior)sgs.m_LoadingBehavior);

	// We need the real TextureFormat, not the simple one
	import->currentSettings.m_TextureFormat = IsLoadingBehaviourBaked(sgs.m_LoadingBehavior) ?
	                                          GetSubstanceBakedFormat((SubstanceOutputFormat)sgs.m_TextureFormat) :
	                                          GetSubstanceTextureFormat((SubstanceOutputFormat)sgs.m_TextureFormat, !forceUncompressed);

	return material;
}

vector<string> SubstanceImporter::GetSubstanceNames ()
{
	string assetPathName = GetAssetPathName();
	std::vector<UInt8> assemblyContent;
	std::vector<UInt8> binaryContent;
	TiXmlDocument xmlData;

	std::vector<std::string> substances;
	if (!LoadSubstanceArchive(assetPathName, assemblyContent, binaryContent, xmlData, true))
		return substances;

	GetSubstanceNamesFromXml(xmlData, substances);
	return substances;
}

void UpdateHashOfProceduralMaterial (ProceduralMaterial& material)
{
	MdFour hash;
	if (material.GetLoadingBehavior()==ProceduralLoadingBehavior_Cache)
	{
		MdFourGenerator generator;

		// material name
		generator.Feed(material.GetName());

		// package
		vector<Object*> objects;
		objects.push_back(material.GetSubstancePackage());

		// inputs scalars
		for (SubstanceInputs::iterator it=material.GetSubstanceInputs().begin() ; it!=material.GetSubstanceInputs().end() ; ++it)
		{
			for (int i=0 ; i<4 ; ++i)
				generator.Feed(it->value.scalar[0]);
		}

		// inputs textures
		for (std::vector<ProceduralMaterial::TextureInput>::iterator it=material.GetTextureInputs().begin() ; it!=material.GetTextureInputs().end() ; ++it)
			objects.push_back(it->texture);
		FeedHashWithObjectAndAllDependencies(generator, objects, 0);

		// output format
		if (material.GetTextures().size()>0)
		{
			generator.Feed((int)material.GetTextures()[0]->GetDataFormat());
		}

		hash = generator.Finish();
	}
	memcpy(material.GetHashPtr(), hash.bytes, 16);
}

void SubstanceImporter::GenerateAssetData ()
{
	// LogString("Importing " + GetAssetPathName());

	bool forceUncompressed = GetImportFlags() & kForceUncompressedImport;
	BuildTargetPlatform targetPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget ();
	if (forceUncompressed)
	{
		GetAssetImportState().SetDidImportTextureUncompressed();
	}
	else
	{
		GetAssetImportState().SetDidImportAssetForTarget ( targetPlatform );
	}

	SubstanceSystem::Context context(ProceduralProcessorUsage_All);

	// Retrieve asset name
	string assetPathName = GetAssetPathName();
	string assetName = GetLastPathNameComponent(assetPathName);

	std::vector<UInt8> assemblyContent;
	std::vector<UInt8> binaryContent;
	TiXmlDocument xmlData;

	if (!LoadSubstanceArchive(PathToAbsolutePath(assetPathName), assemblyContent, binaryContent, xmlData))
	{
		LogImportError( "Import failed..." );
		return;
	}

	string lcAssetName = ToLower(assetName);
	size_t extensionPos = lcAssetName.rfind(".sbsar");
	assetName.erase(extensionPos);
	SubstanceArchive* packageSBS = ProduceAssetObjectHashBased<SubstanceArchive>(assetName);

	if (packageSBS == NULL)
	{
		LogImportError("SubstanceArchive could not be generated because of a hash conflict.");
		return;
	}

	packageSBS->SetHideFlags( kHideInspector );		// < Hide the Package itself in the inspector. The Importer is used for editing.

	packageSBS->Init(&assemblyContent[0], assemblyContent.size());

	SubstanceHandle* substanceHandle = NULL;
	int	error = substanceHandleInit( &substanceHandle, GetSubstanceSystem().GetContext(), &binaryContent[0], binaryContent.size(), NULL );
	if (error != 0)
	{
		LogImportError( "Loading the Substance failed" );
		return;
	}

	std::vector<std::string> prototypeNames;
	std::vector<std::string>::iterator it;

	GetSubstanceNamesFromXml(xmlData, prototypeNames);

	std::vector<ProceduralMaterial*> materials;

	/*
		If m_MaterialInstances is empty, this is the first import of the Substance
			For each non-deleted prototype, we generate a MaterialInstance and a MaterialImportOutput (in synch across vectors)
		Else
			For each MaterialInstance
				If the graph is not in the substance file anymore, delete the MaterialInstance and the MaterialImportOutput
				If the prototype is in the DeletedPrototypes, issue a warning!
				Generate the associated MaterialImportOutput
	*/

	if (m_MaterialInstances.empty())
	{
		if (!m_MaterialImportOutputs.empty())
		{
			WarningStringMsg("m_MaterialInstances is empty for asset %s, but m_MaterialImportOutputs is not!", assetName.c_str());
		}

		m_MaterialImportOutputs.clear();

		// Generate prototype instances, if not deleted
		for (std::vector<std::string>::const_iterator it=prototypeNames.begin() ; it!=prototypeNames.end() ; ++it)
		{
			UnityStr name(*it);
			std::set<UnityStr>::iterator i = std::find(m_DeletedPrototypes.begin(), m_DeletedPrototypes.end(), name);
			if (i==m_DeletedPrototypes.end() && FindPrototypeSettings(*it)==NULL)
			{
				MaterialImportOutput matImpOut;

				// Generate material
				materials.push_back(GenerateSubstanceMaterial(
					xmlData, *packageSBS, substanceHandle, *it, GenerateUniqueName(*it, prototypeNames), NULL, &matImpOut, targetPlatform, forceUncompressed));

				// Populate m_MaterialImportOutputs with the new import results
				m_MaterialImportOutputs.push_back(matImpOut);
			}
		}
	}
	else
	{
		if (m_MaterialImportOutputs.size()>m_MaterialInstances.size())
		{
			WarningStringMsg("Substance Importer has extra MaterialImportOutputs for asset %s", assetName.c_str());
		}
		
		if (m_MaterialImportOutputs.size()<m_MaterialInstances.size())
		{
			m_MaterialImportOutputs.resize(m_MaterialInstances.size());
		}
		
		std::vector<MaterialImportOutput>::iterator matImpOut = m_MaterialImportOutputs.begin();
		for(int i=0 ; i<m_MaterialInstances.size() ; i++, matImpOut++)
		{
			MaterialInstanceSettings& materialInstance = m_MaterialInstances[i];
			std::string protName = std::string(materialInstance.prototypeName);
			if (std::find(m_DeletedPrototypes.begin(), m_DeletedPrototypes.end(), protName) != m_DeletedPrototypes.end())
			{
				WarningStringMsg("Deleted prototype %s (asset %s) is being generated", protName.c_str(), assetName.c_str());
			}
			if (std::find(prototypeNames.begin(), prototypeNames.end(), protName) == prototypeNames.end())
			{
				m_MaterialInstances.erase(m_MaterialInstances.begin()+i);
				i--;
			}
			else
			{
				materials.push_back(GenerateSubstanceMaterial(
					xmlData, *packageSBS, substanceHandle, protName,
					materialInstance.name, &materialInstance, &(*matImpOut), targetPlatform, forceUncompressed));
			}
		}
	}

	// Generate textures if required
	m_CurrentImporter = this;
	m_CurrentInstance = packageSBS->GetInstanceID();

	// Invalidate textures to force a clean refresh if we're here because the SBSAR changed on disk
	if (GetImportFlags() & kImportAssetThroughRefresh)
	{
		GetSubstanceCache().SafeClear();
		for (std::vector<ProceduralMaterial*>::iterator material=materials.begin();material!=materials.end();++material)
		{
			for (ProceduralMaterial::Textures::iterator it=(*material)->GetTextures().begin() ; it!=(*material)->GetTextures().end() ; ++it)
			{
				(*it)->Invalidate();
			}
		}
	}

	for (std::vector<ProceduralMaterial*>::iterator material=materials.begin();material!=materials.end();++material)
	{
		if (*material!=NULL)
			(*material)->AwakeFromLoad(kDefaultAwakeFromLoad);
	}
	GetSubstanceSystem().WaitFinished();

	for (std::vector<ProceduralMaterial*>::iterator material=materials.begin();material!=materials.end();++material)
	{
		if (*material!=NULL)
			(*material)->EnableFlag(ProceduralMaterial::Flag_Import, false);
		UpdateHashOfProceduralMaterial(**material);
	}
	substanceHandleRelease(substanceHandle);
	substanceHandle = NULL;
	m_CurrentImporter = NULL;
	m_CurrentInstance = 0;
}

static int	CanLoadPathName( const string& _PathName, int* _pQueue );
void SubstanceImporter::InitializeClass()
{
	////@TODO: THe import order here is WRONG!
	RegisterCanLoadPathNameCallback( CanLoadPathName, ClassID(SubstanceImporter), kSubstanceImporterVersion, -100000000 );
}

// Checks if the file extension is SBSAR
//
static int	CanLoadPathName( const string& _PathName, int* _pQueue )
{
	*_pQueue = -10000;
	string	Extension = ToLower( GetPathNameExtension( _PathName ) );

	if (Extension == "sbsar")
	{
		return kCopyAsset;
	}

	return false;
}


SubstanceBuildTargetSettings::SubstanceBuildTargetSettings() :
	m_TextureWidth(512),
	m_TextureHeight(512),
	m_TextureFormat(0),
	m_LoadingBehavior(ProceduralLoadingBehavior_Generate)
{
}

template<class T>
void SubstanceBuildTargetSettings::Transfer (T& transfer)
{
	transfer.SetVersion(2);

	TRANSFER(m_BuildTarget);

	if (transfer.IsOldVersion (1))
	{
		int m_MaxTextureSize;
		TRANSFER(m_MaxTextureSize);
		m_TextureWidth = m_MaxTextureSize;
		m_TextureHeight = m_MaxTextureSize;
	}
	else
	{
		TRANSFER(m_TextureWidth);
		TRANSFER(m_TextureHeight);
	}

	TRANSFER(m_TextureFormat);
	TRANSFER(m_LoadingBehavior);
}

template<class T>
void SubstanceImporter::Transfer( T& transfer )
{
	Super::Transfer( transfer );
	transfer.SetVersion(5);

	if (transfer.IsOldVersion (1) && transfer.AssetMetaDataOnly())
	{
		// I gave up trying to make this readable using transfer. Just use the old code.
		// Too many special cases where structs like Vector2 are transferred as strings of the same name, etc.
		Assert ((IsSameType<YAMLRead, T>::result));
		YAMLNode* node = reinterpret_cast<YAMLRead&> (transfer).GetCurrentNode();
		// Let's make sure we have a valid YAMLMapping first (case 567991)
		YAMLMapping* mapping = dynamic_cast<YAMLMapping*>(node);
		if (mapping)
			ApplyImportSettingsDeprecated (mapping);
		delete node;
	}
	else
	{
		TRANSFER( m_MaterialInstances );
		transfer.Align();
		TRANSFER( m_DeletedPrototypes );
		if (!transfer.AssetMetaDataOnly())
			TRANSFER(m_MaterialImportOutputs);
 	}

	if (transfer.IsVersionSmallerOrEqual (2))
	{
		vector<string> prototypes = GetSubstanceNames();
		for (vector<string>::iterator it=prototypes.begin();it!=prototypes.end();++it)
		{
			if (FindPrototypeSettings(*it)==NULL)
				m_DeletedPrototypes.insert(*it);
		}
	}

	PostTransfer (transfer);
}

//////////////////////////////////////////////////////////////////////////
// Material & Inputs info accessors
//////////////////////////////////////////////////////////////////////////
//

void SubstanceImporter::SetMaterialInformation( ProceduralMaterial& material, ProceduralMaterialInformation information )
{
	ApplyMaterialInformation(material, information);
	if (!CheckPlayModeAndSkip(material))
	{
		MaterialInstanceSettings& settings = FindOrCreateMaterialSettings( material.GetName() );
		bool requireReimport(settings.materialInformation.m_GenerateAllOutputs!=information.m_GenerateAllOutputs);
		settings.materialInformation = information;
		SetDirty();
		if (requireReimport)
		{
			set<UnityGUID> assets;
			assets.insert(ObjectToGUID(&material));
			AssetInterface::Get().ImportAssets (assets, kForceUncompressedImport);
		}
	}
}

ProceduralMaterialInformation SubstanceImporter::GetMaterialInformation( ProceduralMaterial& material )
{
	ProceduralMaterialInformation information;
	information.m_GenerateAllOutputs = (int)(material.IsFlagEnabled(ProceduralMaterial::Flag_GenerateAll));
	information.m_AnimationUpdateRate = material.GetAnimationUpdateRate();

	// Try to retrieve offset & scale from textures
	if (material.GetShader()!=NULL)
	{
		const ShaderLab::PropertySheet& properties = material.GetProperties();
		const ShaderLab::PropertySheet::TexEnvs& textureProperties = properties.GetTexEnvsMap();
		for (ShaderLab::PropertySheet::TexEnvs::const_iterator it=textureProperties.begin();it!=textureProperties.end();++it)
		{
			if (material.ActuallyHasTextureProperty(it->first))
			{
				Texture* texture = material.GetTexture(it->first);

				if (texture != NULL
					&& texture->IsDerivedFrom(ProceduralTexture::GetClassIDStatic())
					&& ShaderLab::Property(ProceduralPropertyTypeToUnityShaderPropertyName(static_cast<ProceduralTexture*>(texture)->GetType()))==it->first)
				{
					information.m_Scale = material.GetTextureScale( it->first );
					information.m_Offset = material.GetTextureOffset( it->first );
					return information;
				}
			}
		}
	}

	MaterialInstanceSettings* settings = FindMaterialInstanceSettings( material.GetName() );
	if ( settings != NULL )
		return settings->materialInformation;
	return ProceduralMaterialInformation();
}

TextureImportSettings& SubstanceImporter::GetTextureParameters( ProceduralMaterial& material, std::string textureName )
{
	MaterialInstanceSettings& materialSettings = FindOrCreateMaterialSettings( material.GetName() );
	return FindOrCreateTextureSettings(material, materialSettings, textureName);
}

void SubstanceImporter::SetTextureAlphaSource( ProceduralMaterial& material, std::string textureName, ProceduralOutputType alphaSource )
{
	SKIP_PLAYMODE();
	MaterialInstanceSettings& materialSettings = FindOrCreateMaterialSettings( material.GetName() );
	TextureImportSettings& textureSettings = FindOrCreateTextureSettings(material, materialSettings, textureName);
	SetDirty();

	// Reimport if changed
	if (alphaSource!=textureSettings.alphaSource)
	{
		textureSettings.alphaSource = alphaSource;

		// Remove texture from cache
		GetSubstanceCache().DeleteCachedTexture(material, textureName);

		SetDirty ();

		set<UnityGUID> assets;
		assets.insert(ObjectToGUID(&material));
		AssetInterface::Get().ImportAssets (assets, kForceUncompressedImport);
	}
}

ProceduralOutputType SubstanceImporter::GetTextureAlphaSource( ProceduralMaterial& material, std::string textureName )
{
	return GetTextureParameters(material, textureName).alphaSource;
}

void SubstanceImporter::OnTextureInformationsChanged(ProceduralTexture& texture)
{
	ProceduralMaterial* material = texture.GetSubstanceMaterial();
	if (!CheckPlayModeAndSkip(*material))
	{
		TextureImportSettings& textureSettings = GetTextureParameters(*material, texture.GetName());
		textureSettings.aniso = texture.GetAnisoLevel();
		textureSettings.filterMode = texture.GetFilterMode();
		textureSettings.wrapMode = texture.GetWrapMode();
		SetDirty();
	}
}

void SubstanceImporter::OnShaderModified(ProceduralMaterial& material)
{
	MaterialInstanceSettings& settings = FindOrCreateMaterialSettings( material.GetName() );
	bool hasShaderChanged(false);

	// Check if shader changed
	if (settings.shaderName!=material.GetShader()->GetName())
	{
		settings.shaderName = material.GetShader()->GetName();
		// clear old texture references
		material.GetSavedProperties ().CullUnusedProperties(material.GetShader()->GetParsedForm());
		BindTexturesToShader(material);
        hasShaderChanged = true;
	}

	if (m_IsPlaying)
	{
		m_ModifiedMaterials.insert(material.GetInstanceID());
		#if DEBUG_SUBSTANCE_IMPORTER
		WarningStringMsg("OnShaderModified: Adding %s to the list of modifed materials", material.GetName());
		#endif
		ApplyMaterialInformation(material, GetMaterialInformation(material));
		return;
	}

	// Save modified properties
	settings.materialInformation = GetMaterialInformation(material);
	settings.materialProperties = material.GetSavedProperties();
	ApplyMaterialSettings(material, settings);
	ApplyMaterialInformation(material, settings.materialInformation);
	SetDirty();

    if (hasShaderChanged)
    {
	    // Reimport asset (that's why we don't need to store the default values)
	    set<UnityGUID> assets;
	    assets.insert(ObjectToGUID(&material));
	    AssetInterface::Get().ImportAssets (assets, kForceUncompressedImport);
    }
}

void SubstanceImporter::ResetDefaultValues( ProceduralMaterial& material )
{
	SKIP_PLAYMODE();
	MaterialInstanceSettings* settings = FindMaterialInstanceSettings( material.GetName() );
	if ( settings == NULL )
		return;

	GetSubstanceCache().SafeClear();
	SetDirty();

	// Clear material settings
	settings->shaderName.clear();
	settings->inputs.clear();
	settings->materialInformation = ProceduralMaterialInformation ();
	settings->materialProperties = UnityPropertySheet();

	// Reimport asset (that's why we don't need to store the default values)
	set<UnityGUID> assets;
	assets.insert(ObjectToGUID(&material));
	AssetInterface::Get().ImportAssets (assets, kForceUncompressedImport);
}

bool SubstanceImporter::OnLoadSubstance(ProceduralMaterial& substance)
{
	BuildTargetPlatform platform = GetEditorUserBuildSettings().GetActiveBuildTarget();
    bool baked = !IsSubstanceSupportedOnPlatform(platform) || IsLoadingBehaviourBaked(substance.GetLoadingBehavior());

    if (m_IsPlaying)
		return !baked;

	bool requires_generation = !baked || (substance.GetSubstancePackage()!=NULL
		&& (substance.IsFlagEnabled(ProceduralMaterial::Flag_Import)
		|| substance.IsFlagEnabled(ProceduralMaterial::Flag_Uncompressed)));

	// Try to use the cached textures if generation is required
	if (requires_generation)
	{
		string path = FindAssetPathForSubstance(substance);
		GetSubstanceCache ().Lock();
		if (GetSubstanceCache ().assetPath==path)
		{
			GetSubstanceCache ().cachedIDs.insert(substance.GetInstanceID());
			requires_generation = false;
			for (ProceduralMaterial::Textures::iterator it=substance.GetTextures().begin();it!=substance.GetTextures().end();++it)
			{
				ProceduralTexture* texture = *it;
				if (texture==NULL)
				{
					requires_generation = true;
					continue;
				}
				std::map<string, SubstanceCache::CachedTexture>::iterator i =
					GetSubstanceCache ().textures.find(texture->GetName());
				if (i!=GetSubstanceCache ().textures.end())
				{
					SubstanceCache::CachedTexture& cached = i->second;
					if (cached.width==substance.GetWidth() && cached.height==substance.GetHeight()
						&& texture->GetSubstanceFormat()==cached.format)
					{
						texture->EnableFlag(ProceduralTexture::Flag_Cached, true);
						texture->UploadSubstanceTexture(cached.texture);

						if (substance.IsFlagEnabled(ProceduralMaterial::Flag_Import))
						{
							HandleBakingAndThumbnail(substance, *texture, cached.texture, cached.format, platform,
								cached.modified || !texture->GetBakedParameters().IsValid());
						}
						else
						{
							cached.modified = false;
						}
						continue;
					}
                    else
                    {
                        GetSubstanceCache ().textures.erase(i);
                    }
				}
				texture->GetTextureParameters().Invalidate();
				requires_generation = true;
			}
		}
		GetSubstanceCache ().Unlock();
	}
	return requires_generation;
}

void SubstanceImporter::OnTextureModified(SubstanceSystem::Substance& substance, ProceduralTexture& texture, SubstanceTexture& result)
{
    SKIP_PLAYMODE();

	string path = FindAssetPathForSubstance(*substance.material);
	GetSubstanceCache ().Lock();
    if (GetSubstanceCache ().assetPath!=path)
    {
        GetSubstanceCache ().Clear();
        GetSubstanceCache ().assetPath = path;
	}
    GetSubstanceCache ().cachedIDs.insert(substance.material->GetInstanceID());
    SubstanceCache::CachedTexture& cached = GetSubstanceCache ().textures[texture.GetName()];
    TextureFormat format = texture.GetDataFormat();
    int mips = result.mipmapCount==0?CalculateMipMapCount3D(result.level0Width, result.level0Height, 1):result.mipmapCount;
    size_t size = CalculateMipMapOffset(result.level0Width, result.level0Height, format, mips+1);
    if (size>cached.size)
    {
        if (cached.texture.buffer!=NULL)
            UNITY_FREE(kMemSubstance, cached.texture.buffer);
        cached.texture.buffer = UNITY_MALLOC(kMemSubstance, size);
    }
	BuildTargetPlatform platform = GetEditorUserBuildSettings().GetActiveBuildTarget();
	cached.width = substance.material->GetWidth();
	cached.height = substance.material->GetHeight();
    cached.format = format;
    cached.size = size;
    cached.texture.level0Width = result.level0Width;
    cached.texture.level0Height = result.level0Height;
    cached.texture.mipmapCount = result.mipmapCount;
    cached.texture.pixelFormat = result.pixelFormat;
    cached.texture.channelsOrder = result.channelsOrder;
    memcpy(cached.texture.buffer, result.buffer, size);

	if (substance.material->IsFlagEnabled(ProceduralMaterial::Flag_Import))
	{
		cached.modified = false;
		HandleBakingAndThumbnail(*substance.material, texture, result, format, platform);
	}
	else if (texture.HasBeenGenerated())
	{
		cached.modified = true;
	}
	GetSubstanceCache ().Unlock();
}

TextureFormat SubstanceImporter::GetSubstanceBakedFormat(SubstanceOutputFormat outputFormat) const
{
	BuildTargetPlatform platform = GetEditorUserBuildSettings().GetActiveBuildTarget();
	bool compressed = outputFormat==Substance_OFormat_Compressed;
	TextureFormat fmt = GetSubstanceTextureFormat(outputFormat, true);

    // Select the proper compression format when building for Android
    int compressedAndroidFormat = kTexFormatRGBA4444;
    if (platform == kBuild_Android)
    {
	    AndroidBuildSubtarget androidSubtarget = GetEditorUserBuildSettings().GetSelectedAndroidBuildSubtarget();
	    switch (androidSubtarget)
	    {
		    case kAndroidBuildSubtarget_DXT:   compressedAndroidFormat = kTexFormatDXT5; break;
		    case kAndroidBuildSubtarget_PVRTC: compressedAndroidFormat = kTexFormatPVRTC_RGBA4; break;
		    case kAndroidBuildSubtarget_ATC:   compressedAndroidFormat = kTexFormatATC_RGBA8; break;
		    // Cannot use ETC compression as the default as the
		    // substance texture might make use of the alpha channel...
		    default:
		    case kAndroidBuildSubtarget_Generic:
		    case kAndroidBuildSubtarget_ETC:   compressedAndroidFormat = kTexFormatRGBA4444; break;
	    }
    }

    switch (platform)
    {
	    case kBuild_iPhone:			fmt = compressed? kTexFormatPVRTC_RGBA4 : kTexFormatRGBA32; break;
	    case kBuildPS3:
	    case kBuildXBOX360:			fmt = compressed? kTexFormatDXT5 : kTexFormatRGBA32; break;
	    case kBuild_Android:		fmt = compressed? compressedAndroidFormat : kTexFormatRGBA32; break;
	    case kBuildFlash:			fmt = compressed? kTexFormatFlashATF_RGBA_JPG : kTexFormatRGBA32; break;
		default: break;
	}
	return fmt;
}

void SubstanceImporter::HandleBakingAndThumbnail(ProceduralMaterial& substance, ProceduralTexture& texture, SubstanceTexture& result, TextureFormat format, BuildTargetPlatform targetPlatform, bool doBaking)
{
    // Retrieve raw texture
    Image rawTexture(result.level0Width, result.level0Height,
		format==kTexFormatDXT5 ? (TextureFormat)kTexFormatRGBA32:format);
    if (format==kTexFormatDXT5)
    {
	    // Decompress into upper multiple-of-4 size
	    int decompWidth = (result.level0Width + 3) / 4 * 4;
	    int decompHeight = (result.level0Height + 3) / 4 * 4;
	    Image decompressed( decompWidth, decompHeight, kTexFormatRGBA32 );
	    UInt32* compressed = static_cast<UInt32*>(result.buffer);
	    DecompressNativeTextureFormat( kTexFormatDXT5, result.level0Width, result.level0Height,
								      compressed, decompWidth, decompHeight, (UInt32*)decompressed.GetImageData() );

	    // Clip this back to original size
	    ImageReference clipped = decompressed.ClipImage( 0, 0, result.level0Width, result.level0Height );
	    rawTexture.BlitImage( clipped, ImageReference::BLIT_COPY );
    }
    else
    {
	    memcpy(rawTexture.GetImageData(), result.buffer, result.level0Width*result.level0Height*4);
    }

	SubstanceImporter* importer = FindImporterForSubstance(substance);
    if (importer!=NULL)
    {
	    // Set thumbnail
	    Image thumbnail(128, 128, kTexFormatRGBA32);
	    thumbnail.BlitImage( rawTexture, ImageReference::BLIT_BILINEAR_SCALE );
		thumbnail.ClearImage(ColorRGBA32(0, 0, 0, 255), ImageReference::CLEAR_ALPHA);
	    importer->SetThumbnail(thumbnail, texture.GetInstanceID());

	    // Bake texture if required
		if (substance.IsFlagEnabled(ProceduralMaterial::Flag_Uncompressed) || 
		    (IsSubstanceSupportedOnPlatform(targetPlatform) && !IsLoadingBehaviourBaked(substance.GetLoadingBehavior())))
	    {
		    texture.GetBakedData().clear();
		    texture.GetBakedParameters().Invalidate();
	    }
	    else if (doBaking)
	    {
			// Set baked format
		    BuildTargetPlatform platform = GetEditorUserBuildSettings().GetActiveBuildTarget();
			SubstanceBuildTargetSettings sgs = importer->GetPlatformSettings(substance.GetName(), targetPlatform);
			TextureFormat fmt = importer->GetSubstanceBakedFormat((SubstanceOutputFormat)sgs.m_TextureFormat);

			switch (platform)
			{
				case kBuildWii:
				case kBuildPS3:
				case kBuildXBOX360:
				case kBuildFlash: break;
				default:
				{
					// Can we just reuse the editor (DTX5) data ?
					if (fmt == kTexFormatDXT5)
					{
						texture.GetBakedParameters().width = result.level0Width;
						texture.GetBakedParameters().height = result.level0Height;
						texture.GetBakedParameters().textureFormat = format;
						int mips = result.mipmapCount;
						if (result.mipmapCount==0) mips = CalculateMipMapCount3D(result.level0Width, result.level0Height,1);
						texture.GetBakedParameters().mipLevels = mips;
						int size = CalculateMipMapOffset(result.level0Width, result.level0Height, format, mips+1);
						texture.GetBakedData().resize(size);
						memcpy(&texture.GetBakedData()[0], result.buffer, size);
						return;
					}
				}
			}

		    // Check size requirements
		    int bakedWidth = pow(2.0f, substance.GetWidth());
		    int bakedHeight = pow(2.0f, substance.GetHeight());

			// Handle iOS non-square unsupported PVRTC
			if (platform==kBuild_iPhone && fmt==kTexFormatPVRTC_RGBA4)
			{
				if (bakedWidth>bakedHeight) bakedHeight = bakedWidth;
				else bakedWidth = bakedHeight;
			}

		    // Initialize baked texture
		    rawTexture.ReformatImage(bakedWidth, bakedHeight, rawTexture.GetFormat(), ImageReference::BLIT_BILINEAR_SCALE);
		    texture.GetBakedParameters().width = bakedWidth;
		    texture.GetBakedParameters().height = bakedHeight;
		    texture.GetBakedParameters().textureFormat = kTexFormatRGBA32;
			texture.GetBakedParameters().mipLevels = CalculateMipMapCount3D(bakedWidth, bakedHeight,1);
		    texture.GetBakedData().resize(0);

		    // Generate mipmaps
		    for (int i=0;i<texture.GetBakedParameters().mipLevels;++i)
		    {
			    size_t size = rawTexture.GetWidth()*rawTexture.GetHeight()*4;
			    size_t offset = texture.GetBakedData().size();
			    texture.GetBakedData().resize(texture.GetBakedData().size()+size);
			    UInt8* bakedBuffer = &texture.GetBakedData()[offset];

		        Image highRes(rawTexture.GetWidth(), rawTexture.GetHeight(), kTexFormatRGBA32);
			    highRes.BlitImage( rawTexture, ImageReference::BLIT_COPY );
			    memcpy(bakedBuffer, highRes.GetImageData(), size);

			    // Next mipmap
			    rawTexture.ReformatImage(max(rawTexture.GetWidth()/2, 1), max(rawTexture.GetHeight()/2, 1),
									     rawTexture.GetFormat(), ImageReference::BLIT_BILINEAR_SCALE);
		    }

		    // Compress if required
		    if (fmt!=kTexFormatRGBA32)
		    {
			    Texture2D* tempTexture = CreateObjectFromCode<Texture2D>(kDefaultAwakeFromLoad);;
			    tempTexture->AwakeFromLoad(kDefaultAwakeFromLoad);
			    UInt8* data = tempTexture->AllocateTextureData(texture.GetBakedData().size(), kTexFormatRGBA32);
			    memcpy(data, &texture.GetBakedData()[0], texture.GetBakedData().size());
			    // ARGB4444 (and probably RGBA4444) compression does not like RGBA32 data
				if (fmt==kTexFormatRGBA4444)
				{
					UInt8* p = data;
					for (unsigned int i=0 ; i<texture.GetBakedData().size() ; i+=4)
					{
						UInt8 r = p[0];
						p[0] = p[3];
						p[3] = p[2];
						p[2] = p[1];
						p[1] = r;
						p+=4;
					}
				}
			    tempTexture->InitTextureInternal(bakedWidth, bakedHeight,
					fmt==kTexFormatRGBA4444?kTexFormatARGB32:kTexFormatRGBA32,
				    texture.GetBakedData().size(), data, Texture2D::kMipmapMask, 1);
			    CompressTexture(*tempTexture, fmt);
			    texture.GetBakedData().resize(tempTexture->GetStorageMemorySize());
			    if (tempTexture->GetRawImageData()==NULL)
			    {
				    ErrorString("Substance baking unexpected error - failed to compress texture");
				    return;
			    }
			    memcpy(&texture.GetBakedData()[0], tempTexture->GetRawImageData(), texture.GetBakedData().size());
			    texture.GetBakedParameters().textureFormat = tempTexture->GetTextureFormat();
			    DestroySingleObject(tempTexture);
		    }
	    }
    }
}

void SubstanceImporter::OnInputModified(ProceduralMaterial& substance, std::string inputName, SubstanceValue& inputValue)
{
	SKIP_PLAYMODE();

	SubstanceImporter* importer = FindImporterForSubstance(substance);
	if (importer!=NULL)
	{
		InputImportSettings& setting = importer->FindOrCreateInputImportSetting (substance, inputName);
		memcpy(setting.value.scalar, inputValue.scalar, sizeof(float) * 4);
		setting.value.texture = inputValue.texture;
		importer->SetDirty();
	}
}

SubstanceImporter* SubstanceImporter::FindImporterForSubstance(ProceduralMaterial& substance)
{
	SubstanceImporter* importer(NULL);
	if (substance.GetSubstancePackage()!=NULL)
	{
		importer = static_cast<SubstanceImporter*>(FindAssetImporterForObject(substance.GetSubstancePackage()->GetInstanceID()));

		// @TODO: This is very ugly. Can we instead of abusing static variables, pass the SubstanceImporter down the various generation functions.???

		if (importer==NULL && m_CurrentImporter!=NULL && substance.GetSubstancePackage()->GetInstanceID()==m_CurrentInstance)
			return m_CurrentImporter;
	}
	return importer;
}

std::string SubstanceImporter::FindAssetPathForSubstance(ProceduralMaterial& substance)
{
	string path = GetPersistentManager ().GetPathName (substance.GetInstanceID());
	if (path=="")
	{
		SubstanceImporter* importer = FindImporterForSubstance(substance);
		if (importer!=NULL)
			path = GetPersistentManager ().GetPathName (importer->GetInstanceID());
	}
	AssertIf (path=="");
	return path;
}

void SubstanceImporter::OnSubstanceModified(SubstanceSystem::Substance& substance)
{
	bool awake = substance.material->IsFlagEnabled(ProceduralMaterial::Flag_Awake);
	if (awake && !substance.material->IsFlagEnabled(ProceduralMaterial::Flag_Import))
		substance.material->EnableFlag(ProceduralMaterial::Flag_Awake, false);
	if (!CheckPlayModeAndSkip(*substance.material))
	{
		if (substance.material->IsFlagEnabled(ProceduralMaterial::Flag_Import))
		{
			substance.material->EnableFlag(ProceduralMaterial::Flag_Import, false);
			return;
		}

		
		// Fix for case 570654. Below code was removed.
		// The substance importer is being dirtied even though the user has not changed
		// anything in the importer.
		// This leads to a crash due to a reimport during drag and drop, which is triggered
		// because the ProceduralMaterial Inspector triggers a reimport when the inspector is
		// closed.
		
//		SubstanceImporter* importer = FindImporterForSubstance(*substance.material);
//		if (importer!=NULL && !awake)
//			importer->SetDirty();
	}
}

std::string SubstanceImporter::InstanciatePrototype( std::string prototypeName )
{
	// Check the prototype is valid
	vector<string> prototypes = GetSubstanceNames();
	vector<string>::iterator it = std::find(prototypes.begin(),prototypes.end(),prototypeName);
	if (it==prototypes.end())
		return "";

	// Remove it from the deleted list
	set<UnityStr>::iterator i = std::find(m_DeletedPrototypes.begin(), m_DeletedPrototypes.end(), prototypeName);
	if (i!=m_DeletedPrototypes.end())
		m_DeletedPrototypes.erase(i);

	// Generate instance name
	std::string instanceName(GenerateUniqueName(prototypeName, prototypes));

	// Create material instance
	FindOrCreateMaterialSettings(instanceName, prototypeName);
	SetDirty();
	return instanceName;
}

void SubstanceImporter::DeleteMaterialInstance( ProceduralMaterial& material )
{
	// Simply erase the instance settings, a re-import will simply miss to recreate this instance...
	std::vector<MaterialInstanceSettings>::iterator matInstEnd = m_MaterialInstances.end();
	
	std::vector<MaterialImportOutput>::iterator itImp  = m_MaterialImportOutputs.begin();
	
	for (std::vector<MaterialInstanceSettings>::iterator itInst = m_MaterialInstances.begin() ; itInst != matInstEnd; ++itInst, ++itImp)
	{
		if ( itInst->name == material.GetName() )
		{
			std::string prototypeName = itInst->prototypeName;

			m_MaterialInstances.erase    ( itInst );
			m_MaterialImportOutputs.erase( itImp  );

			// Do we still have at least one instance of this prototype or can we skip its generation altogether?
			if (FindPrototypeSettings(prototypeName)==NULL)
				m_DeletedPrototypes.insert(prototypeName);

            // Remove cached textures
			GetSubstanceCache().DeleteCachedTextures(material);

			SetDirty();
			return;
		}
	}
}

bool SubstanceImporter::RenameMaterialInstance( ProceduralMaterial& material, std::string name )
{
	MaterialInstanceSettings* settings = FindMaterialInstanceSettings( material.GetName() );
	if (settings==NULL)
	{
        ErrorString("Failed to rename ProceduralMaterial: the material is invalid");
        return false;
	}
    else if (FindMaterialInstanceSettings( name )!=NULL)
    {
	    ErrorString("Failed to rename ProceduralMaterial: name is already assigned");
        return false;
	}
	else if (settings->name != name)
	{
		// Erase textures with the old name from the Substance Cache (case 558822)
		GetSubstanceCache().DeleteCachedTextures(material);

		settings->materialProperties.m_TexEnvs.clear();

		// Rename the pre-existing TextureImportSettings, otherwise the textureParameters will end up with
		// a lot of invalid texture names, which will end up causing problems when being serialized.
		// This is the "let's not cause this anymore" part of the fix for case 567991.
		std::vector<TextureImportSettings>& texParams = settings->textureParameters;
		for (std::vector<TextureImportSettings>::iterator it_texParam = texParams.begin() ; it_texParam != texParams.end() ; ++it_texParam)
		{
			std::string texName = it_texParam->name.c_str() + settings->name.size();
			it_texParam->name = name + texName;
		}

		settings->name = name;
		SetDirty();
	}
    return true;
}

vector<ProceduralMaterial*> SubstanceImporter::GetImportedMaterials( )
{
	vector<Object*> objects = FindAllAssetsAtPath(GetAssetPathName(), ClassID(ProceduralMaterial));
    return *reinterpret_cast<vector<ProceduralMaterial*>* > (&objects);
}

string SubstanceImporter::GenerateUniqueName (const string& name)
{
	return GenerateUniqueName(name, GetSubstanceNames());
}

string SubstanceImporter::GenerateUniqueName (const std::string& name, const std::vector<std::string> &prototypes)
{
	// Get existing Substance instance names
	set<std::string> forbidden;
	vector<MaterialInstanceSettings>::iterator end = m_MaterialInstances.end();
	for (vector<MaterialInstanceSettings>::iterator	i = m_MaterialInstances.begin(); i != end; ++i )
		forbidden.insert(ToLower(i->name));

	// Check if the name is the name of a prototype
	vector<std::string>::const_iterator it = std::find(prototypes.begin(), prototypes.end(), name);
	if (it!=prototypes.end())
	{
		// If it is the name of a prototype, check if an instance already has this name
		for (vector<MaterialInstanceSettings>::iterator	i = m_MaterialInstances.begin(); i != end; ++i )
		{
			if (i->name == name)
			{
				// Another instance already has this name.
				// Add underscore and 1.
				// Since substance prototypes can have names ending in numbers (e.g. bricks_008)
				// we need to differentiate instance numbering from prototype numbering.
				// So if an instance bricks_008 already exists, a clone of it should not be
				// bricks_009 but rather bricks_008_1. Next should be bricks_008_2 and so on.
				return GenerateUniqueName (name + "_1", &forbidden);
			}
		}
	}

	return GenerateUniqueName (name, &forbidden);
}

// TODO: This function is completely general-purpose and should be moved into some tools library. Where?
// Generalized from GUIDPersistentManager::GenerateUniqueAssetPathName
string SubstanceImporter::GenerateUniqueName (const string& name, const std::set<string>* disallowedNames)
{
	string base = DeletePathNameExtension(name);
	string ext = GetPathNameExtension(name);
	string dot = (base == name?"":"."); // Don't add dot if there was no dot to begin with

	// detect if base ends with a number, strip it off and begin counting from there.
	int initial = 0;
	string::size_type nonNum = base.find_last_not_of ("0123456789");
	string numPad = ""; // Used to keep any zero-padding if already there

	if(base.size() - nonNum  > 9) // we only care about the last 8 digits
		nonNum = base.size()-1;

	if (nonNum != string::npos && nonNum >= base.size()-1) { // the name does not end with a number
		if(base[base.size()-1] != ' ') base+=" "; // add a space to make it look nicer if the name doesn't end with a space
	}
	else { // the name ends with a number
		numPad = "0" + IntToString(base.size() - nonNum - 1);
		initial = StringToInt(base.substr(nonNum + 1));
		base = base.substr(0,nonNum+1);
	}

	// Generate a custom format string for inserting the number into the file name
	string nameTemplate =  "%s%" + numPad +"d" + dot + "%s";

	// Create unique name
	for (int i=initial;i<initial+5000;i++)
	{
		string renamed;
		if (i == 0)
			renamed = name;
		else
			renamed = Format(nameTemplate.c_str(), base.c_str(), i, ext.c_str() );

		// Skip names explicitly disallowed by the caller
		if( disallowedNames != NULL && disallowedNames->find(ToLower(renamed)) != disallowedNames->end() )
			continue;

		return renamed;
	}

	return string();
}

// Clones an existing material instance
//	_Source, the source material instance to clone
std::string SubstanceImporter::CloneMaterialInstance( const ProceduralMaterial& _Source )
{
	const MaterialInstanceSettings*	sourceSettings = FindMaterialInstanceSettings(_Source.GetName());
	if (sourceSettings==NULL)
		return "";

	// Get a new instance name
	std::string cloneName(GenerateUniqueName(_Source.GetName()));
	if (FindMaterialInstanceSettings(cloneName))
	{
		char name[256];
		int instanceIndex(0);
		do
		{
			sprintf(name, "%s_instance_%d", _Source.GetName(), ++instanceIndex);
		}
		while(FindMaterialInstanceSettings(name));
		cloneName = name;
	}

	// Generate settings
	MaterialInstanceSettings settings;
	settings.name = cloneName;
	settings.prototypeName = sourceSettings->prototypeName;
	settings.shaderName = sourceSettings->shaderName;
	settings.inputs = sourceSettings->inputs;

	settings.textureParameters = sourceSettings->textureParameters;

	for (std::vector<TextureImportSettings>::iterator it=settings.textureParameters.begin();it!=settings.textureParameters.end();++it)
	{
		UnityStr textureName = it->name.c_str() + sourceSettings->name.size();
		it->name = settings.name + textureName;
	}

	settings.buildTargetSettings = sourceSettings->buildTargetSettings;

	// Material properties
	settings.materialProperties.m_Floats = sourceSettings->materialProperties.m_Floats;
	settings.materialProperties.m_Colors = sourceSettings->materialProperties.m_Colors;

	settings.materialInformation = sourceSettings->materialInformation;

	m_MaterialInstances.push_back(settings);
    SetDirty();
	return cloneName;
}

MaterialInstanceSettings* SubstanceImporter::FindMaterialInstanceSettings( const std::string& name )
{
	for (vector<MaterialInstanceSettings>::iterator it=m_MaterialInstances.begin() ; it!=m_MaterialInstances.end(); ++it)
	{
		if (it->name==name)
			return &*it;
	}
	return	NULL;
}

MaterialInstanceSettings* SubstanceImporter::FindPrototypeSettings( const std::string& name )
{
	for (vector<MaterialInstanceSettings>::iterator it=m_MaterialInstances.begin() ; it!=m_MaterialInstances.end(); ++it)
	{
		if (it->prototypeName==name)
			return &*it;
	}
	return	NULL;
}

MaterialInstanceSettings& SubstanceImporter::FindOrCreateMaterialSettings( const std::string& name, std::string prototypeName )
{
	MaterialInstanceSettings* settings = FindMaterialInstanceSettings(name);
	if (settings)
		return *settings;
	else
	{
		if (prototypeName.size()==0)
		{
			ErrorString("SubstanceImporter unexpected error");
		}

		MaterialInstanceSettings settings;
		settings.name = name;
		settings.prototypeName = prototypeName;
		settings.materialInformation.m_GenerateAllOutputs = 0;
		settings.materialInformation.m_AnimationUpdateRate = 0;
		m_MaterialInstances.push_back(settings);
		return m_MaterialInstances.back();
	}
}

TextureImportSettings* SubstanceImporter::FindTextureSettings(ProceduralMaterial& material, MaterialInstanceSettings& materialSettings, std::string textureName)
{
	for (std::vector<TextureImportSettings>::iterator it=materialSettings.textureParameters.begin();
		 it!=materialSettings.textureParameters.end();++it)
	{
		if (it->name==textureName)
		{
			return &*it;
		}
	}
	return NULL;
}

TextureImportSettings& SubstanceImporter::FindOrCreateTextureSettings(ProceduralMaterial& material, MaterialInstanceSettings& materialSettings, std::string textureName)
{
	// Find if existing
	TextureImportSettings* textureSettings = FindTextureSettings(material, materialSettings, textureName);
	if (textureSettings!=NULL)
		return *textureSettings;

	// else create a new one
	TextureImportSettings newSettings;
	newSettings.name = textureName;

	// Check if its diffuse (binded to specular by default)
	for (ProceduralMaterial::Textures::const_iterator i=material.GetTextures().begin();i!=material.GetTextures().end();++i)
	{
		if ((*i)->GetName()==textureName)
		{
			if ((*i)->GetType()==Substance_OType_Diffuse)
			{
				newSettings.alphaSource = Substance_OType_Specular;
			}
			break;
		}
	}

	materialSettings.textureParameters.push_back(newSettings);
	return *materialSettings.textureParameters.rbegin();
}

InputImportSettings* SubstanceImporter::FindMaterialInputImportSetting( const std::string& materialName, const string& name )
{
	MaterialInstanceSettings* settings = FindMaterialInstanceSettings (materialName);
	if (!settings)
		return NULL;
	int index = FindInputImportSetting(settings->inputs, name);
	if (index != -1)
		return &settings->inputs[index];
	return NULL;
}

InputImportSettings& SubstanceImporter::FindOrCreateInputImportSetting( ProceduralMaterial& material, const string& name )
{
	MaterialInstanceSettings& settings = FindOrCreateMaterialSettings (material.GetName());
	int index = FindInputImportSetting(settings.inputs, name);
	if (index != -1)
		return settings.inputs[index];
	else
	{
		settings.inputs.push_back(InputImportSettings());
		InputImportSettings& result = settings.inputs.back();
		result.name = name;
		return result;
	}
}

std::set<int> SubstanceImporter::m_ModifiedMaterials;
std::set<ProceduralMaterial*> SubstanceImporter::m_MaterialsAddedToTheScene;
bool SubstanceImporter::m_IsPlaying = false;
SubstanceImporter* SubstanceImporter::m_CurrentImporter = NULL;
int SubstanceImporter::m_CurrentInstance = 0;

void SubstanceImporter::OnEnterPlaymode()
{
	GetSubstanceCache ().SafeClear(true);
	GetSubstanceSystem().SetProcessorUsage(ProceduralProcessorUsage_One);
	m_IsPlaying = true;
	m_MaterialsAddedToTheScene.clear();
}

bool SubstanceImporter::CheckPlayModeAndSkip(ProceduralMaterial& material)
{
	if (m_IsPlaying)
	{
		if (!material.IsFlagEnabled(ProceduralMaterial::Flag_Clone)
		    && !material.IsFlagEnabled(ProceduralMaterial::Flag_Awake))
		{
			// Do not treat the substance as modified (= will trigger a reimport on exiting playmode)
			// if it is the first time it is loaded and generated. Otherwise, substances assigned
			// to prefabs included in scenes that are loaded via LoadLevel (i.e. not the first one) will
			// be inserted in that list and then needlessly reimported when the user exits playmode, even
			// if they are just loaded once and not modified.
			if (material.m_isAlreadyLoadedInCurrentScene)
			{
				#if DEBUG_SUBSTANCE_IMPORTER
				WarningStringMsg("CheckPlayModeAndSkip: Adding %s to the list of modified materials", material.GetName());
				#endif
				m_ModifiedMaterials.insert(material.GetInstanceID());
			}

			// We still need to keep track of the materials that were added to the scene in playmode so that
			// even if they were not modified, the "import/do not import" setting does not flip-flop between
			// enter/leave/enter/leave playmode sequences
			m_MaterialsAddedToTheScene.insert(&material);

			// This is reset to "false" in OnLeavePlayMode
			material.m_isAlreadyLoadedInCurrentScene = true;
		}
		return true;
	}
	return false;
}

void SubstanceImporter::OnLeavePlaymode()
{
	// Make sure all substances are sleeping
	GetSubstanceSystem().WaitFinished();

	m_IsPlaying = false;
	
	// Multithread the Substance processing before reimporting after leaving playmode
	GetSubstanceSystem().SetProcessorUsage(ProceduralProcessorUsage_All);

	#if DEBUG_SUBSTANCE_IMPORTER
	WarningStringMsg("OnLeavePlayMode: Reimporting %d materials", m_ModifiedMaterials.size());
	#endif

	set<UnityGUID> assets;
	for (std::set<int>::iterator i=m_ModifiedMaterials.begin(); i!=m_ModifiedMaterials.end(); ++i)
	{
		PPtr<ProceduralMaterial> material(*i);
		if (material.IsValid())
		{
			assets.insert(ObjectToGUID(&*material));
		}
	}

	AssetInterface::Get().ImportAssets (assets);

	// Reset the isAlreadyLoaded flag
	for (std::set<ProceduralMaterial*>::iterator i=m_MaterialsAddedToTheScene.begin() ;
	     i != m_MaterialsAddedToTheScene.end() ; ++i)
	{
		PPtr<ProceduralMaterial> material(*i);
		if (material.IsValid())
		{
			material->m_isAlreadyLoadedInCurrentScene = false;
		}
	}

	// Back to single-threaded operation
	GetSubstanceSystem().SetProcessorUsage(ProceduralProcessorUsage_One);

	// Clear the modified list
	m_ModifiedMaterials.clear();
}

/// Called after a meta data file has been seen to modify the import settings.
void SubstanceImporter::ApplyImportSettingsDeprecated( const YAMLMapping* settings )
{
	YAMLScalar* importerVersion = dynamic_cast<YAMLScalar*>( settings->Get("importerVersion") );
	if (!importerVersion || int(*importerVersion) > kSubstanceImporterVersion)
	{
		ErrorString("Unknown Substance importer version");
		return;
	}

	// Material instances
	YAMLMapping* instanceSettings = dynamic_cast<YAMLMapping*>( settings->Get("materials") );
	if (!instanceSettings)
	{
		ErrorString("Failed to read Substance materials");
		return;
	}
	for (YAMLMapping::const_iterator it=instanceSettings->begin();it!=instanceSettings->end();++it)
	{
		std::string materialName = *it->first;
		YAMLMapping* materialSettings = dynamic_cast<YAMLMapping*>(it->second);
		if (!materialSettings)
		{
			printf_console("Failed to read Substance material : %s\n", materialName.c_str());
			continue;
		}
		MaterialInstanceSettings * material = FindMaterialInstanceSettings(materialName);
		if (!material)
		{
			printf_console("Removed Substance material : %s\n", materialName.c_str());
			continue;
		}

		YAMLScalar* prototypeName = dynamic_cast<YAMLScalar*>( materialSettings->Get("prototypeName") );
		if (prototypeName) material->prototypeName = (std::string)(*prototypeName);
		YAMLScalar* shaderName = dynamic_cast<YAMLScalar*>( materialSettings->Get("shaderName") );
		if (shaderName) material->shaderName = (std::string)(*shaderName);

		// Inputs
		YAMLMapping* inputsSettings = dynamic_cast<YAMLMapping*>( materialSettings->Get("inputs") );
		if (!inputsSettings)
		{
			printf_console("Failed to read Substance material inputs : %s\n", materialName.c_str());
			continue;
		}
		for (YAMLMapping::const_iterator i=inputsSettings->begin();i!=inputsSettings->end();++i)
		{
			std::string inputName = *i->first;
			YAMLMapping* inputSettings = dynamic_cast<YAMLMapping*>(i->second);
			if (!inputSettings)
			{
				printf_console("Failed to read Substance material input : %s : %s\n", materialName.c_str(), inputName.c_str());
				continue;
			}
			InputImportSettings* input = FindMaterialInputImportSetting(materialName, inputName);
			if (!input)
			{
				printf_console("Removed Substance material input : %s : %s\n", materialName.c_str(), inputName.c_str());
				continue;
			}

			// Value
			YAMLScalar* floatValue = dynamic_cast<YAMLScalar*>( inputSettings->Get("scalar") );
			YAMLMapping* textureValue = dynamic_cast<YAMLMapping*>( inputSettings->Get("texture") );
			if (floatValue)
			{
				if (sscanf(std::string(*floatValue).c_str(), "%f %f %f %f", input->value.scalar+0, input->value.scalar+1, input->value.scalar+2, input->value.scalar+3)!=4)
				{
					printf_console("Invalid Substance material input : %s : %s\n", materialName.c_str(), inputName.c_str());
				}
			}
			else if (textureValue)
			{
				if (!textureValue->GetPPtr().IsValid())
				{
					printf_console("Invalid Substance material input : %s : %s\n", materialName.c_str(), inputName.c_str());
				}
				else
				{
					input->value.texture = dynamic_pptr_cast<Texture2D*> (textureValue->GetPPtr());
				}
			}
		}

		// Material Information
		YAMLMapping* informationSettings = dynamic_cast<YAMLMapping*>( materialSettings->Get("informations") );
		if (!informationSettings)
		{
			printf_console("Failed to read Substance material informations : %s\n", materialName.c_str());
			continue;
		}
		YAMLScalar* offsetValue = dynamic_cast<YAMLScalar*>( informationSettings->Get("offset") );
		if (!offsetValue || sscanf(std::string(*offsetValue).c_str(), "%f %f", &material->materialInformation.m_Offset.x, &material->materialInformation.m_Offset.y)!=2)
		{
			printf_console("Failed to read Substance material offset : %s\n", materialName.c_str());
		}
		YAMLScalar* scaleValue = dynamic_cast<YAMLScalar*>( informationSettings->Get("scale") );
		if (!scaleValue || sscanf(std::string(*scaleValue).c_str(), "%f %f", &material->materialInformation.m_Scale.x, &material->materialInformation.m_Scale.y)!=2)
		{
			printf_console("Failed to read Substance material scale : %s\n", materialName.c_str());
		}
		int m_GeneratedAtLoading(1);
		YAMLScalar* loadValue = dynamic_cast<YAMLScalar*>( informationSettings->Get("flags") );
		if (loadValue) sscanf(std::string(*loadValue).c_str(), "%d", &m_GeneratedAtLoading);

		YAMLScalar* allValue = dynamic_cast<YAMLScalar*>( informationSettings->Get("allValue") );
		if (!allValue || sscanf(std::string(*allValue).c_str(), "%d", &material->materialInformation.m_GenerateAllOutputs)!=1)
		{
			printf_console("Failed to read Substance material m_GenerateAllOutputs : %s\n", materialName.c_str());
		}
		YAMLScalar* animateValue = dynamic_cast<YAMLScalar*>( informationSettings->Get("animation_update_rate") );
		if (!animateValue || sscanf(std::string(*animateValue).c_str(), "%d", &material->materialInformation.m_AnimationUpdateRate)!=1)
		{
			printf_console("Failed to read Substance material m_AnimationUpdateRate : %s\n", materialName.c_str());
		}

		// Material Properties
		YAMLMapping* propertiesSettings = dynamic_cast<YAMLMapping*>( materialSettings->Get("properties") );
		if (!propertiesSettings)
		{
			printf_console("Failed to read Substance material properties : %s\n", materialName.c_str());
			continue;
		}
		for (YAMLMapping::const_iterator i=propertiesSettings->begin();i!=propertiesSettings->end();++i)
		{
			std::string propertyName = *i->first;
			YAMLScalar* propertyValue = dynamic_cast<YAMLScalar*>( i->second );
			YAMLMapping* propertyTextureValue = dynamic_cast<YAMLMapping*>( i->second );
			ShaderLab::FastPropertyName propName;
			propName.SetName(propertyName.c_str());
			if (propertyValue)
			{
				float value[4];
				int count = sscanf(std::string(*propertyValue).c_str(), "%f %f %f %f", value+0, value+1, value+2, value+3);
				if (count==1)
				{
					material->materialProperties.m_Floats[propName] = value[0];
				}
				else if (count==4)
				{
					material->materialProperties.m_Colors[propName] = ColorRGBAf(value);
				}
				else
				{
					printf_console("Invalid Substance material property scalar : %s : %s\n", materialName.c_str(), propertyName.c_str());
				}
			}
			else if (propertyTextureValue)
			{
				YAMLMapping* textureValue = dynamic_cast<YAMLMapping*>( propertyTextureValue->Get("texture") );
				YAMLScalar* offsetValue = dynamic_cast<YAMLScalar*>( propertyTextureValue->Get("offset") );
				YAMLScalar* scaleValue = dynamic_cast<YAMLScalar*>( propertyTextureValue->Get("scale") );
				if (!textureValue || !offsetValue || !scaleValue)
				{
					printf_console("Invalid Substance material property texture : %s : %s\n", materialName.c_str(), propertyName.c_str());
				}
				else
				{
					UnityPropertySheet::UnityTexEnv texEnv;
					if (textureValue->GetPPtr().IsValid())
					{
						texEnv.m_Texture = dynamic_cast<Texture2D*>(&*textureValue->GetPPtr());
					}
					if (sscanf(std::string(*offsetValue).c_str(), "%f %f", &texEnv.m_Offset.x, &texEnv.m_Offset.y)!=2
						|| sscanf(std::string(*scaleValue).c_str(), "%f %f", &texEnv.m_Scale.x, &texEnv.m_Scale.y)!=2)
					{
						printf_console("Invalid Substance material property texture : %s : %s\n", materialName.c_str(), propertyName.c_str());
					}
					material->materialProperties.m_TexEnvs[propName] = texEnv;
				}
			}
			else
			{
				printf_console("Failed to read Substance material property : %s : %s\n", materialName.c_str(), propertyName.c_str());
			}
		}

		// Texture settings
		YAMLMapping* textureSettings = dynamic_cast<YAMLMapping*>( materialSettings->Get("textures") );
		if (!textureSettings)
		{
			continue;
		}
		material->textureParameters.clear();
		for (YAMLMapping::const_iterator i=textureSettings->begin();i!=textureSettings->end();++i)
		{
			std::string textureName = *i->first;
			YAMLMapping* textureValue = dynamic_cast<YAMLMapping*>( i->second );
			if (!textureValue)
			{
				printf_console("Failed to read Substance texture setting : %s\n", textureName.c_str());
				continue;
			}
			TextureImportSettings textureSetting;
			textureSetting.name = textureName;
			YAMLScalar* alphaSourceValue = dynamic_cast<YAMLScalar*>( textureValue->Get("alphaSource") );
			if (alphaSourceValue) textureSetting.alphaSource = (ProceduralOutputType)((int)*alphaSourceValue);
			YAMLScalar* filterModeValue = dynamic_cast<YAMLScalar*>( textureValue->Get("filterMode") );
			if (filterModeValue) textureSetting.filterMode = (int)*filterModeValue;
			YAMLScalar* anisoValue = dynamic_cast<YAMLScalar*>( textureValue->Get("aniso") );
			if (anisoValue) textureSetting.aniso = (int)*anisoValue;
			YAMLScalar* wrapModeValue = dynamic_cast<YAMLScalar*>( textureValue->Get("wrapMode") );
			if (wrapModeValue) textureSetting.wrapMode = (int)*wrapModeValue;
			material->textureParameters.push_back(textureSetting);
		}

		// Build Settings
		YAMLMapping* buildSettings = dynamic_cast<YAMLMapping*>( materialSettings->Get("buildSettings") );
		if (!buildSettings)
		{
			ErrorString("Failed to read Substance buildSettings");
			return;
		}

		int loadBehavior = m_GeneratedAtLoading==1?ProceduralLoadingBehavior_Generate:ProceduralLoadingBehavior_None;
		for (YAMLMapping::const_iterator i=buildSettings->begin();i!=buildSettings->end();++i)
		{
			std::string platformName = *i->first;
			YAMLMapping* platformValue = dynamic_cast<YAMLMapping*>( i->second );
			if (!platformValue)
			{
				printf_console("Failed to read Substance material build setting : %s\n", platformName.c_str());
				continue;
			}
			YAMLScalar* maxTextureSizeValue = dynamic_cast<YAMLScalar*>( platformValue->Get("maxTextureSize") );
			YAMLScalar* textureFormatValue = dynamic_cast<YAMLScalar*>( platformValue->Get("textureFormat") );
			if (!maxTextureSizeValue || !textureFormatValue)
			{
				printf_console("Failed to read Substance build setting : %s\n", platformName.c_str());
			}
			else
			{
				SetPlatformTextureSettings(materialName, platformName, *maxTextureSizeValue, *maxTextureSizeValue, *textureFormatValue, loadBehavior);
			}
		}
	}
}

bool SubstanceImporter::DoesAssetNeedReimport (const string& assetPath, BuildTargetPlatform targetPlatform, bool unload, bool requireCompressedAssets)
{
	if (!CanLoadPathName(assetPath))
		return false;

    string metaDataPath = GetMetaDataPathFromAssetPath (assetPath);
	SubstanceImporter* importer = dynamic_pptr_cast<SubstanceImporter*> (FindAssetImporterAtPath (metaDataPath));
	if (importer==NULL)
		return false;

	bool reimport = false;
	bool hasToUnload = unload && !importer->IsPersistentDirty();

	if (importer->m_MaterialImportOutputs.size()<importer->m_MaterialInstances.size())
		importer->m_MaterialImportOutputs.resize(importer->m_MaterialInstances.size());
	vector<MaterialImportOutput>::iterator imp = importer->m_MaterialImportOutputs.begin();
	for (vector<MaterialInstanceSettings>::iterator it=importer->m_MaterialInstances.begin() ; it!=importer->m_MaterialInstances.end() ; ++it,++imp)
	{
		SubstanceBuildTargetSettings sgs = importer->GetPlatformSettings(it->name, targetPlatform);

		if (!IsSubstanceSupportedOnPlatform(targetPlatform))
			sgs.m_LoadingBehavior = ProceduralLoadingBehavior_BakeAndDiscard;

		TextureFormat format = IsLoadingBehaviourBaked(sgs.m_LoadingBehavior) ? 
		                       importer->GetSubstanceBakedFormat((SubstanceOutputFormat)sgs.m_TextureFormat) :
		                       GetSubstanceTextureFormat((SubstanceOutputFormat)sgs.m_TextureFormat, true);
		
		if (   imp->currentSettings.m_TextureWidth    != sgs.m_TextureWidth
		    || imp->currentSettings.m_TextureHeight   != sgs.m_TextureHeight
		    || imp->currentSettings.m_TextureFormat   != format
		    || imp->baked                             != IsLoadingBehaviourBaked(sgs.m_LoadingBehavior)
		    || (requireCompressedAssets && imp->baked))
		{
			reimport = true;
			#if DEBUG_SUBSTANCE_IMPORTER
			WarningStringMsg("DoesAssetNeedReimport: %s needs to be reimported because of %s (%d, %d, %d, %d) vs. (%d, %d, %d, %d)%s", assetPath.c_str(), it->name.c_str(),
			                 imp->currentSettings.m_TextureWidth, imp->currentSettings.m_TextureHeight, imp->currentSettings.m_TextureFormat, imp->baked,
			                 sgs.m_TextureWidth, sgs.m_TextureHeight, (int) format, IsLoadingBehaviourBaked(sgs.m_LoadingBehavior),
			                 (requireCompressedAssets && imp->baked) ? ", reqCompAssets is true and the substance is baked" : "");
			#endif
			break;
		}
	}

	if (hasToUnload)
	{
		UnloadObject (importer);
		GetPersistentManager().UnloadStream(metaDataPath);
	}
	return reimport;
}

int SubstanceImporter::CanLoadPathName(const std::string& path, int* q)
{
	if (q != NULL)
		*q = -1002;
	string ext = ToLower (GetPathNameExtension (path));
	return (ext == "sbsar");
}

SubstanceBuildTargetSettings SubstanceImporter::GetPlatformSettings (const std::string& materialName, BuildTargetPlatform platform) const
{
	SubstanceBuildTargetSettings settings;
	MaterialInstanceSettings* material = ((SubstanceImporter*)this)->FindMaterialInstanceSettings(materialName);
	if (!material)
		return settings;
	settings.m_BuildTarget = GetBuildTargetGroupName(platform, false);
	if (!GetPlatformTextureSettings(materialName, settings.m_BuildTarget, &settings.m_TextureWidth, &settings.m_TextureHeight, &settings.m_TextureFormat, &settings.m_LoadingBehavior))
		GetPlatformTextureSettings(materialName, "", &settings.m_TextureWidth, &settings.m_TextureHeight, &settings.m_TextureFormat, &settings.m_LoadingBehavior);
	return settings;
}

void SubstanceImporter::ClearPlatformTextureSettings(const std::string& materialName, const std::string& platform)
{
	SKIP_PLAYMODE();
	MaterialInstanceSettings* material = FindMaterialInstanceSettings(materialName);
	if (material)
		erase_if(material->buildTargetSettings, EqualTargetByName(platform));
}

void SubstanceImporter::SetPlatformTextureSettings(const std::string& materialName, const std::string& platform, int textureWidth, int textureHeight, int format, int loadingBehavior)
{
	SKIP_PLAYMODE();
	MaterialInstanceSettings* material = FindMaterialInstanceSettings(materialName);
	if (!material) return;

	// any specific platform settings?
	std::vector<SubstanceBuildTargetSettings>::iterator it =
	find_if(material->buildTargetSettings, EqualTargetByName(platform));

	if (it != material->buildTargetSettings.end())
	{
		SubstanceBuildTargetSettings& settings = *it;
		settings.m_TextureWidth = textureWidth;
		settings.m_TextureHeight = textureHeight;
		settings.m_TextureFormat = format;
		settings.m_LoadingBehavior = (ProceduralLoadingBehavior)loadingBehavior;
	}
	else // add new
	{
		SubstanceBuildTargetSettings settings;
		settings.m_BuildTarget = platform;
		settings.m_TextureWidth = textureWidth;
		settings.m_TextureHeight = textureHeight;
		settings.m_TextureFormat = format;
		settings.m_LoadingBehavior = (ProceduralLoadingBehavior)loadingBehavior;
		material->buildTargetSettings.push_back(settings);
	}
	SetDirty();
}

bool SubstanceImporter::GetPlatformTextureSettings(const std::string& materialName, const std::string& platform, int* textureWidth, int* textureHeight, int* format, int* loadBehavior) const
{
	MaterialInstanceSettings* material = ((SubstanceImporter*)this)->FindMaterialInstanceSettings(materialName);
	if (!material) return false;

	// any specific platform settings?
	std::vector<SubstanceBuildTargetSettings>::const_iterator it =
	find_if(material->buildTargetSettings.begin(), material->buildTargetSettings.end(), EqualTargetByName(platform));

	if (it != material->buildTargetSettings.end())
	{
		*textureWidth = it->m_TextureWidth;
		*textureHeight = it->m_TextureHeight;
		*format = it->m_TextureFormat;
		*loadBehavior = (int)it->m_LoadingBehavior;
		return true;
	}
	else // add new
	{
		*textureWidth = 512;
		*textureHeight = 512;
		*format = Substance_OFormat_Compressed;
		*loadBehavior = (int)ProceduralLoadingBehavior_Generate;
		return false;
	}
}

// Export to bitmap

void ExportTexture(SubstanceHandle* _pHandle, unsigned int _OutputIndex, ProceduralTexture* texturePtr, const std::string& exportBitmapPath)
{
	SubstanceTexture outputTexture;

	int error = substanceHandleGetOutputs(_pHandle, Substance_OutOpt_TextureId, _OutputIndex, 1, &outputTexture);
	if (error != 0)
	{
		ErrorString("Failed to retrieve substance texture data");
		return;
	}

	std::string exportPath = exportBitmapPath + "/" + texturePtr->GetName();
	exportPath += ".tga";
	if (outputTexture.mipmapCount==0)
	{
		outputTexture.mipmapCount = CalculateMipMapCount3D(outputTexture.level0Width, outputTexture.level0Height,1);
	}

	if (texturePtr->GetSubstanceFormat() != kTexFormatRGBA32)
	{
		ErrorString("Failed to generate bitmap from Substance");
		return;
	}

	// Save TGA
	SaveImageToFile(static_cast<UInt8*>(outputTexture.buffer), outputTexture.level0Width, outputTexture.level0Height, kTexFormatRGBA32, exportPath, 'TGAf');

	UNITY_FREE(kMemSubstance, outputTexture.buffer);
}

// Substance callback for backing
std::vector<SubstanceImporter::GeneratedTexture> SubstanceImporter::m_GeneratedTextures;

void SUBSTANCE_CALLBACK	SubstanceImporter::OnTextureGenerated(SubstanceHandle* _pHandle, unsigned int _OutputIndex, size_t _JobUserData)
{
	SubstanceOutputDesc outputDesc;
	if (substanceHandleGetOutputDesc(_pHandle, _OutputIndex, &outputDesc)==0)
	{
		std::map<unsigned int, ProceduralTexture*>::iterator it;
		it = GetSubstanceSystem ().processedTextures.find(outputDesc.outputId);
		if (it!=GetSubstanceSystem ().processedTextures.end())
		{
			GeneratedTexture texture;
			texture.handle = _pHandle;
			texture.outputIndex = it->first;
			texture.texture = it->second;
			m_GeneratedTextures.push_back(texture);
		}
	}
}

void SubstanceImporter::ExportBitmaps(ProceduralMaterial& material)
{
	// Let user choose output path
	string exportBitmapPath = RunSaveFolderPanel("Set export path...", "", "", true);
	if (!IsDirectoryCreated(exportBitmapPath))
		return;

	// Invalidate full material
	ProceduralMaterial::Textures& textures = material.GetTextures();
	for (ProceduralMaterial::Textures::iterator it=textures.begin();it!=textures.end();++it)
	{
		(*it)->GetTextureParameters().Invalidate();
		(*it)->SetSubstanceFormat(kTexFormatRGBA32);

		if ((*it)->GetUsageMode()==kTexUsageNormalmapDXT5nm)
		{
			(*it)->SetUsageMode(kTexUsageNormalmapPlain);
		}
		else
		{
			(*it)->SetUsageMode(kTexUsageNone);
		}
	}
	SubstanceInputs& inputs = material.GetSubstanceInputs();
	for (SubstanceInputs::iterator i=inputs.begin();i!=inputs.end();++i)
	{
		i->EnableFlag(SubstanceInput::Flag_Modified, true);
	}
	material.Clean();
	GetSubstanceCache ().SafeClear();

	// Generate textures
	GetSubstanceSystem().SetOutputCallback((void*)OnTextureGenerated);
	material.RebuildTexturesImmediately();
	GetSubstanceSystem().SetOutputCallback();
	for (std::vector<GeneratedTexture>::iterator i=m_GeneratedTextures.begin() ; i!=m_GeneratedTextures.end() ; ++i)
	{
		ExportTexture(i->handle, i->outputIndex, i->texture, exportBitmapPath);
	}
	m_GeneratedTextures.clear();

	// Reimport material
	set<UnityGUID> assets;
	assets.insert(ObjectToGUID(&material));
	AssetInterface::Get().ImportAssets (assets, kForceUncompressedImport);
}

bool SubstanceImporter::IsSubstanceParented (ProceduralTexture& texture, ProceduralMaterial& material)
{
	ProceduralMaterial* parentMaterial = texture.GetSubstanceMaterialPtr();
	return parentMaterial==&material;
}

static ProceduralOutputType SubstanceUsageNameToProceduralPropertyType (const std::string& usageArbitraryCase)
{
	string usage = ToUpper(usageArbitraryCase);

	if (usage == "DIFFUSE")  return Substance_OType_Diffuse;
	else if (usage == "NORMAL")   return Substance_OType_Normal;
	else if (usage == "HEIGHT")   return Substance_OType_Height;
	else if (usage == "EMISSIVE") return Substance_OType_Emissive;
	else if (usage == "SPECULAR") return Substance_OType_Specular;
	else if (usage == "OUTPUT")   return Substance_OType_Diffuse;
	else if (usage == "OPACITY")  return Substance_OType_Opacity;
	else						  return Substance_OType_Unknown;
}

static TextureUsageMode SubstanceUsageNameToTextureUsage (BuildTargetPlatform targetPlatform, const std::string& usageArbitraryCase)
{
	string usage = ToUpper(usageArbitraryCase);

	if (usage == "NORMAL")
		return DoesTargetPlatformUseDXT5nm(targetPlatform) ? kTexUsageNormalmapDXT5nm : kTexUsageNormalmapPlain;
	else
		return kTexUsageNone;
}

std::string ProceduralPropertyTypeToUnityShaderPropertyName (ProceduralOutputType type)
{
	if (type == Substance_OType_Diffuse)        return "_MainTex";
	else if (type == Substance_OType_Normal)    return "_BumpMap";
	else if (type == Substance_OType_Height)    return "_ParallaxMap";
	else if (type == Substance_OType_Emissive)  return "_Illum";
	else                                        return "";
}

static void BindTexturesToShader( ProceduralMaterial& material )
{
	const ProceduralMaterial::Textures& textures = material.GetTextures();

	// This has the effect of reloading the shader and testing for its validity at the same time...
	if ( material.GetShader() == NULL )
		return;

	// Get shader properties
	const ShaderLab::PropertySheet& properties = material.GetProperties();
	const ShaderLab::PropertySheet::TexEnvs& textureProperties = properties.GetTexEnvsMap();
	UnityPropertySheet::TexEnvMap& savedProps = material.GetSavedProperties().m_TexEnvs;

	for (ShaderLab::PropertySheet::TexEnvs::const_iterator it=textureProperties.begin() ; it!=textureProperties.end() ; ++it)
	{
		bool assigned = false;

		// Did we have some old setting for this particular slot?
		const ShaderLab::FastPropertyName slotPropName = it->first;
		if (savedProps.count(slotPropName))
		{
			// An old setting was found for this particular slot
			Texture* oldTex = savedProps[slotPropName].m_Texture;

			// Was something assigned to this slot?
			if (oldTex)
			{
				// Was it a regular texture or was it a ProceduralTexture?
				ProceduralTexture* oldPTex = dynamic_cast<ProceduralTexture*> (oldTex);
				if (oldPTex)
				{
					// The old assignment was a ProceduralTexture
					// We need to find which ProceduralMaterial it came from and find it back in this material's textures
					ProceduralMaterial* oldPMat = oldPTex->GetSubstanceMaterial();
					if (!strcmp(oldPMat->GetName(), material.GetName()))
					{
						// The old ProceduralTexture was from this material
						// Since the TextureID is no more valid, we need to find the old texture's name
						// and look up the new texture from the re-imported material by its name
						const char *oldPTexName = oldPTex->GetName();
						for (int index=0 ; index<textures.size() ; ++index)
						{
							if (!strcmp(textures[index]->GetName(), oldPTexName))
							{
								// Extra check to ensure the usage is the same
								if (oldPTex->GetType() == textures[index]->GetType())
								{
									material.SetTexture(slotPropName, textures[index]);
									assigned = true;
								}
								break;
							}
						}
					}
					else
					{
						// The old ProceduralTexture comes from another ProceduralMaterial

						// This can only work reliably if the textures come from a separate package than the one that
						// contains the current material (the fact that this code is called means that the current package
						// is being reimported, and texture IDs will change as a consequence)

						if (strcmp(material.GetSubstancePackageName(), oldPMat->GetSubstancePackageName()))
						{
							material.SetTexture(slotPropName, oldTex);
							assigned = true;
						}
						else
						{
							// If the SubstancePackage names are the same but the material names are not, it means that the user is
							// trying to create some cross-graph intra-package texture-to-shader-slot assignment. This is not allowed
							// as the graph that produces the texture being assigned will be reimported as well, and there is no way
							// to predict the TextureID the desired texture will have next

							WarningString("Assigning a ProceduralTexture to a ProceduralMaterial shader from the same package is not allowed.");
						}
					}
				}
				else
				{
					// The old assignment was a regular texture, let's keep it
					material.SetTexture(slotPropName, oldTex);
					assigned = true;
				}
			}
		}

		if (!assigned)
		{
			// No old setting (or an invalid one) was found for this particular slot
			// We must try to fill it with a Substance output whose usage matches this slot
			for (int texIdx=0 ; texIdx<textures.size() ; ++texIdx)
			{
				UnityStr shaderLabName = ProceduralPropertyTypeToUnityShaderPropertyName (textures[texIdx]->GetType());
				ShaderLab::FastPropertyName	shaderProperty = ShaderLab::Property( shaderLabName );
				if (slotPropName == shaderProperty)
				{
					material.SetTexture(slotPropName, textures[texIdx]);
					assigned = true;
					break;
				}
			}
		}

		// If we still haven't find anything valid for this particular slot
		//  - if it is the _MainTex slot, we bind the first Substance output to it
		//  - if it is any other slot, we set it to NULL
		if (!assigned)
		{
			if (slotPropName == ShaderLab::Property("_MainTex"))
			{
				material.SetTexture(slotPropName, textures[0]);
			}
			else
			{
				material.SetTexture(slotPropName, NULL);
			}
		}
	}

	material.SetDirty();
}

static void ApplyMaterialInformation (ProceduralMaterial& material, const ProceduralMaterialInformation& information)
{
	if ( material.GetShader() != NULL )
	{
		const ShaderLab::PropertySheet& properties = material.GetProperties();

		// Apply TexEnvs & global scale
		const ShaderLab::PropertySheet::TexEnvs& textureProperties = properties.GetTexEnvsMap();

		for (ShaderLab::PropertySheet::TexEnvs::const_iterator it=textureProperties.begin();it!=textureProperties.end();++it)
		{
			if (material.ActuallyHasTextureProperty(it->first))
			{
				Texture* texture = material.GetTexture(it->first);

				if (texture != NULL
					&& texture->IsDerivedFrom(ProceduralTexture::GetClassIDStatic())
					&& ShaderLab::Property(ProceduralPropertyTypeToUnityShaderPropertyName(static_cast<ProceduralTexture*>(texture)->GetType()))==it->first)
				{
					material.SetTextureScale( it->first, information.m_Scale );
					material.SetTextureOffset( it->first, information.m_Offset );
				}
			}
		}

		// Apply flags
		material.EnableFlag(ProceduralMaterial::Flag_GenerateAll, information.m_GenerateAllOutputs!=0);
		material.SetAnimationUpdateRate(information.m_AnimationUpdateRate);
	}
}

static void ApplyMaterialSettings (ProceduralMaterial& material, const MaterialInstanceSettings& settings)
{
	if ( material.GetShader() != NULL )
	{
		// Apply saved base-properties if needed
		// No need to save it if there is no user-modification
		if (settings.materialProperties.m_Colors.size()>0)
			material.GetSavedProperties().m_Colors = settings.materialProperties.m_Colors;
		if (settings.materialProperties.m_Floats.size()>0)
			material.GetSavedProperties().m_Floats = settings.materialProperties.m_Floats;
		if (settings.materialProperties.m_TexEnvs.size()>0)
			material.GetSavedProperties().m_TexEnvs = settings.materialProperties.m_TexEnvs;

        // Remove unused properties
        material.GetSavedProperties().CullUnusedProperties(material.GetShader()->GetParsedForm());

        // Remove old textures references since we are going to assign the new ones
        UnityPropertySheet::TexEnvMap& texEnvs = material.GetSavedProperties().m_TexEnvs;
        for (UnityPropertySheet::TexEnvMap::iterator it=texEnvs.begin() ; it!=texEnvs.end() ; ++it)
        {
            Texture* tex = it->second.m_Texture;
            if (tex==NULL) it->second.m_Texture = NULL;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// MaterialInstanceSettings::InputImportSettings class
//////////////////////////////////////////////////////////////////////////
//
InputImportSettings::InputImportSettings()
{
}

template<class T>
void InputImportSettings::Transfer( T& transfer )
{
	TRANSFER( name );
	TRANSFER( value );
}

template<class T>
void MaterialInstanceSettings::Transfer( T& transfer )
{
	transfer.SetVersion(10);

	// Prepare to retrieve old m_GeneratedAtLoading which moved from materialInformation to per platform settings
	int m_OldGeneratedAtLoading;
	if (transfer.IsVersionSmallerOrEqual (3))
	{
		Assert(transfer.GetUserData() == NULL);
		transfer.SetUserData(&m_OldGeneratedAtLoading);
	}

	TRANSFER( name );
	TRANSFER( prototypeName );
	TRANSFER( shaderName );
	TRANSFER( inputs );
	TRANSFER( materialInformation );
	TRANSFER( materialProperties );
	TRANSFER( textureParameters );

	// Sanitize the texture parameters of this material instance after having read them
	// as some of them can be completely bogus with settings produced by older versions.
	// This is the "let's deal with broken existing data" part of the fix for case 567991.
	if (transfer.IsReading() && transfer.IsVersionSmallerOrEqual (9))
	{
		std::vector<TextureImportSettings> cleanTexParams;
		cleanTexParams.reserve(textureParameters.size());
		for (std::vector<TextureImportSettings>::iterator it_texParam = textureParameters.begin() ; it_texParam != textureParameters.end() ; ++it_texParam)
		{
			// Only TextureParameters whose name starts with the name of this MaterialInstance belong here
			if(it_texParam->name.find(name) == 0)
			{
				cleanTexParams.push_back(*it_texParam);
			}
		}

		// Only copy the cleaned vector if there was actually some cleaning to do
		if (textureParameters.size() != cleanTexParams.size())
		{
			textureParameters = cleanTexParams;
		}
	}

	TRANSFER( buildTargetSettings );

	// Use old m_GeneratedAtLoading to set load behaviors
	if (transfer.IsVersionSmallerOrEqual (3))
	{
		for (vector<SubstanceBuildTargetSettings>::iterator it=buildTargetSettings.begin() ; it!=buildTargetSettings.end() ; ++it)
			it->m_LoadingBehavior = m_OldGeneratedAtLoading!=0?ProceduralLoadingBehavior_Generate:ProceduralLoadingBehavior_None;
	}

	if (transfer.IsVersionSmallerOrEqual (3))
	{
		transfer.SetUserData(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
// MaterialImportOutput
//////////////////////////////////////////////////////////////////////////
//
MaterialImportOutput::MaterialImportOutput() :
	baked(0)
{
	currentSettings.m_TextureWidth = 0;
	currentSettings.m_TextureHeight = 0;
}

template<class T>
void MaterialImportOutput::Transfer( T& transfer )
{
	transfer.SetVersion(10);

	TRANSFER( currentSettings );
	TRANSFER( baked );
}

//////////////////////////////////////////////////////////////////////////
// TextureImportSettings
//////////////////////////////////////////////////////////////////////////
//
TextureImportSettings::TextureImportSettings() :
	alphaSource(Substance_OType_Unknown),
	filterMode((int)kTexFilterBilinear),
	aniso(1),
	wrapMode(0)
{
}

template<class T>
void TextureImportSettings::Transfer( T& transfer )
{
	TRANSFER( name );
	transfer.Transfer(reinterpret_cast<int&> (alphaSource), "alphaSource");
	transfer.Transfer(filterMode, "filterMode");
	transfer.Transfer(aniso, "aniso");
	transfer.Transfer(wrapMode, "wrapMode");
}

//////////////////////////////////////////////////////////////////////////
// ProceduralMaterialInformation
//////////////////////////////////////////////////////////////////////////
//

ProceduralMaterialInformation::ProceduralMaterialInformation() :
	m_Offset( 0.0f, 0.0f ),
	m_Scale( 1.0f, 1.0f ),
	m_GenerateAllOutputs( 0 ),
	m_AnimationUpdateRate( 42 ) // 24fps
{
}

template<class TransferFunc>
void ProceduralMaterialInformation::Transfer( TransferFunc& transfer )
{
	transfer.SetVersion(5);

	TRANSFER (m_Offset);
	TRANSFER (m_Scale);

	// Forward old flag to user data
	if (transfer.IsVersionSmallerOrEqual (3))
		transfer.Transfer(*static_cast<int*>(transfer.GetUserData()), "m_GeneratedAtLoading");

	TRANSFER (m_GenerateAllOutputs);
	TRANSFER (m_AnimationUpdateRate);
}

IMPLEMENT_CLASS_HAS_INIT( SubstanceImporter )
IMPLEMENT_OBJECT_SERIALIZE( SubstanceImporter )
