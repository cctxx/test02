#include "UnityPrefix.h"
#include "ModelImporter.h"
#include "ImportMeshUtility.h"
#include "ClipBoundsCalculatorDeprecated.h"
#include "SkinnedMeshRendererBoundsCalculator.h"
#include "ModelLODGroupGenerator.h"
#include "AssetDatabase.h"
#include "AssetPathUtilities.h"
#include "ImportAnimationUtility.h"
#include "AssetImporterPrefabQueue.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/TypeUtilities.h"

#include "Runtime/GameCode/CloneObject.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Shaders/Shader.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "Runtime/Dynamics/RigidBody.h"
#include "Runtime/Dynamics/ConfigurableJoint.h"
#include "Runtime/Dynamics/MeshCollider.h"
#include "Runtime/Dynamics/BoxCollider.h"
#include "Runtime/Dynamics/SphereCollider.h"
#include "Runtime/Dynamics/CapsuleCollider.h"
#include "External/shaderlab/Library/properties.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Editor/Src/Utility/SerializedProperty.h"
#include "AssetInterface.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Utilities/vector_utility.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Animation/NewAnimationTrack.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Animation/AnimationClipUtility.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Animation/AnimationUtility.h"
#include "Runtime/Animation/AnimationCurveUtility.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Animation/KeyframeReducer.h"
#include "Runtime/Animation/AnimationBinder.h"
#include "Runtime/Animation/Avatar.h"
#include "Runtime/Animation/Animator.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Serialize/WriteTypeToBuffer.h"
#include "Runtime/Animation/OptimizeTransformHierarchy.h"

#include "Runtime/Animation/MecanimClipBuilder.h"

#include "Editor/Src/Animation/AvatarUtility.h"

#include "Runtime/Input/TimeManager.h"
#include "Editor/Src/Utility/Analytics.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/Light.h"

#include <sstream>

using namespace std;

IMPLEMENT_CLASS_HAS_INIT (ModelImporter)
IMPLEMENT_OBJECT_SERIALIZE (ModelImporter)

static int CanLoadPathName (const string& pathName, int* queue);
static Object* CreateExternalSerializedAsset (int classID, const string& pathName);

static void SanitizeName (string& name);
static void UpdateAffectedRootTransformDeprecated(Transform*& root, Transform& transform, bool& sendmessageToRoot);
static void CalculateAllRootBonesSortedByNumberOfChildBones(const dynamic_array<PPtr<Transform> >& allBones, dynamic_array<Transform*>& rootsSortedByNumberOfChildren);
static void OnAddedAnimationPostprocessCallback (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved);
static pair<string, string> ExtractNameAndClipFromPath(string fileName);
static UnityStr GetBaseModelOfAtConventionClipPath (const UnityStr& assetPath);
const char* kMaterialExtension = "mat";

static void SanitizeName (string& name)
{
	for (int i=0;i<name.size();i++)
	{
		if (name[i] < 32 || name[i] > 126)
		{
			name[i] = '_';
		}
	}
}



const float kDefaultAnimationRotationError = 0.5f;
const float kDefaultAnimationPositionError = 0.5f;
const float kDefaultAnimationScaleError = 0.5f;

void ModelImporter::InitializeClass ()
{
	RegisterAllowNameConversion("ModelImporter", "scaleFactor", "m_GlobalScale");
	RegisterAllowNameConversion("ModelImporter", "generateColliders", "m_AddColliders");
	RegisterAllowNameConversion("ModelImporter", "swapUVs", "swapUVChannels");
	RegisterAllowNameConversion("ModelImporter", "m_MeshSettings.swapUVChannels", "swapUVChannels");

	RegisterAllowNameConversion("ModelImporter", "m_MeshSettings.generateSecondaryUV", "generateSecondaryUV");
	RegisterAllowNameConversion("ModelImporter", "m_MeshSettings.secondaryUVAngleDistortion", "secondaryUVAngleDistortion");
	RegisterAllowNameConversion("ModelImporter", "m_MeshSettings.secondaryUVAreaDistortion", "secondaryUVAreaDistortion");
	RegisterAllowNameConversion("ModelImporter", "m_MeshSettings.secondaryUVHardAngle", "secondaryUVHardAngle");
	RegisterAllowNameConversion("ModelImporter", "m_MeshSettings.secondaryUVPackMargin", "secondaryUVPackMargin");

	RegisterAllowNameConversion("ModelImporter", "m_MeshSettings.normalImportMode", "normalImportMode");
	RegisterAllowNameConversion("ModelImporter", "m_MeshSettings.tangentImportMode", "tangentImportMode");

	RegisterAllowNameConversion("ModelImporter", "splitTangents", "splitTangentsAcrossUV");
	RegisterAllowNameConversion("ModelImporter", "smoothingAngle", "normalSmoothAngle");


	///@TODO: This is ambigous. Shouldn't we be able to specify the parent name or something???
	// materials
	//   generations: 1
	// animations:
	//   generation: 1
	//   bakeAnimations: 1

	RegisterAllowNameConversion("ModelImporter", "generation", "m_MeshSettings.tangentSpaceMode");
	RegisterAllowNameConversion("ModelImporter", "generation", "m_GenerateMaterials");

	RegisterAllowNameConversion("ModelImporter", "generateAnimations", "m_LegacyGenerateAnimations");
	RegisterAllowNameConversion("ModelImporter", "generation", "m_LegacyGenerateAnimations");

	RegisterAllowNameConversion("ModelImporter", "calculateNormals", "recalculateNormals");

	RegisterAllowNameConversion("ModelImporter", "bakeAnimations", "m_BakeSimulation");
	RegisterAllowNameConversion("ModelImporter", "clips", "m_ClipAnimations");

	AssetDatabase::RegisterPostprocessCallback(OnAddedAnimationPostprocessCallback);
}

ModelImporter::ModelImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode), m_UseFileUnits(true), m_IsReadable(true), m_LegacyGenerateAnimations(kGenerateAnimations), m_NeedToPatchRecycleIDsFromPrefab(false)
{
}

void ModelImporter::Reset ()
{
	Super::Reset();

	m_OptimizeGameObjects = false;
	m_AnimationRotationError = kDefaultAnimationRotationError;
	m_AnimationPositionError = kDefaultAnimationPositionError;
	m_AnimationScaleError = kDefaultAnimationScaleError;
	m_AddColliders = false;
	m_BakeSimulation = false;
	m_AnimationWrapMode = 0;
	m_ImportBlendShapes = true;
	m_LegacyGenerateAnimations = kGenerateAnimations;
	m_ImportAnimation = true;
	m_ImportMaterials = true;
	m_MaterialName = kMaterialNameBasedOnTextureName;
	m_MaterialSearch = kMaterialSearchRecursiveUp;
	m_AnimationType = kGeneric;
	m_AnimationCompression = kAnimationCompressionKeyframeReduction;
	m_MeshCompression = kMeshCompressionOff;
	m_MeshSettings.normalImportMode = kTangentSpaceOptionsImport;
	m_MeshSettings.tangentImportMode = kTangentSpaceOptionsCalculate;
	m_GlobalScale = 0.0F;
	m_UseFileUnits = true;
	m_IsReadable = true;
	m_MeshSettings.optimizeMesh = true;
	m_MeshSettings.swapUVChannels = false;
	m_NeedToPatchRecycleIDsFromPrefab = false;
	m_CopyAvatar = false;
	m_HumanDescription.Reset();
}

ModelImporter::~ModelImporter ()
{}

namespace
{
	void ConvertOldTangentSpaceOptions(int tangentSpaceMode, bool recalculateNormals, TangentSpaceOptions& normalImportMode, TangentSpaceOptions& tangentImportMode)
	{
		switch(tangentSpaceMode)
		{
		case ObsoleteTangentSpaceOptions::kTangentSpaceAll:
			normalImportMode = recalculateNormals ? kTangentSpaceOptionsCalculate : kTangentSpaceOptionsImport;
			// we fall back to kTangentSpaceOptionsCalculate, because it was the only option for tangents before version 5
			tangentImportMode = kTangentSpaceOptionsCalculate;
			break;
		case ObsoleteTangentSpaceOptions::kTangentSpaceOnlyNormals:
			normalImportMode = recalculateNormals ? kTangentSpaceOptionsCalculate : kTangentSpaceOptionsImport;
			tangentImportMode = kTangentSpaceOptionsNone;
			break;
		case ObsoleteTangentSpaceOptions::kTangentSpaceNone:
			normalImportMode = kTangentSpaceOptionsNone;
			tangentImportMode = kTangentSpaceOptionsNone;
			break;
		default:
			AssertMsg(true, "Unknown tangentSpaceMode mode (%d)", tangentSpaceMode);
			break;
		}
	}
}

/// Read old "textMetaNamesToFileIDs" entry in meta-data from format after
/// version 7.
void ModelImporter::TransferReadMetaNamesToFileIDs (YAMLMapping* settings, NamesToFileIDsByClassID& textMetaNamesToFileIDs)
{
	/* Data looks like this:

	   textMetaNamesToFileIDs:
	     //RootNode:
	       data:
	         first: 1
	         second: a0860100
	*/

	// Loop through string -> map<int, list> entries.
	for (YAMLMapping::const_iterator iter = settings->begin ();
	     iter != settings->end (); ++iter)
	{
		UnityStr key = iter->first->GetStringValue ();

		YAMLMapping* dataNodes = dynamic_cast<YAMLMapping*> (iter->second);
		AssertMsg (dataNodes != NULL, "Expected mapping node as value of textMetaNamesToFileIDs entry!");
		if (!dataNodes)
			continue;

		FileIDsByClassID fileIDsByClassID;
		for (YAMLMapping::const_iterator dataIter = dataNodes->begin ();
		     dataIter != dataNodes->end (); ++dataIter)
		{
			DebugAssert (strcmp (dataIter->first->GetStringValue ().c_str (), "data") == 0);

			YAMLMapping* pairNode = dynamic_cast<YAMLMapping*> (dataIter->second);
			AssertMsg (pairNode != NULL, "Expected mapping node with first&second pair!");
			if (!pairNode)
				continue;

			YAMLScalar* classIDNode = dynamic_cast<YAMLScalar*> (pairNode->Get ("first"));
			AssertMsg (classIDNode != NULL, "Expected scalar 'first' key in pair node!");
			if (!classIDNode)
				continue;

			YAMLScalar* fileIDCollection = dynamic_cast<YAMLScalar*> (pairNode->Get ("second"));
			AssertMsg (fileIDCollection != NULL, "Expected scalar 'second' key in pair node!");
			if (!fileIDCollection)
				continue;

			int classID = classIDNode->GetIntValue ();
			std::string fileIDString = fileIDCollection->GetStringValue ();
			int numFileIDs = fileIDString.size () / sizeof (int) / 2;

			std::list<LocalIdentifierInFileType>& fileIDs = fileIDsByClassID[classID];
			for (int i = 0; i < numFileIDs; ++i)
			{
				int id;
				HexStringToBytes (&fileIDString[i * 2 * sizeof (int)], sizeof (int), &id);
				fileIDs.push_back (id);
			}
		}

		textMetaNamesToFileIDs[key] = fileIDsByClassID;
	}
}

void ModelImporter::TransferReadMetaNamesToFileIDsUnity34OrOlder (YAMLMapping* settings, NamesToFileIDsByClassID& textMetaNamesToFileIDs)
{
	YAMLMapping * gameObjectNamesToFileIDs = dynamic_cast<YAMLMapping*>(settings->Get("nodeInfo"));
	if (gameObjectNamesToFileIDs)
	{
		for( YAMLMapping::const_iterator i = gameObjectNamesToFileIDs->begin(); i != gameObjectNamesToFileIDs->end() ; i++ )
		{
			string name = string(*i->first);
			YAMLSequence* fileIDs  = dynamic_cast<YAMLSequence*> ( i->second );

			if ( fileIDs )
			{
				FileIDsByClassID tmp;

				for( YAMLSequence::const_iterator j = fileIDs->begin(); j != fileIDs->end() ; j++ )
				{
					YAMLScalar* value = dynamic_cast<YAMLScalar*> (*j);
					if (! value )
						continue;

					int fileID = *value;
					int classID = fileID /  kMaxObjectsPerClassID;

					// File IDs must always be even numbers
					if( fileID & 1 )
					{
						WarningString(Format("Ignoring invalid file id %d for %s %s in meta file \"%s\"", fileID, Object::ClassIDToString (classID).c_str(), name.c_str(), GetTextMetaDataPath().c_str()));
						continue;
					}

					////@TODO: Enforce fileID being in classID Range!
					tmp[classID].push_back(fileID);
				}

				textMetaNamesToFileIDs.insert( make_pair(name, tmp) );
			}

			// Backwards compatibility with 2.6 beta 2:
			YAMLMapping* fileIDsMap  = dynamic_cast<YAMLMapping*> ( i->second );

			if ( fileIDsMap )
			{
				FileIDsByClassID tmp;

				for( YAMLMapping::const_iterator j = fileIDsMap->begin(); j != fileIDsMap->end() ; j++ )
				{
					int fileID = *j->first;

					int classID = fileID /  kMaxObjectsPerClassID;
					tmp[classID].push_back(fileID);
				}

				textMetaNamesToFileIDs.insert( make_pair(name, tmp) );
			}
		}
	}
}

// Naming conventions for modo have changed when switching from collada to fbx
void ModelImporter::PatchFileIDsForModo()
{
	for (std::map<LocalIdentifierInFileType, UnityStr>::iterator i = m_FileIDToRecycleName.begin(); i != m_FileIDToRecycleName.end(); ++i)
	{
		if (BeginsWith(i->second, "Geometry_") && EndsWith(i->second, "Node"))
		{
			i->second = i->second.substr(9, i->second.length() - 9 - 4);
		}
		else if (BeginsWith(i->second, "Camera_") && EndsWith(i->second, "Node"))
		{
			i->second = i->second.substr(7, i->second.length() - 7 - 4);
		}
		else if (BeginsWith(i->second, "Light_") && EndsWith(i->second, "Node"))
		{
			i->second = i->second.substr(6, i->second.length() - 6 - 4);
		}
		else if (EndsWith(i->second, "Node") && i->second != "//RootNode")
		{
			i->second = i->second.substr(0, i->second.length() - 4);
		}
	}
}

// We no longer store the root game object name in the file id recycle identifier.
// "animationName //// rootNodeName"
void ModelImporter::PatchGenerateAnimationFileIDs()
{
	if (m_LegacyGenerateAnimations == kGenerateAnimationsInRoot || m_LegacyGenerateAnimations == kGenerateAnimations)
	{
		// Based on the name of the prefab, remove any local identifiers in file which does not map to the prefab root name used in 3.5
		UnityStr prefabName = DeletePathNameExtension(GetLastPathNameComponent(GetAssetPathName()));
		for (std::map<LocalIdentifierInFileType, UnityStr>::iterator i = m_FileIDToRecycleName.begin(); i != m_FileIDToRecycleName.end();)
		{
			int classID = i->first / kMaxObjectsPerClassID;
			if (classID == ClassID(AnimationClip))
			{
				UnityStr::size_type index = i->second.find(" //// ");
				if (index != UnityStr::npos)
				{
					if (i->second.substr(index + 6) == prefabName)
					{
					i->second.erase(index, UnityStr::npos);
				}
					else
					{
						m_FileIDToRecycleName.erase(i++);
						continue;
			}
		}
			}
			++i;
		}
	}
}

void ModelImporter::UpgradeTextMetaNameToRecycledNames(const NamesToFileIDsByClassID& textMetaNamesToFileIDs)
{
	NamesToFileIDsByClassID::const_iterator names = textMetaNamesToFileIDs.begin();
	for (;names != textMetaNamesToFileIDs.end(); ++names)
	{
		FileIDsByClassID::const_iterator classids = names->second.begin();
		for (;classids != names->second.end(); ++classids)
		{
			std::list<LocalIdentifierInFileType>::const_iterator fileIDs = classids->second.begin();
			for (; fileIDs != classids->second.end(); ++fileIDs)
			{
				LocalIdentifierInFileType fileID = *fileIDs;
				if (m_FileIDToRecycleName.find(fileID) != m_FileIDToRecycleName.end())
				{
					// This seriously should not happen, but in case it does we print a warnings that references might be lost and continue
					WarningStringObject("Internal object identifiers in the imported model has collided, references to objects in the model might be lost", this);
					continue;
				}

				m_RecycleNameToFileID.insert (make_pair (names->first, fileID));
				m_FileIDToRecycleName.insert (make_pair (fileID, names->first));
			}
		}
	}
}

/////@TODO: It looks like importing a project touches the .meta files. THis IS REALLY BAD!!!!

template<class T>
void ModelImporter::RecycleIDBackwardsCompatibility (T& transfer)
{
	// Detect if we need to patch recycle ids from prefab
	// - Only on old versions
	// - Never do it from .meta files
	if (transfer.IsVersionSmallerOrEqual (12))
		m_NeedToPatchRecycleIDsFromPrefab = true;
	if (transfer.AssetMetaDataOnly())
		m_NeedToPatchRecycleIDsFromPrefab = false;

	// Previous versions of the model importer would write the main prefab fileID into the list of recycled fileids
	// This is no longer needed but if upgrading from an older project it might still be there and we have to remove it
	// otherwise scene references to the model will be lost
	if (transfer.IsVersionSmallerOrEqual(12))
		RemoveRecycledFileIDForClassID(ClassID(Prefab));

	// This can be removed in a later version of 4.x it was only for Unity 4 beta compatbility
	// Avatars used to be based on the root game object name, thus changing when moving an avatar.
	// There is no point in having it in the fileID mapping because there is only ever one Avatar.
	if (transfer.IsOldVersion(14))
		RemoveRecycledFileIDForClassID(ClassID(Avatar));
}

static float EnsureCorrectLightmapUVPackMargin(float margin);

template<class T>
void ModelImporter::Transfer (T& transfer)
{
	transfer.SetVersion(15);

	Super::Transfer (transfer);

	RecycleIDBackwardsCompatibility (transfer);

	transfer.BeginMetaGroup ("materials");

	TRANSFER(m_ImportMaterials);

	transfer.Align();
	TRANSFER_ENUM(m_MaterialName);
	TRANSFER_ENUM(m_MaterialSearch);

	MaterialBackwardsCompatibility (transfer);

	transfer.EndMetaGroup ();

	transfer.BeginMetaGroup ("animations");
	TRANSFER_ENUM(m_LegacyGenerateAnimations);

	TRANSFER (m_BakeSimulation);
	TRANSFER (m_OptimizeGameObjects);
	
	transfer.Align();
	TRANSFER (m_AnimationCompression);
	TRANSFER (m_AnimationRotationError);
	TRANSFER (m_AnimationPositionError);
	TRANSFER (m_AnimationScaleError);
	TRANSFER (m_AnimationWrapMode);
	TRANSFER (m_ExtraExposedTransformPaths);

	TRANSFER (m_ClipAnimations);

	TRANSFER (m_IsReadable);
	transfer.Align();

	ReduceKeyframesBackwardsCompatibility (transfer);

	transfer.EndMetaGroup ();

	transfer.BeginMetaGroup ("meshes");
	TRANSFER (m_LODScreenPercentages);
	TRANSFER (m_GlobalScale);
	TRANSFER (m_MeshCompression);
	TRANSFER (m_AddColliders);
	TRANSFER (m_ImportBlendShapes);
	transfer.Transfer (m_MeshSettings.swapUVChannels, "swapUVChannels");
	transfer.Transfer (m_MeshSettings.generateSecondaryUV, "generateSecondaryUV");
	TRANSFER (m_UseFileUnits);
	transfer.Transfer (m_MeshSettings.optimizeMesh, "optimizeMeshForGPU");
	transfer.Transfer (m_MeshSettings.weldVertices, "weldVertices");
	transfer.Align();

	transfer.Transfer (m_MeshSettings.secondaryUVAngleDistortion, "secondaryUVAngleDistortion");
	transfer.Transfer (m_MeshSettings.secondaryUVAreaDistortion, "secondaryUVAreaDistortion");
	transfer.Transfer (m_MeshSettings.secondaryUVHardAngle, "secondaryUVHardAngle");
	transfer.Transfer (m_MeshSettings.secondaryUVPackMargin, "secondaryUVPackMargin");
	transfer.EndMetaGroup ();

	m_MeshSettings.secondaryUVPackMargin = EnsureCorrectLightmapUVPackMargin(m_MeshSettings.secondaryUVPackMargin);

	transfer.BeginMetaGroup ("tangentSpace");
	transfer.Transfer (m_MeshSettings.normalSmoothAngle, "normalSmoothAngle");
	transfer.Transfer (m_MeshSettings.splitTangentsAcrossUV, "splitTangentsAcrossUV");

	if (transfer.IsVersionSmallerOrEqual(4))
	{
		int tangentSpaceMode = ObsoleteTangentSpaceOptions::kTangentSpaceAll;
		transfer.Transfer (tangentSpaceMode, "m_MeshSettings.tangentSpaceMode");
		bool recalculateNormals = false;
		transfer.Transfer(recalculateNormals, "recalculateNormals");

		ConvertOldTangentSpaceOptions(tangentSpaceMode, recalculateNormals, m_MeshSettings.normalImportMode, m_MeshSettings.tangentImportMode);
		transfer.Align();
	}
	else
	{
		transfer.Align();
		transfer.Transfer((int&)m_MeshSettings.normalImportMode, "normalImportMode");
		transfer.Transfer((int&)m_MeshSettings.tangentImportMode, "tangentImportMode");
	}
	transfer.EndMetaGroup ();

	transfer.Align();
	transfer.Transfer (m_Output.importedTakeInfos, "m_ImportedTakeInfos", kIgnoreInMetaFiles);
	transfer.Transfer (m_Output.referencedClips, "m_ReferencedClips", kIgnoreInMetaFiles);
	transfer.Transfer (m_Output.animationRoots, "m_ImportedRoots", kIgnoreInMetaFiles);
	transfer.Transfer (m_Output.hasExtraRoot, "m_HasExtraRoot", kIgnoreInMetaFiles);
	transfer.Align();

	TextNameBackwardsCompatibility (transfer);

	if (transfer.IsReading() && transfer.IsVersionSmallerOrEqual (7))
	{
		if (EndsWith(GetAssetPathName(), ".lxo"))
			PatchFileIDsForModo();
	}

	if (transfer.IsReading() && transfer.IsVersionSmallerOrEqual (13))
		PatchGenerateAnimationFileIDs ();

	TRANSFER(m_ImportAnimation);
	TRANSFER(m_CopyAvatar);

	transfer.Align();

	transfer.Transfer (m_HumanDescription, "m_HumanDescription", kHideInEditorMask);
	transfer.Transfer (m_LastHumanDescriptionAvatarSource, "m_LastHumanDescriptionAvatarSource");

	TRANSFER_ENUM (m_AnimationType);

	MecanimAnimationBackwardsCompatibility(transfer);

	PostTransfer (transfer);
}


template<class T>
void ModelImporter::MaterialBackwardsCompatibility (T& transfer)
{
	if (transfer.IsVersionSmallerOrEqual(8))
	{
		int oldGenerateMaterials;
		transfer.Transfer(oldGenerateMaterials, "m_GenerateMaterials");
		SetGenerateMaterials(oldGenerateMaterials);
	}
	else
	{
		if (transfer.IsVersionSmallerOrEqual(9))
		{
			bool useDefaultMaterial;
			transfer.Transfer(useDefaultMaterial, "m_UseDefaultMaterial");
			m_ImportMaterials = !useDefaultMaterial;
		}
	}
}

template<class T>
void ModelImporter::ReduceKeyframesBackwardsCompatibility (T& transfer)
{
	if (transfer.IsOldVersion(3) || transfer.IsOldVersion(2) || transfer.IsOldVersion(1))
	{
		bool reduceKeyFrames = true;
		transfer.Transfer (reduceKeyFrames, "m_ReduceKeyframes");
		m_AnimationCompression = reduceKeyFrames ? kAnimationCompressionKeyframeReduction : kAnimationCompressionOff;
	}
}

template<class T>
void ModelImporter::TextNameBackwardsCompatibility (T& transfer)
{
	if (transfer.AssetMetaDataOnly() && transfer.IsReading())
	{
		// Temporary data used for maintaining consistent file ids in combination with using text based metadata
		NamesToFileIDsByClassID	textMetaNamesToFileIDs;

		Assert ((IsSameType<YAMLRead, T>::result));
		if (transfer.IsVersionSmallerOrEqual (7))
		{
			// Model imported by Unity 3.4.x or below
			//
			// Use custom code to transfer meta file name IDs.
			// This is needed for backwards compatibility, as the mapping used here would not work in the generic code.
			// Also, meta file name IDs are not transferred in the normal case.

			YAMLMapping* settings = dynamic_cast<YAMLMapping*> (reinterpret_cast<YAMLRead&> (transfer).GetCurrentNode ());
			if (settings)
			{
				TransferReadMetaNamesToFileIDsUnity34OrOlder (settings, textMetaNamesToFileIDs);
				delete settings;
			}
		}
		else if (transfer.IsVersionSmallerOrEqual (14))
		{
			// Need custom transfer code to work around bug we had in YAML serialization
			// code that caused "int" and "SInt32" to serialize differently.  Work around
			// this here rather than dealing with it in the YAML reader as the bug is tricky
			// to detect in the input stream.

			YAMLMapping* settings = dynamic_cast<YAMLMapping*> (reinterpret_cast<YAMLRead&> (transfer).GetValueNodeForKey ("textMetaNamesToFileIDs"));
			if (settings)
			{
				TransferReadMetaNamesToFileIDs (settings, textMetaNamesToFileIDs);
				delete settings;
			}
		}

		UpgradeTextMetaNameToRecycledNames(textMetaNamesToFileIDs);
	}
}

template <class T>
void ModelImporter::MecanimAnimationBackwardsCompatibility(T& transfer)
{
	if (transfer.IsVersionSmallerOrEqual (14))
	{
		// 3.5 compatibility
		// m_SplitAnimations was a checkbox for if we want to use the split animations. Now we just check for m_ClipAnimations being empty.
		bool m_SplitAnimations = true;
		TRANSFER(m_SplitAnimations);
		if (!m_SplitAnimations)
			m_ClipAnimations.clear();
	}

	// Unity 4 Beta compatibility
	if (transfer.IsVersionSmallerOrEqual (14))
	{
		bool m_GenericAnimation = false;
		TRANSFER(m_GenericAnimation);

		int m_AnimatorType = -1;
		TRANSFER(m_AnimatorType);

		if (m_HumanDescription.m_Human.size() >= HumanTrait::RequiredBoneCount())
		{
			m_AnimationType = kHumanoid;
		}
		else if (m_AnimatorType == 1 && !m_GenericAnimation)
		{
			m_AnimationType = kGeneric;
			m_ImportAnimation = 0;
		}
		else if (m_AnimatorType == 2)
		{
			m_AnimationType = kHumanoid;
			m_ImportAnimation = 1;
		}
		else if (m_GenericAnimation)
		{
			m_AnimationType = kGeneric;
		}
		else
			m_AnimationType = kLegacy;

		if (m_AnimationType == kHumanoid)
			m_CopyAvatar = m_LastHumanDescriptionAvatarSource.GetInstanceID() != 0;
	}
}


int ModelImporter::GetGenerateMaterials(bool showErrors) const
{
	if (!m_ImportMaterials)
		return kDontGenerateMaterials;
	else
	{
		switch (m_MaterialName)
		{
		case kMaterialNameBasedOnTextureName:
			if (showErrors)
				ErrorString("ModelImporterMaterialName.BasedOnTextureName doesn't not exist in old GenerateMaterials options");
			break;
		case kMaterialNameBasedOnMaterialName:
			if (showErrors)
				ErrorString("ModelImporterMaterialName.BasedOnMaterialName doesn't not exist in old GenerateMaterials options");
			break;
		case kMaterialNameBasedOnModelAndMaterialName: return kGenerateMaterialPerImportMaterial;
		case kMaterialNameBasedOnTextureName_Before35: return kGenerateMaterialPerTexture;
		default:
			ErrorStringMsg("Internal error: unknown m_MaterialName type: %d", m_MaterialName);
			break;
		}

		return kGenerateMaterialPerTexture;
	}
}

void ModelImporter::SetGenerateMaterials(int value)
{
	switch (value)
	{
	case kDontGenerateMaterials:
		SetImportMaterials(false);
		break;
	case kGenerateMaterialPerTexture:
		SetImportMaterials(true);
		SetMaterialName(kMaterialNameBasedOnTextureName_Before35);
		SetMaterialSearch(kMaterialSearchRecursiveUp);
		break;
	case kGenerateMaterialPerImportMaterial:
		SetImportMaterials(true);
		SetMaterialName(kMaterialNameBasedOnModelAndMaterialName);
		SetMaterialSearch(kMaterialSearchLocal);
		break;
	default:
		ErrorStringMsg("Internal error: unknown GenerateMaterial type: %d", value);
		break;
	}
}

bool ModelImporter::NeedToRetainFileIDToRecycleNameMapping()
{
	return true;
}

int ModelImporter::GetQueueNumber(const std::string& assetPathName)
{
	std::pair<std::string, std::string> thisNameAndClip = ExtractNameAndClipFromPath(assetPathName);
	// Animation clip files are referenced by base name files eg. soldier@run.fbx is referenced by soldier.fbx
	// Thus we import all referenced animation files first.
	if (!thisNameAndClip.second.empty())
		return kImportQueueModels;
	else
		return kImportQueueAnimations;
}

void ModelImporter::InstantiateImportMesh (int index, const Matrix4x4f& transform, std::vector<Mesh*>& lodMeshes, std::vector<std::vector<int> >& lodMeshesMaterials, ModelImportData& importData)
{
	AssertIf (index == -1);

	// Search through already instantiated meshes. If the transform is the same we can reuse it!
	for (InstantiatedMeshes::iterator i=importData.instantiatedMeshes[index].begin ();i != importData.instantiatedMeshes[index].end ();i++)
	{
		if (CompareApproximately (transform, i->transform))
		{
			lodMeshesMaterials = i->lodMeshesMaterials;
			lodMeshes = i->lodMeshes;
			return;
		}
	}

	Matrix4x4f scale;
	scale.SetScale (Vector3f (m_GlobalScale, m_GlobalScale, m_GlobalScale));

	// Generate mesh from import mesh
	string name = importData.scene.meshes[index].name;
	SanitizeName(name);

	std::vector<std::string> warnings;
	Matrix4x4f scaledTransform;
	MultiplyMatrices4x4 (&scale, &transform, &scaledTransform);

	const string error = GenerateMeshData(*this, importData.scene.meshes[index], scaledTransform, m_MeshSettings, name, lodMeshes, lodMeshesMaterials, warnings);

	Assert(lodMeshes.size() > 0);

	{
		// warnings are for importedMesh, but we need to associate them with some lodMesh, so we just associate them with first one
		Mesh& lodMesh = *lodMeshes[0];

		for (std::vector<std::string>::const_iterator it = warnings.begin(); it != warnings.end(); ++it)
			LogImportWarningObject (*it, &lodMesh);

		// TODO : log error instead, because it aborts calculation
		LogImportWarningObject (error, &lodMesh);
	}

	for (int i = 0; i < lodMeshes.size(); ++i)
	{
		Mesh& lodMesh = *lodMeshes[i];
		lodMesh.SetIsReadable(m_IsReadable);
		lodMesh.SetMeshOptimized(m_MeshSettings.optimizeMesh); // mircea@ non-optimized meshes have to maintain the original vertex layout hence no partitioning
		lodMesh.SetMeshCompression(m_MeshCompression);
		lodMesh.AwakeFromLoad(kDefaultAwakeFromLoad);

		lodMesh.RecalculateBounds();
	}

	InstantiatedMesh instantiated;
	instantiated.transform = transform;
	instantiated.lodMeshes = lodMeshes;
	instantiated.lodMeshesMaterials = lodMeshesMaterials;
	importData.instantiatedMeshes[index].push_back (instantiated);
}

namespace
{
	void InstantiateCamera(const ImportCameraComponent& importedCamera, GameObject& gameObject, const float globalScale)
	{
		GameObject& cameraGo = CreateGameObjectWithHideFlags("Camera", true, 0, "Transform", "Camera", NULL);
		{
			Transform& transform = cameraGo.GetComponent(Transform);
			transform.SetLocalEulerAngles(Vector3f(0, 180, 0));
			transform.SetParent(&gameObject.GetComponent(Transform), Transform::kLocalPositionStays);
		}

		//AddComponent(gameObject, "Camera", NULL);
		Camera& camera = cameraGo.GetComponent(Camera);

		camera.SetOrthographic(importedCamera.orthographic);
		if (importedCamera.nearPlane >= 0)
			camera.SetNear(importedCamera.nearPlane * globalScale);
		if (importedCamera.farPlane >= 0)
			camera.SetFar(importedCamera.farPlane * globalScale);
		if (importedCamera.fieldOfView >= 0)
			camera.SetFov(importedCamera.fieldOfView);
		if (importedCamera.orthographicSize >= 0)
			camera.SetOrthographicSize(importedCamera.orthographicSize * globalScale);
	}

	void InstantiateLight(const ImportLightComponent& importedLight, GameObject& gameObject, const float globalScale)
	{
		GameObject& cameraGo = CreateGameObjectWithHideFlags("Light", true, 0, "Transform", "Light", NULL);
		{
			Transform& transform = cameraGo.GetComponent(Transform);
			transform.SetLocalEulerAngles(Vector3f(90, 0, 0));
			transform.SetParent(&gameObject.GetComponent(Transform), Transform::kLocalPositionStays);
		}

		Light& light = cameraGo.GetComponent(Light);

		light.SetType(importedLight.type);
		// TODO : implement range end and range start
		//if (importedLight.rangeStart >= 0)
		//	light.SetRangeStart(importedLight.rangeStart * globalScale);
		//if (importedLight.rangeEnd >= 0)
		//	light.SetRangeEnd(importedLight.rangeEnd * globalScale);
		if (importedLight.spotAngle >= 0)
			light.SetSpotAngle(importedLight.spotAngle);
		if (importedLight.intensity >= 0)
			light.SetIntensity(importedLight.intensity);

		light.SetColor(importedLight.color);
		light.SetShadows(importedLight.castShadows ? kShadowHard : kShadowNone);
	}
}

GameObject& ModelImporter::InstantiateImportNode (const ImportNode& node, Transform* parent, ModelImportData& importData)
{
	ImportScene& scene = importData.scene;

	string name = node.name;
	SanitizeName(name);
	GameObject* go = NULL;

	// Note: here and below the GOs area created as activated. When we do model post-processing
	// in scripts, they really expect regular active GOs to be there, so they can query components etc.
	if (ShouldGenerateAnimationComponentOnEveryNode ())
		go = &CreateGameObjectWithHideFlags (name, true, 0, "Transform", "Animation", NULL);
	else
		go = &CreateGameObjectWithHideFlags (name, true, 0, "Transform", NULL);

	Transform& goTransform = go->GetComponent(Transform);
	goTransform.SetLocalPosition (node.position * m_GlobalScale);
	goTransform.SetLocalRotation (node.rotation);
	goTransform.SetLocalScale (node.scale);
	goTransform.SetParent (parent, Transform::kLocalPositionStays);

	// Import mesh
	if (node.meshIndex != -1)
	{
		std::vector<std::vector<int> > lodMeshesMaterials;
		std::vector<Mesh*> lodMeshes;

		// Instantiate mesh
		InstantiateImportMesh (node.meshIndex, node.meshTransform, lodMeshes, lodMeshesMaterials, importData);
		Assert(lodMeshes.size() >= 1 && lodMeshes.size() == lodMeshesMaterials.size());

		// TODO : if mesh is split, then blendshape animation should be attached to each submesh
		node.instantiatedLodMeshes.clear();
		for (int i = 0; i < lodMeshes.size(); ++i)
		{
			std::vector<int>& lodMeshMaterials = lodMeshesMaterials[i];
			Mesh& lodMesh = *lodMeshes[i];

			GameObject* meshGo = go;
			if (lodMeshes.size() > 1)
			{
				string submeshName = name + "_MeshPart" + IntToString(i);

				meshGo = &CreateGameObject(submeshName, "Transform", NULL);

				meshGo->GetComponent(Transform).SetParent(&goTransform, Transform::kLocalPositionStays);
			}

			node.instantiatedLodMeshes.push_back(ImportNode::InstantiatedMesh(meshGo, &lodMesh));

			// Unskinned mesh
			const ImportMesh& importMesh = scene.meshes[node.meshIndex];
			if (importMesh.bones.empty ())
			{
				// Add Components
				AddComponents (*meshGo, "MeshFilter", "MeshRenderer", NULL);
				if (m_AddColliders)
				{
						AddComponent (*meshGo, "MeshCollider", NULL);
						meshGo->GetComponent (MeshCollider).SetSharedMesh (&lodMesh);
				}
			}
			// Skinned mesh
			else
			{
				// We dont add collider for skinned meshes!
				AddComponents (*meshGo, "MeshFilter", "MeshRenderer", NULL);
			}

			// Assign lodmesh and materials
			meshGo->GetComponent (MeshFilter).SetSharedMesh (&lodMesh);
			MeshRenderer& renderer = meshGo->GetComponent (MeshRenderer);

			// Assign normal materials1
			if (!lodMeshMaterials.empty ())
			{
				if (lodMesh.GetSubMeshCount () != lodMeshMaterials.size ())
				{
					LogImportError (Format("The mesh of %s has %u sub meshes but the renderer is using %u materials. Your mesh should use the same amount of sub meshes as materials.", renderer.GetName(), lodMesh.GetSubMeshCount (), (unsigned int)lodMeshMaterials.size ()));
				}

				renderer.SetMaterialCount (lodMeshMaterials.size ());
				for (int i=0;i<lodMeshMaterials.size ();i++)
					renderer.SetMaterial (InstantiateImportMaterial (lodMeshMaterials[i], renderer, importData), i);
			}
			// Assign default material
			else
			{
				int submeshCount = lodMesh.GetSubMeshCount();
				renderer.SetMaterialCount (max (submeshCount, 1));
				for (int i=0;i<submeshCount;i++)
					renderer.SetMaterial (GetBuiltinExtraResource<Material> ("Default-Diffuse.mat"), i);
			}
		}
	}

	node.instantiatedGameObject = go;

	// TODO : Camera & Light import is disabled for now
	//if (node.cameraIndex >= 0)
	//	InstantiateCamera(m_Scene->cameras[node.cameraIndex], *go, m_GlobalScale);
	//
	//if (node.lightIndex >= 0)
	//	InstantiateLight(m_Scene->lights[node.lightIndex], *go, m_GlobalScale);

	// allow mono to do stuff based on the userproperties stored in the fbx file for this node.
	if (!node.userData.empty())
		MonoPostprocessGameObjectWithUserProperties(*go,node.userData);

	// Import node children
	for (ImportNodes::const_iterator i=node.children.begin ();i != node.children.end ();i++)
		InstantiateImportNode (*i, &go->GetComponent (Transform), importData);

	return *go;
}

inline Transform& GetTransformFromImportAnimation (const ImportBaseAnimation& import)
{
	return import.node->instantiatedGameObject->GetComponent(Transform);
}

AnimationClip* ModelImporter::GetPreviewAnimationClipForTake (const std::string& takeName)
{
	for (int i=0;i<m_Output.importedTakeInfos.size();i++)
	{
		if (m_Output.importedTakeInfos[i].name == takeName)
			return m_Output.importedTakeInfos[i].clip;
	}

	return m_Output.importedTakeInfos.size() > 0 ? m_Output.importedTakeInfos[0].clip : NULL;
}

AnimationClip& ModelImporter::ProduceClip (const std::string& internalProduceName, const std::string& clipName, ImportScene& scene)
{
	///@TODO: Review if humanoid animation shouldn't best be setup in import muscle clip

	AnimationClip* clip = NULL;
	if (!internalProduceName.empty())
		clip = &ProduceAssetObject<AnimationClip> (internalProduceName);
	else
	{
		// Generate preview clips with a special classID range for the LocalIdentifierInFile. This way they will never conflict with other
		// AnimationClips and a preview clip can never accidentally end up being referenced, eg. after a .meta file was deleted etc.
		clip = static_cast<AnimationClip*> (&ProducePreviewAsset(ClassID(AnimationClip), ClassID(PreviewAssetType)));
	}

	clip->SetName(clipName.c_str());
	clip->SetSampleRate(scene.sampleRate);
	clip->SetAnimationType((AnimationClip::AnimationType)m_AnimationType);

	return *clip;
}

void ModelImporter::SplitAnimationClips(GameObject& root, const ClipAnimations &clipAnimations, const vector<string>& internalClipNames, bool clipClip, ModelImportData& importData)
{
	ImportScene& scene = importData.scene;
	for (int i=0;i<clipAnimations.size();i++)
	{
		const ClipAnimationInfo& clipInfo = clipAnimations[i];

		AnimationClip* sourceClip = NULL;
		for (int t=0;t<m_Output.importedTakeInfos.size();t++)
		{
			TakeInfo& takeInfo = m_Output.importedTakeInfos[t];
			if (clipInfo.takeName == takeInfo.name || m_Output.importedTakeInfos.size() == 1)
			{
				sourceClip = takeInfo.clip;
				break;
			}
		}

		if (sourceClip == NULL)
		{
			LogImportWarning(Format("Split Animation Take Not Found '%s'", clipInfo.takeName.c_str()));
			continue;
		}

		Animation *animation = root.QueryComponent(Animation);

		AnimationClip& clip = CreateAnimClipFromSourceClip(internalClipNames[i], *sourceClip, clipInfo, sourceClip->GetSampleRate(), clipClip, root.GetName(), scene);

		AnimationClipSettings settings;
		clipInfo.ToAnimationClipSettings(settings, clip.GetSampleRate());
		clip.SetAnimationClipSettings(settings);

		if(sourceClip->IsAnimatorMotion())
		{
			RemovedMaskedCurve(clip, clipInfo);
			AddAdditionnalCurve(clip, clipInfo);

			// Events			
			AnimationClip::Events importerEvents;
			for(int eventIter = 0; eventIter < clipInfo.events.size(); eventIter++)
			{
				importerEvents.push_back(clipInfo.events[eventIter]);
				importerEvents.back().time *=  (clip.GetAnimationClipSettings().m_StopTime - clip.GetAnimationClipSettings().m_StartTime);
				importerEvents.back().time += clip.GetAnimationClipSettings().m_StartTime;
			}
			clip.SetEvents(&importerEvents[0], importerEvents.size(), true);	



		}

		if(animation != NULL)
		{
			animation->AddClip(clip);

			if((!animation->GetClip()) || (!ShouldSplitAnimations() && clip.GetName() == scene.defaultAnimationClipName))
			{
				animation->SetClip(&clip);
			}
		}
	}
}

namespace
{
	enum ImportAnimationType { kImportAnimationType_Node, kImportAnimationType_Float };

	void AssignImportAnimationToClip(const float globalScale, AnimationClip& clip, const Transform& attachToTransform, ImportBaseAnimation& importAnimation, ImportAnimationType animationType);
}

void ModelImporter::GenerateAnimationClips (GameObject& root, const std::string& remapTakeName, ModelImportData& importData)
{
	ImportScene& scene = importData.scene;
	m_Output.importedTakeInfos.reserve(importData.scene.animationClips.size());

	Transform& rootTransform = root.GetComponent(Transform);

	dynamic_array<AnimationClip*> sourceClips;

	// - Generate unsplit clips & take info
	for (int i=0;i<importData.scene.animationClips.size();i++)
	{
		ImportAnimationClip& importClip = scene.animationClips[i];
		if (!importClip.HasAnimations())
			continue;

		string previewClipName = "__preview__" + importClip.name;
		AnimationClip& clip = ProduceClip("", previewClipName, scene);

		clip.SetWrapMode(m_AnimationWrapMode);
		clip.SetHideFlags(Object::kNotEditable | Object::kHideInHierarchy);

		// Assign source animation data
		for (ImportNodeAnimations::iterator i = importClip.nodeAnimations.begin(); i != importClip.nodeAnimations.end();i++)
			AssignImportAnimationToClip(m_GlobalScale, clip, rootTransform, *i, kImportAnimationType_Node);
		for (ImportFloatAnimations::iterator itf = importClip.floatAnimations.begin(); itf != importClip.floatAnimations.end(); ++itf)
			AssignImportAnimationToClip(m_GlobalScale, clip, rootTransform, *itf, kImportAnimationType_Float);

		clip.AwakeFromLoad(kDefaultAwakeFromLoad);

		// Create take info
		if (clip.GetRange().second > clip.GetRange().first)
		{
			TakeInfo takeInfo;
			takeInfo.name = importClip.name;
			if (m_Output.importedTakeInfos.size() == 0 && !remapTakeName.empty())
				takeInfo.defaultClipName = remapTakeName;
			else
				takeInfo.defaultClipName = takeInfo.name;

			takeInfo.startTime = clip.GetRange().first;
			takeInfo.stopTime = clip.GetRange().second;
			takeInfo.bakeStartTime = importClip.bakeStart;
			takeInfo.bakeStopTime = importClip.bakeStop;
			takeInfo.sampleRate = clip.GetSampleRate();
			takeInfo.clip = &clip;
			m_Output.importedTakeInfos.push_back(takeInfo);
			sourceClips.push_back(&clip);
		}
	}

	// - Generate muscle space
	GameObject& tmpGOForSample = static_cast<GameObject&>(CloneObject(root));
	tmpGOForSample.SetName(root.GetName());
	ImportMuscleClip(tmpGOForSample, sourceClips.begin(), sourceClips.size());
	DestroyObjectHighLevel(&tmpGOForSample);

	// - Optimize curves
	for (int i=0;i<sourceClips.size();i++)
	{
		AnimationClip& clip = *sourceClips[i];

		clip.SetUseHighQualityCurve(m_AnimationCompression != kAnimationCompressionOptimal);
		if (m_AnimationCompression >= kAnimationCompressionKeyframeReduction && clip.IsAnimatorMotion())
			ReduceKeyframes(clip, m_AnimationRotationError, m_AnimationPositionError, m_AnimationScaleError, m_AnimationPositionError);

		if (m_AnimationCompression >= kAnimationCompressionKeyframeReductionAndCompression && clip.IsAnimatorMotion())
			clip.SetCompressionEnabled(true);
	}
}

// TODO : this could be optimized to return true if it has more than one
int CountSkinnedMeshesRecurse (const ImportNodes& nodes, const ImportScene& scene)
{
	// TODO : this ignores that some meshes might be split

	int counter = 0;
	for (int i=0;i<nodes.size();i++)
	{
		if (nodes[i].meshIndex != -1 && !scene.meshes[nodes[i].meshIndex].bones.empty())
		{
			const size_t instantiatedMeshCount = nodes[i].instantiatedLodMeshes.size();
			Assert(instantiatedMeshCount >= 1);
			counter += instantiatedMeshCount;
		}

		counter += CountSkinnedMeshesRecurse(nodes[i].children, scene);
	}

	return counter;
}

std::vector<UnityStr> ModelImporter::GetTransformPaths()
{
	std::vector<UnityStr> tmpPaths;
	
	if (m_OptimizeGameObjects)
	{
		if(m_Output.animationRoots.size() != 1)
			return tmpPaths;

		Animator* animator = m_Output.animationRoots[0]->QueryComponent(Animator);

		if(animator == 0)
			return tmpPaths;

		const Avatar* avatar = animator->GetAvatar();

		if(avatar == 0)
			return tmpPaths;

		const TOSVector& tos = avatar->GetTOS();
		const mecanim::animation::AvatarConstant& avatarConstant = *avatar->GetAsset();
		const mecanim::skeleton::Skeleton& skeleton = *avatarConstant.m_AvatarSkeleton;
		
		// Here we make the paths follow the same order of the bones in the Skeleton
		// (parent comes first)
		for (int i=0; i<skeleton.m_Count; i++)
		{
			mecanim::uint32_t id = skeleton.m_ID[i];
			TOSVector::const_iterator it = tos.find(id);
			AssertIf(it == tos.end());
			tmpPaths.push_back(it->second);
		}
	}
	else
	{
		for(Roots::iterator it = m_Output.animationRoots.begin() ; it < m_Output.animationRoots.end() ; it++)
		{
			GameObject* go = (*it);
			if (go == NULL)
				continue;
			
			Transform& rootTransform = go->GetComponent(Transform);
			// Get all the transform below root transform
			AvatarBuilder::NamedTransforms namedAllTransform;
			AvatarBuilder::GetAllParent(rootTransform, namedAllTransform);
			AvatarBuilder::GetAllChildren(rootTransform, namedAllTransform);
			
			tmpPaths.resize(namedAllTransform.size());
			for(int i=0;i<namedAllTransform.size();i++)
				tmpPaths[i] = namedAllTransform[i].path;
			
		}
	}

	return tmpPaths;
}

std::string ModelImporter::CalculateBestFittingPreviewGameObject()
{
	// Always use the model defined by @ convention base name
	string atConventionBase = GetBaseModelOfAtConventionClipPath(GetAssetPathName ());
	if (!atConventionBase.empty())
		return atConventionBase;
	
	if(m_AnimationType == kHumanoid)
	{
		string avatarSourcePath = GetAssetPathFromObject(m_LastHumanDescriptionAvatarSource);
		if (!avatarSourcePath.empty())
			return avatarSourcePath;			
	}

	return GetAssetPathName();
	
}


void ModelImporter::UpdateSkeletonPose(SkeletonBoneList & skeletonBones, SerializedProperty& serializedProperty)
{
	dynamic_array<UInt8> data(kMemTempAlloc);
	WriteTypeToVector (skeletonBones, &data);
	if(data.size() > 0)
		serializedProperty.ApplySerializedData(data);
}

void ModelImporter::UpdateTransformMask(AvatarMask& mask, SerializedProperty& serializedProperty)
{
	dynamic_array<UInt8> data(kMemTempAlloc);
	WriteTypeToVector (mask.m_Elements, &data);
	if(data.size() > 0)
		serializedProperty.ApplySerializedData(data);
}

#if 0
static int MecanimToUnityJointType(mecanim::uint32_t t)
{
	if (t == math::kLocked)
		return 0;
	if (t == math::kLimited)
		return 1;
	return 2;
}

void ModelImporter::GenerateAvatarCollider(Avatar& avatar, NamedTransform const& namedTransform)
{
	GameObject& colliderRootGO = CreateGameObject (std::string("PhysicsRig"), "Transform", NULL);
	Transform& colliderRoot = colliderRootGO.GetComponent(Transform);
	colliderRoot.SetParent(namedTransform[0].second);

	std::map<mecanim::uint32_t, Transform* > id;
	for(int i=0;i<namedTransform.size();i++)
		id.insert( std::make_pair( mecanim::processCRC32( namedTransform[i].second->GetName()), namedTransform[i].second) );

	std::vector<Transform* > colliders(HumanTrait::BoneCount, 0);

	mecanim::animation::AvatarConstant* cst = avatar.GetAsset();

	mecanim::memory::MecanimAllocator alloc(kMemAnimationTemp);
	mecanim::skeleton::SkeletonPose* globalPose = mecanim::skeleton::CreateSkeletonPose(cst->m_Human->m_Skeleton, alloc);

	mecanim::skeleton::SkeletonPoseComputeGlobal(cst->m_Human->m_Skeleton, cst->m_Human->m_SkeletonPose, globalPose);


	for(int i=0;i<HumanTrait::BoneCount && cst != 0;i++)
	{
		int boneId = HumanTrait::GetBoneId(avatar, i);
		if(boneId != -1)
		{
			std::map< mecanim::uint32_t, Transform* >::iterator it = id.find(cst->m_Human->m_Skeleton->m_ID[boneId]);
			if(it!=id.end())
			{
				int colliderId = HumanTrait::GetColliderId(avatar,i);

				//Transform* transform = it->second;
				if(HumanTrait::HasCollider(avatar, i) && cst->m_Human->m_ColliderArray[colliderId].m_Type != math::kNone)
				{
					TransformX boneX;

					xform2unity(globalPose->m_X[boneId], boneX.position, boneX.rotation, boneX.scale);

					//for(int j=0;j<m_HumanDescription.m_Skeleton.size();j++)
					//{
					//	if(m_HumanDescription.m_Skeleton[j].m_Name == transform->GetName())
					//	{
					//		boneX.position = m_HumanDescription.m_Skeleton[j].m_Position;
					//		boneX.rotation = m_HumanDescription.m_Skeleton[j].m_Rotation;
					//		boneX.scale = m_HumanDescription.m_Skeleton[j].m_Scale;
					//		break;
					//	}
					//}
					TransformX colliderX;
					AvatarUtility::HumanGetColliderTransform(avatar, i, boneX, colliderX);

					std::vector<string> boneNames = HumanTrait::GetBoneName();
					GameObject* colliderGO = &CreateGameObject (boneNames[i] + std::string("Collider"), "Transform", "Rigidbody", NULL);
					Transform& boneTransform = colliderGO->GetComponent(Transform);
					colliders[boneId] = &boneTransform;

					colliderGO->SetLayer(m_UserLayer);

					int parentBoneId = cst->m_Human->m_Skeleton->m_Node[boneId].m_ParentId;
					// we can do a shaky while here cause the array will contain 0 element already if they dont have a collider or kNone
					while(colliders[parentBoneId] == 0 && parentBoneId >= 0)
							parentBoneId = cst->m_Human->m_Skeleton->m_Node[parentBoneId].m_ParentId;

					boneTransform.SetParent(&colliderRoot);

					boneTransform.SetPosition(colliderX.position);
					boneTransform.SetRotation(colliderX.rotation);

					Rigidbody* rigidbody = colliderGO->QueryComponent(Rigidbody);
					float mass = cst->m_Human->m_HumanBoneMass[boneId];
					rigidbody->SetMass(max(mass, 0.01f));
					rigidbody->SetIsKinematic(true);

					/*if (boneId != cst->m_Human->mHumanBoneIndex[mecanim::human::eHips])
					{
						int axesId = cst->m_Human->m_Skeleton->m_Node[boneId].mAxesId;
						if(axesId != -1)
						{
							math::float4 maxv = cst->m_Human->m_Skeleton->mAxesArray[axesId].mLimit.mMax;
							math::float4 minv = cst->m_Human->m_Skeleton->mAxesArray[axesId].mLimit.mMin;

							int mx = HumanTrait::MuscleFromBone(i, 0);
							int my = HumanTrait::MuscleFromBone(i, 1);
							int mz = HumanTrait::MuscleFromBone(i, 2);

							AddComponent(*colliderGO, "ConfigurableJoint");
							ConfigurableJoint* configurableJoint = colliderGO->QueryComponent(ConfigurableJoint);
							//configurableJoint->SetConfiguredInWorldSpace(true);
							configurableJoint->SetAxis(Vector3f(0, 0, 1));
							configurableJoint->SetSecondaryAxis(Vector3f(1, 0, 0));
							if(parentBoneId != -1)
								configurableJoint->SetConnectedBody(colliders[parentBoneId]->GetGameObject().QueryComponent(Rigidbody));

							configurableJoint->SetXMotion(0);
							configurableJoint->SetYMotion(0);
							configurableJoint->SetZMotion(0);
							configurableJoint->SetAngularXMotion((mx != -1) ? 1 : 0);
							configurableJoint->SetAngularYMotion((my != -1) ? 1 : 0);
							configurableJoint->SetAngularZMotion((mz != -1) ? 1 : 0);

							Vector3f anchor = boneTransform.InverseTransformPoint(boneX.position);
							configurableJoint->SetAnchor(anchor);


							//ChestCollider (Limited[-10->10],Limited[10],Limited[10])
							//HeadCollider (Limited[-40->40],Limited[40],Limited[40])
							//LeftArmCollider (Limited[-10->140],Limited[90],Limited[100])
							//LeftFootCollider (Locked[0->0],Limited[30],Limited[50])
							//LeftForeArmCollider (Limited[0->160],Locked[0],Limited[60])
							//LeftHandCollider (Locked[0->0],Limited[40],Limited[80])
							//LeftLegCollider (Limited[0->160],Locked[0],Limited[40])
							//LeftUpperLegCollider (Limited[-10->120],Limited[60],Limited[50])
							//RightArmCollider (Limited[-10->140],Limited[90],Limited[100])
							//RightFootCollider (Locked[0->0],Limited[30],Limited[50])
							//RightForeArmCollider (Limited[0->160],Locked[0],Limited[50])
							//RightHandCollider (Locked[0->0],Limited[40],Limited[80])
							//RightLegCollider (Limited[0->160],Locked[0],Limited[40])
							//RightUpperLegCollider (Limited[-10->120],Limited[60],Limited[50])
							//SpineCollider (Limited[-20->20],Limited[20],Limited[20])


							SoftJointLimit limit;
							limit.limit = 0;
							limit.spring = 0;
							limit.damper = 0;
							limit.bounciness = 0;

							configurableJoint->SetLowAngularXLimit(limit);
							configurableJoint->SetHighAngularXLimit(limit);
							configurableJoint->SetAngularYLimit(limit);
							configurableJoint->SetAngularZLimit(limit);

							if (mx != -1)
							{
								limit.limit = math::degrees(minv.x());
								configurableJoint->SetLowAngularXLimit(limit);
								limit.limit = math::degrees(maxv.x());
								configurableJoint->SetHighAngularXLimit(limit);
							}
							if (my != -1)
							{
								limit.limit = math::degrees(maxv.y());//math::maximum(math::abs(minv.y()), maxv.y());
								configurableJoint->SetAngularYLimit(limit);
							}
							if (mz != -1)
							{
								limit.limit = math::degrees(maxv.z());//math::maximum(math::abs(minv.z()), maxv.z());
								configurableJoint->SetAngularZLimit(limit);
							}
						}
					}*/

					if(cst->m_Human->m_ColliderArray[colliderId].m_XMotionType != math::kIgnored &&
					   cst->m_Human->m_ColliderArray[colliderId].m_YMotionType != math::kIgnored &&
					   cst->m_Human->m_ColliderArray[colliderId].m_ZMotionType != math::kIgnored)
					{
						AddComponent(*colliderGO, "ConfigurableJoint");
						ConfigurableJoint* configurableJoint = colliderGO->QueryComponent(ConfigurableJoint);
						// change the coordinate system of the anchor to match the mecanim collider convention
						configurableJoint->SetAxis(Vector3f(0, 0, 1));
						configurableJoint->SetSecondaryAxis(Vector3f(1, 0, 0));
						if(parentBoneId != -1)
							configurableJoint->SetConnectedBody(colliders[parentBoneId]->GetGameObject().QueryComponent(Rigidbody));

						configurableJoint->SetXMotion(0);
						configurableJoint->SetYMotion(0);
						configurableJoint->SetZMotion(0);
						configurableJoint->SetAngularXMotion(MecanimToUnityJointType(cst->m_Human->m_ColliderArray[colliderId].m_XMotionType));
						configurableJoint->SetAngularYMotion(MecanimToUnityJointType(cst->m_Human->m_ColliderArray[colliderId].m_YMotionType));
						configurableJoint->SetAngularZMotion(MecanimToUnityJointType(cst->m_Human->m_ColliderArray[colliderId].m_ZMotionType));

						Vector3f anchor = boneTransform.InverseTransformPoint(boneX.position);
						configurableJoint->SetAnchor(anchor);

						SoftJointLimit limit;
						limit.limit = 0;
						limit.spring = 0;
						limit.damper = 0;
						limit.bounciness = 0;

						limit.limit = cst->m_Human->m_ColliderArray[colliderId].m_MinLimitX;
						configurableJoint->SetLowAngularXLimit(limit);
						limit.limit = cst->m_Human->m_ColliderArray[colliderId].m_MaxLimitX;
						configurableJoint->SetHighAngularXLimit(limit);
						limit.limit = cst->m_Human->m_ColliderArray[colliderId].m_MaxLimitY;
						configurableJoint->SetAngularYLimit(limit);
						limit.limit = cst->m_Human->m_ColliderArray[colliderId].m_MaxLimitZ;
						configurableJoint->SetAngularZLimit(limit);
					}

					switch(cst->m_Human->m_ColliderArray[colliderId].m_Type)
					{
						case math::kCube:
						{
							AddComponent(*colliderGO, "BoxCollider");
							BoxCollider* collider = colliderGO->QueryComponent(BoxCollider);
							collider->SetSize(colliderX.scale);
							break;
						}
						case math::kSphere:
						{
							AddComponent(*colliderGO, "SphereCollider");
							SphereCollider* collider = colliderGO->QueryComponent(SphereCollider);
							collider->SetRadius( min(colliderX.scale.x, min(colliderX.scale.y, colliderX.scale.z)) );

							break;
						}
						case math::kCylinder:
						case math::kCapsule:
						{
							AddComponent(*colliderGO, "CapsuleCollider");
							CapsuleCollider* collider = colliderGO->QueryComponent(CapsuleCollider);
							collider->SetDirection(0);
							collider->SetHeight(colliderX.scale.x);
							collider->SetRadius( min(colliderX.scale.y, colliderX.scale.z) );
							break;
						}
						default:
							break;
					}
				}
			}
		}
	}

	mecanim::skeleton::DestroySkeletonPose(globalPose, alloc);

	/*std::map<mecanim::uint32_t, Transform* > id;
	for(int i=0;i<namedTransform.size();i++)
		id.insert( std::make_pair( mecanim::processCRC32( namedTransform[i].second->GetName()), namedTransform[i].second) );

	mecanim::animation::AvatarConstant* cst = avatar.GetAsset();
	for(int i=0;i<HumanTrait::BoneCount && cst != 0;i++)
	{
		int boneId = HumanTrait::GetBoneId(avatar, i);
		if(boneId != -1)
		{
			std::map< mecanim::uint32_t, Transform* >::iterator it = id.find(cst->m_Human->m_Skeleton->m_ID[boneId]);
			if(it!=id.end())
			{
				Transform* transform = it->second;
				if(HumanTrait::HasCollider(avatar, i))
				{
					TransformX boneX;
					for(int j=0;j<m_HumanDescription.m_Skeleton.size();j++)
					{
						if(m_HumanDescription.m_Skeleton[j].m_Name == transform->GetName())
						{
							boneX.position = m_HumanDescription.m_Skeleton[j].m_Position;
							boneX.rotation = m_HumanDescription.m_Skeleton[j].m_Rotation;
							boneX.scale = m_HumanDescription.m_Skeleton[j].m_Scale;
							break;
						}
					}

					GameObject* go = &CreateGameObjectWithHideFlags (transform->GetName() + std::string("ColliderOffset"), true, 0, "Transform", NULL);

					go->SetHideFlags(kHideAndDontSave);
					Transform& transformOffset = go->GetComponent(Transform);
					go->SetLayer(m_UserLayer);
					transformOffset.SetParent(transform);

					TransformX colliderX;
					AvatarUtility::HumanGetColliderTransform(avatar, i, boneX, colliderX);

					transformOffset.SetLocalPosition(colliderX.position);
					transformOffset.SetLocalRotation(colliderX.rotation);

					// Most Collider doesn't support scale
					//transformOffset.SetLocalScale(colliderX.scale);

					// Set a rigidbody on the bone transform, assign its mass and create a joint
					GameObject& boneGO = transform->GetGameObject();
					AddComponent(boneGO, "Rigidbody");
					Rigidbody* rigidbody = boneGO.QueryComponent(Rigidbody);
					float mass = cst->m_Human->m_HumanBoneMass[boneId];
					rigidbody->SetMass(max(mass, 0.01f));
					rigidbody->SetIsKinematic(true);

					if (boneId != cst->m_Human->mHumanBoneIndex[mecanim::human::eHips])
					{
						AddComponent(boneGO, "ConfigurableJoint");
						ConfigurableJoint* configurableJoint = boneGO.QueryComponent(ConfigurableJoint);
						configurableJoint->SetConfiguredInWorldSpace(true);
						Transform* parentTransform = transform->GetParent();
						if (parentTransform)
						{
							configurableJoint->SetConnectedBody(parentTransform->GetGameObject().QueryComponent(Rigidbody));
						}
					}
					int colliderId = HumanTrait::GetColliderId(avatar,i);
					switch(cst->m_Human->m_ColliderArray[colliderId].m_Type)
					{
						case math::kCube:
						{
							AddComponent(*go, "BoxCollider");
							BoxCollider* collider = transformOffset.QueryComponent(BoxCollider);
							collider->SetSize(colliderX.scale);
							break;
						}
						case math::kSphere:
						{
							AddComponent(*go, "SphereCollider");
							SphereCollider* collider = transformOffset.QueryComponent(SphereCollider);
							collider->SetRadius( min(colliderX.scale.x, min(colliderX.scale.y, colliderX.scale.z)) );

							break;
						}
						default:
						case math::kCylinder:
						case math::kCapsule:
						{
							AddComponent(*go, "CapsuleCollider");
							CapsuleCollider* collider = transformOffset.QueryComponent(CapsuleCollider);
							collider->SetDirection(0);
							collider->SetHeight(colliderX.scale.x);
							collider->SetRadius( min(colliderX.scale.y, colliderX.scale.z) );
							break;
						}
					}
				}
			}
		}
	}*/
}
#endif



void ModelImporter::ImportAvatar (GameObject& rootGameObject, bool didImportSkinnedMesh)
{
	if (m_AnimationType != kGeneric && m_AnimationType != kHumanoid)
		return;

	// Don't generate avatar if copy avatar is enabled.
	if (m_CopyAvatar)
		return;

	std::string error;

	/*
	UNITY_VECTOR(kMemTempAlloc, UnityStr) exposedPaths;
	if (m_OptimizeGameObjects)
	{
		// 1. Figure out the transforms to be exposed
		const Transform& rootTransform = rootGameObject.GetComponent(Transform);
		GetUsefulTransformPaths(rootTransform, rootTransform, exposedPaths);
		for (int i=0;i < m_ExtraExposedTransformPaths.size();i++)
		{
			if (std::find(exposedPaths.begin(), exposedPaths.end(), m_ExtraExposedTransformPaths[i]) != exposedPaths.end())
				continue;

			exposedPaths.push_back(m_ExtraExposedTransformPaths[i]);
		}

		// 2. Pre-strip the unnecessary transforms
		RemoveUnnecessaryTransforms(rootGameObject, 
			&m_HumanDescription,
			exposedPaths.data(),
			exposedPaths.size(),
			true);
	}
	*/

	// There is only one avatar thus there is no need to register it by name.
	AvatarBuilder::Options options;
	options.avatarType = AnimationTypeToAvatarType(m_AnimationType);
	options.readTransform = true;

	Avatar& avatar = ProduceAssetObject<Avatar>();
	error = AvatarBuilder::BuildAvatar(avatar, rootGameObject, m_OptimizeGameObjects, m_HumanDescription, options);
	if(!error.empty())
		LogImportWarning(error);

	avatar.SetNameCpp(rootGameObject.GetName() + std::string("Avatar"));
	avatar.AwakeFromLoad(kDefaultAwakeFromLoad);

	AddComponent(rootGameObject, "Animator");
	Animator* animator = rootGameObject.QueryComponent(Animator);
	if(animator != NULL)
	{
		animator->SetAvatar(&avatar);
		animator->AwakeFromLoad(kDefaultAwakeFromLoad);

		if (didImportSkinnedMesh)
			animator->SetCullingMode(Animator::kCullBasedOnRenderers);
	}
	else
	{
		ErrorStringMsg("Internal error: could not add Animator");
	}
}

void ModelImporter::ImportMuscleClip(GameObject& rootGameObject, AnimationClip** clips, size_t size)
{
	if (m_AnimationType != kHumanoid && m_AnimationType != kGeneric)
		return;

	if (m_AnimationType == kGeneric)
		m_HumanDescription.m_Human.clear();

	AvatarBuilder::NamedTransforms namedTransform;

	std::string warning = AvatarBuilder::GenerateAvatarMap(rootGameObject, namedTransform, m_HumanDescription, false, AnimationTypeToAvatarType(m_AnimationType));
	if(!warning.empty())
	{
		LogImportWarning ( Format("MuscleClip '%s' conversion failed: %s", rootGameObject.GetName(), warning.c_str()));
		return;
	}

	Avatar& avatar = *CreateObjectFromCode<Avatar>();
	AvatarBuilder::Options options;
	options.avatarType = AnimationTypeToAvatarType(m_AnimationType);

	warning = AvatarBuilder::BuildAvatar(avatar, rootGameObject,false, m_HumanDescription, options);
	if(!warning.empty())
	{
		DestroySingleObject(&avatar);
		LogImportWarning ( Format("MuscleClip '%s' conversion failed: %s", rootGameObject.GetName(), warning.c_str()));
		return;
	}

	mecanim::animation::AvatarConstant* avatarConstant = avatar.GetAsset();

	TOSVector const& tos = avatar.GetTOS();
	if (avatarConstant == NULL)
	{
		DestroySingleObject(&avatar);
		LogImportWarning ( Format("MuscleClip '%s' conversion failed: couldn't find avatar definition", rootGameObject.GetName()));
		return;
	}

	
	bool isHuman = m_AnimationType == kHumanoid;

	if(isHuman && !AvatarBuilder::TPoseMatch(*avatarConstant, namedTransform, warning))
		LogImportWarning ( Format("MuscleClip '%s' conversion warning: Bone length is different in avatar and animation\n%s", rootGameObject.GetName(), warning.c_str()));

	// [case 482547] Errant / inaccurate animation when applying animation to an accurately auto-mapped avatar
	// avatarConstant->m_Human->m_Skeleton->m_Count - 1 is needed because a human skeleton always instanciate an extra root node not define in human
	if(isHuman && avatarConstant->m_Human->m_Skeleton->m_Count - 1 < m_HumanDescription.m_Human.size())
	{
		warning = Format("MuscleClip '%s' conversion warning: avatar definition doesn't match\n", rootGameObject.GetName());
		for(HumanBoneList::const_iterator it = m_HumanDescription.m_Human.begin(); it != m_HumanDescription.m_Human.end(); ++it)
		{
			mecanim::uint32_t id = mecanim::processCRC32(it->m_BoneName.c_str());
			TOSVector::const_iterator tosIt = tos.find(id);
			if(tosIt == tos.end())
				warning += Format("\tBone '%s' not found\n", it->m_BoneName.c_str());
		}

		LogImportWarning (warning);
	}

	int rootMotionIndex = avatarConstant->m_RootMotionBoneIndex;
	if(isHuman || rootMotionIndex != -1)
	{
		warning = GenerateMecanimClipsCurves(clips, size, m_ClipAnimations, *avatarConstant, isHuman, m_HumanDescription, rootGameObject, namedTransform);
		if(warning.length() > 0)
			LogImportWarning(warning);
	}

	DestroySingleObject(&avatar);
}


bool ModelImporter::ImportSkinnedMesh (const ImportNode& node, ImportScene& scene, const Transform& skeletonRoot)
{
	bool didGenerateSkinnedMesh = false;

	ImportMesh* importMesh = NULL;
	if (node.meshIndex != -1)
		importMesh = &scene.meshes[node.meshIndex];

	// SkinnedMeshRenderer is used for skinning & blendshapes (even when mesh is rigid)
	if (importMesh && (!importMesh->bones.empty() || !importMesh->shapeChannels.empty()))
	{
	for (size_t meshIndex = 0; meshIndex < node.instantiatedLodMeshes.size(); ++meshIndex)
	{
		const ImportNode::InstantiatedMesh& instantiatedMesh = node.instantiatedLodMeshes[meshIndex];

		GameObject& meshGo = *instantiatedMesh.gameObject;
		Assert(&meshGo);
		Mesh* mesh = instantiatedMesh.mesh;
		Assert(mesh);

		Transform* root = NULL;
		bool hasValidSingleRoot = false;

		// Extract Transforms that drive the bones
		dynamic_array<PPtr<Transform> > sourceTransforms;
		dynamic_array<Matrix4x4f> bindposes;
		sourceTransforms.resize_initialized (importMesh->bones.size ());
		bindposes.resize_initialized (importMesh->bones.size ());
		for (int i=0;i<importMesh->bones.size ();i++)
		{
			Matrix4x4f bindPose = importMesh->bones[i].bindpose;
			if (importMesh->bones[i].node)
			{
				GameObject& boneGO = *reinterpret_cast<GameObject*> (importMesh->bones[i].node->instantiatedGameObject);
				Transform& transform = boneGO.GetComponent (Transform);
				sourceTransforms[i] = &transform;

				UpdateAffectedRootTransformDeprecated(root, transform, hasValidSingleRoot);

				// Adjust bindpose for global scale
				if (!CompareApproximately(m_GlobalScale, 1.0F))
				{
					Matrix4x4f worldTransform = transform.GetLocalToWorldMatrix ();

					Matrix4x4f originalTransform = worldTransform;
					originalTransform.Get(0,3) /= m_GlobalScale;
					originalTransform.Get(1,3) /= m_GlobalScale;
					originalTransform.Get(2,3) /= m_GlobalScale;

					Matrix4x4f scale;
					scale.SetScale (Vector3f (m_GlobalScale, m_GlobalScale, m_GlobalScale));
					Matrix4x4f invScale;
					invScale.SetScale (Vector3f (1.0F / m_GlobalScale, 1.0F / m_GlobalScale, 1.0F / m_GlobalScale));

					Matrix4x4f temp, full;
					MultiplyMatrices4x4 (&scale, &originalTransform, &temp);
					MultiplyMatrices4x4 (&temp, &importMesh->bones[i].bindpose, &full);
					worldTransform.Invert_Full();
					MultiplyMatrices4x4 (&worldTransform, &full, &temp);
					MultiplyMatrices4x4 (&temp, &invScale, &bindposes[i]);
				}
				else
					bindposes[i] = importMesh->bones[i].bindpose;
			}
			else
				bindposes[i] = importMesh->bones[i].bindpose;

			if (!IsFinite(bindposes[i]))
			{
				LogImportWarningObject("Bindpose of imported mesh is invalid", mesh);
			}
		}

		mesh->SetBindposes(bindposes.data(), bindposes.size());

		SkinnedMeshRenderer* skin = NULL;


		// In Unity 2.x etc we moved the SkinnedMeshRenderer to the root bone.
		// This is fixed because now the bounding volume is always relative to a root bone that is calculated by the importer and stored in the SkinnedMeshRenderer.
		bool hasSingleSkinnedMeshRoot = hasValidSingleRoot && root != NULL && root->QueryComponent(SkinnedMeshRenderer) == NULL && CountSkinnedMeshesRecurse(scene.nodes, scene) == 1;
		bool moveSkinnedMeshToRootDeprecated = hasSingleSkinnedMeshRoot && ShouldUseDeprecatedMoveSkinnedMeshRendererToRootBone();

		if (!moveSkinnedMeshToRootDeprecated)
		{
			MeshRenderer* meshRenderer = meshGo.QueryComponent(MeshRenderer);
			MeshFilter* meshFilter = meshGo.QueryComponent(MeshFilter);

			AddComponent(meshGo, "SkinnedMeshRenderer");
			skin = meshGo.QueryComponent(SkinnedMeshRenderer);
			skin->SetMaterialArray(meshRenderer->GetMaterialArray(), meshRenderer->GetSubsetIndices());

			DestroyObjectHighLevel(meshRenderer);
			DestroyObjectHighLevel(meshFilter);
		}
		// Try moving the skinned mesh to the root transform.
		// We do this for backwards compatibility reasons.
		// We now can specify a primary root bone on the SkinnedMesh, thus the bounding volume will always be relative to a bone
		// instead of relative to the potentially arbitrary skinned mesh renderer transform.
		else
		{
			MeshFilter* lodMesh = meshGo.QueryComponent(MeshFilter);
			MeshRenderer* meshRenderer = meshGo.QueryComponent(MeshRenderer);

			Renderer::MaterialArray materialArray = meshRenderer->GetMaterialArray();
			Renderer::IndexArray indexArray = meshRenderer->GetSubsetIndices();

			DestroyObjectHighLevel(meshRenderer);
			DestroyObjectHighLevel(lodMesh);


			AddComponent(root->GetGameObject(), "SkinnedMeshRenderer");
			skin = root->QueryComponent(SkinnedMeshRenderer);

			skin->SetMaterialArray(materialArray, indexArray);
		}

		// Setup filter
		skin->Setup (mesh, sourceTransforms);

		// When the SkinnedMeshRenderer is not attached to the root bone.
		// Setup a reference to the root bone so that we can make the bounding volume relative to that.
		dynamic_array<Transform*> rootsSortedByNumberOfChildren;
		CalculateAllRootBonesSortedByNumberOfChildBones(sourceTransforms, rootsSortedByNumberOfChildren);
		Transform* mostUsedRoot = rootsSortedByNumberOfChildren.empty() ? NULL : rootsSortedByNumberOfChildren.back();
		if (mostUsedRoot != NULL && skin->QueryComponent(Transform) != mostUsedRoot)
			skin->SetRootBone(mostUsedRoot);

		mesh->GetBonePathHashes().clear();
		for (int boneIndex = 0; boneIndex < sourceTransforms.size(); boneIndex++)
		{
			const Transform* boneTr = sourceTransforms[boneIndex];
			UnityStr bonePath = CalculateTransformPath(*boneTr, &skeletonRoot);
			mesh->GetBonePathHashes().push_back(mecanim::processCRC32(bonePath.c_str()));
		}
		if (skin->GetRootBone())
		{
			UnityStr bonePath = CalculateTransformPath(*skin->GetRootBone(), &skeletonRoot);
			mesh->SetRootBonePathHash(mecanim::processCRC32(bonePath.c_str()));
		}

		didGenerateSkinnedMesh = true;
	}
	}

	// Import node children
	for (ImportNodes::const_iterator i=node.children.begin ();i != node.children.end ();i++)
		didGenerateSkinnedMesh |= ImportSkinnedMesh (*i, scene, skeletonRoot);

	return didGenerateSkinnedMesh;
}

namespace
{

vector<string>& SuggestTexturePathNames(const std::string& assetPathName, const ImportTexture& texture, bool includeActualPath)
{
	// NOTE : this is not very multithreading friendly, but we're doing this for performance
	static vector<string> paths;
	paths.clear ();
	paths.reserve (10);

	if (texture.path.empty ())
		return paths;

	// GetLastPathNameComponent doesn't support slashes
	AssertMsg(texture.path.find('\\') == std::string::npos, "Filename '%s' contains a forward slash", texture.path.c_str());
	AssertMsg(texture.relativePath.find('\\') == std::string::npos, "Filename '%s' contains a forward slash", texture.relativePath.c_str());

	const string textureName = GetLastPathNameComponent (texture.path);
	// This should not happen - we validate and remove invalid textures in earlier stage
	AssertMsg(CheckValidFileNameDetail(textureName) != kFileNameInvalid, "Internal error: filename '%s' is invalid");

	string meshesFolder = ToLower(DeleteLastPathNameComponent(assetPathName));
	string texturesFolderAndTextureName = AppendPathName ("Textures", textureName);

	// MeshFolder/textureName
	paths.push_back (AppendPathName (meshesFolder, textureName));

	// MeshFolder/Textures/textureName
	// ../MeshFolder/Textures/textureName
	// ../../MeshFolder/Textures/textureName
	// And so on
	while (true)
	{
		paths.push_back (AppendPathName (meshesFolder, texturesFolderAndTextureName));

		meshesFolder = DeleteLastPathNameComponent (meshesFolder);
		if (meshesFolder.find ("assets") != 0)
			break;
	}

	if (includeActualPath)
	{
		string newPath;
		// Look up the relative pathname
		newPath = ToLower(DeleteLastPathNameComponent(PathToAbsolutePath(assetPathName)));

		newPath = AppendPathName (newPath, texture.relativePath);
		// skipping paths which are longer than MAX_PATH, because ResolveSymlinks can't handle longer ones (at least) on windows
		if (newPath.size() < 260)
		{
			newPath = ResolveSymlinks (newPath);
			newPath = GetProjectRelativePath (newPath);
			if (!newPath.empty())
				paths.push_back (newPath);

			// Look up the absolute pathname
			newPath = ResolveSymlinks (texture.path);
			newPath = GetProjectRelativePath (newPath);
			if (!newPath.empty())
				paths.push_back (newPath);
		}
	}

	return paths;
}

	const std::string kMateriaNoName = "No Name";

inline void CleanupMaterialName (string& name)
{
	if (!AsciiToUTF8(name))
	{
		ErrorString("Material name " + name + " couldn't be used because it is not a valid string. Reverting to '" + kMateriaNoName + "'");
		name = kMateriaNoName;
		return;
	}

	if (name.empty())
	{
		name = kMateriaNoName;
		return;
	}

	name = MakeFileNameValid (name);

	if (!CheckValidFileName(name))
	{
		ErrorString("Material name " + name + " couldn't be used because it is not a valid file name. Reverting to '" + kMateriaNoName + "'");
		name = kMateriaNoName;
	}
}

}

void ModelImporter::ValidateAndClearTextureFileName(ImportTexture& texture, const std::string& assetPathName, const Renderer& renderer, const std::string& materialName)
{
	AssertMsg(texture.path.find('\\') == std::string::npos, "Filename '%s' contains a forward slash", texture.path.c_str());
	AssertMsg(texture.relativePath.find('\\') == std::string::npos, "Filename '%s' contains a forward slash", texture.relativePath.c_str());

	const string textureName = GetLastPathNameComponent(texture.path);
	if (CheckValidFileNameDetail(textureName) == kFileNameInvalid)
	{
		LogImportWarningObject(Format("'%s' is not a valid texture file name on asset '%s' on material '%s' of renderer '%s'. The file will be ignored.", textureName.c_str(), assetPathName.c_str(), materialName.c_str(), renderer.GetName()), &renderer);

		texture.path = texture.relativePath = "";
	}
}

/// If path is inside a Textures folder "../Materials/textureName"
/// If path is not inside a Textures folder, "Materials/texturename"
string TexturePathToMaterialPath (const string& path)
{
	string textureFolder = DeleteLastPathNameComponent (path);
	string textureName = DeletePathNameExtension (GetLastPathNameComponent (path));

	string materialFolder;
	if (StrICmp (GetLastPathNameComponent (textureFolder), "Textures") == 0)
		materialFolder = AppendPathName (DeleteLastPathNameComponent (textureFolder), "Materials");
	else
		materialFolder = AppendPathName (textureFolder, "Materials");

	string materialPath = AppendPathName (materialFolder, textureName);
	return materialPath;
}

Texture* ModelImporter::FindTexture (const vector<string>& texturePaths) const
{
	if (texturePaths.empty())
		return NULL;

	Texture2D* texture = NULL;
	for (int i=0;i<texturePaths.size ();i++)
	{
		string path = texturePaths[i];

		DebugAssertIf(path.find("..") != std::string::npos); // Texture paths should never be relative at this point
		DebugAssertIf(path.empty()); // Texture paths should never be empty

		texture = GetFirstDerivedObjectAtPath<Texture2D> (path);
		if (texture)
			return texture;

		// Maybe we haven't imported the texture yet?
		// Fbx does that when embedding textures!
		if (IsFileCreated (path))
		{
			UnityGUID guid = GetGUIDPersistentManager().CreateAsset(path);
			if (guid != UnityGUID() && !AssetDatabase::Get().IsAssetAvailable(guid))
			{
				AssetInterface::Get().ImportAtPathImmediate (path);
				texture = GetFirstDerivedObjectAtPath<Texture2D> (path);
				if (texture)
					return texture;
			}
		}
	}

	// Now we are just going to search anywhere in the project, gimme those textures!
	return FindAssetAnywhere<Texture2D>(texturePaths);
}

template <class TAsset>
TAsset* ModelImporter::FindAssetAnywhere(const vector<string>& texturePaths) const
{
	// Now we are just going to search anywhere in the project, gimme those textures!
	set<string> paths;
	AssetDatabase::Get().FindAssetsWithName(GetLastPathNameComponent(texturePaths[0]), paths);
	if (paths.empty ())
		return NULL;

	// If they are in sub folders of the suggested texture paths then we try them in the order of the texture paths
	for (int i=0;i<texturePaths.size();i++)
	{
		for (set<string>::iterator p=paths.begin();p != paths.end();p++)
		{
			// Optimized version of if (BeginsWith (foundPath, DeleteLastPathNameComponent(texturePaths)))
			const std::string& foundPath = *p;
			const std::string& suggestedDirectory = texturePaths[i];
			string::size_type lengthToCheck = GetLastPathNameComponent(suggestedDirectory.c_str(), suggestedDirectory.size()) - suggestedDirectory.c_str();
			if (lengthToCheck != 0)
			{
				lengthToCheck--;
				Assert(suggestedDirectory[lengthToCheck] == kPathNameSeparator);
			}

			if (StrNICmp (foundPath.c_str(), suggestedDirectory.c_str(), lengthToCheck) == 0)
			{
				TAsset* texture = GetFirstDerivedObjectAtPath<TAsset> (foundPath);
				if (texture)
					return texture;
			}
		}
	}

	// Otherwise just return any texture in the project
	for (set<string>::iterator p=paths.begin();p != paths.end();p++)
	{
		TAsset* texture = GetFirstDerivedObjectAtPath<TAsset> (*p);
		if (texture)
			return texture;
	}

	return NULL;
}

namespace
{
	void SetShaderTexture(Material* material, const std::string& propertyName, Texture* tex, const ImportTexture& offsetScale)
	{
		material->SetTexture(ShaderLab::Property(propertyName), tex);
		material->SetTextureOffset(ShaderLab::Property(propertyName), offsetScale.offset);
		material->SetTextureScale(ShaderLab::Property(propertyName), offsetScale.scale);
	}
}

Material* ModelImporter::CreateNewMaterial (const ImportMaterial& importMaterial)
{
	const std::string assetPathName = GetAssetPathName();

	vector<string>& bumpPaths = SuggestTexturePathNames(assetPathName, importMaterial.normalMap, true);
	Texture* bumpmap = FindTexture(bumpPaths);

	bool isTransparent = importMaterial.hasTransparencyTexture || importMaterial.diffuse.a < 0.99f;
	Shader* shader = !isTransparent ?
		(!bumpmap ? GetScriptMapper().GetDefaultShader() : GetScriptMapper().FindShader("Bumped Diffuse")) :
		(!bumpmap ? GetScriptMapper().FindShader("Transparent/Diffuse") : GetScriptMapper().FindShader("Transparent/Bumped Diffuse"));

	if (shader == NULL)
		return NULL;

	Material* material = Material::CreateMaterial (*shader, 0);
	material->SetName(importMaterial.name.c_str());

	// Assign diffuse color
	material->SetColor (ShaderLab::Property("_Color"), importMaterial.diffuse);

	// Assign texture
	vector<string>& texturePaths = SuggestTexturePathNames(assetPathName, importMaterial.texture, true);
	Texture* tex = FindTexture(texturePaths);
	SetShaderTexture(material, "_MainTex", tex, importMaterial.texture);

	// Assign bumpmap
	if (bumpmap)
		SetShaderTexture(material, "_BumpMap", bumpmap, importMaterial.normalMap);

	//
	material->AwakeFromLoad(kDefaultAwakeFromLoad);

	return material;
}

Material* FindMaterial (const string& path)
{
	return AssetImporter::GetFirstDerivedObjectAtPath<Material> (AppendPathNameExtension (path, kMaterialExtension));
}

static Material* GetDefaultMaterial ()
{
	// store PPtr instead of raw pointer, as the material might be unloaded whenever editor feels like it
	static PPtr<Material> gDefaultMaterial;
	if (gDefaultMaterial.IsNull())
		gDefaultMaterial = GetBuiltinExtraResource<Material> ("Default-Diffuse.mat");
	return gDefaultMaterial;
}

PPtr<Material> ModelImporter::CreateSharedMaterial(const ImportMaterial& importMaterial)
{
	std::vector<std::string>& paths = SuggestTexturePathNames(GetAssetPathName(), importMaterial.texture, false);

	// Search for already generated materials.
	for (int i=0;i<paths.size ();i++)
	{
		Material* material = FindMaterial(TexturePathToMaterialPath(paths[i]));
		if (material)
			return material;
	}

	// We will have to create a material from scratch!
	// Search for the texture!
	for (int i=0;i<paths.size ();i++)
	{
		Texture2D* texture = GetFirstDerivedObjectAtPath<Texture2D>(paths[i]);
		if (texture)
		{
			ImportMaterial im = importMaterial;
			im.name = TexturePathToMaterialPath(paths[i]);

			return CreateMaterialAsset(CreateNewMaterial(im), im.name);
		}
	}

	return NULL;
}

// - NULL if no material is attached to the geometry in the scene file
// - If the texture can be found creates a material in a folder "Materials" in the same folder as the texture
// - If the texture can not be found creates a material in a folder "Materials" in the same folder as the scene
//   with the name the material has in the scene file
PPtr<Material> ModelImporter::InstantiateImportMaterial (int index, Renderer& renderer, ModelImportData& importData)
{
	ImportScene& scene = importData.scene;

	if (index >= 0 && index < scene.materials.size())
	{
		const std::string& assetPathName = GetAssetPathName();

		ImportMaterial& importMaterial = scene.materials[index];
		// valid and clear texture paths if they are invalid
		ValidateAndClearTextureFileName(importMaterial.texture, assetPathName, renderer, importMaterial.name);
		ValidateAndClearTextureFileName(importMaterial.normalMap, assetPathName, renderer, importMaterial.name);
	}

	// Let an editor script find & assign the right material
	if (MonoProcessMeshHasAssignMaterial())
	{
		PPtr<Material> tempMaterial = (index >= 0 && index < importData.instantiatedMaterials.size ()) ?
			CreateNewMaterial(scene.materials[index]) : GetDefaultMaterial();

		// Create a material as imported from fbx
		// Pass the material generated from the fbx file to the postprocessor
		PPtr<Material> assignedMaterial = MonoProcessAssignMaterial(renderer, *tempMaterial);

		if (assignedMaterial.IsValid() && !assignedMaterial->IsPersistent())
		{
			ErrorString("OnAssignMaterial must return a persistent material (AssetDatabase.CreateAsset)");
			assignedMaterial = NULL;
		}

		// Delete unless the user made it persistent, which probably meant he wanted to use it for something.
		if (tempMaterial.IsValid() && !tempMaterial->IsPersistent())
		{
			DestroySingleObject(tempMaterial);
		}

		if (assignedMaterial)
			return assignedMaterial;
	}

	if (index == -1)
		return GetDefaultMaterial ();

	if (index >= importData.instantiatedMaterials.size ())
	{
		ErrorString ("Failed loading material because the material index it is out of bounds!");
		return GetDefaultMaterial ();
	}

	if (!m_ImportMaterials)
		return GetDefaultMaterial ();

	if (importData.instantiatedMaterials[index].IsValid())
		return importData.instantiatedMaterials[index];

	ImportMaterial importMaterial = scene.materials[index];

	MaterialName materialNameOption = m_MaterialName;
	MaterialSearch materialSearchOption = m_MaterialSearch;

	if (materialNameOption == kMaterialNameBasedOnTextureName_Before35)
	{
		// This is backwards Unity 3.4 (and earlier) compatible behavior
		// If imported material has texture assigned and texture exists in Unity project folder in one of Textures folder, then create material <textureName>.mat
		// Else try to locate or create new <modelName>-<materialName>.mat material in local Materials folder
		PPtr<Material> sharedMaterial = CreateSharedMaterial(importMaterial);
		if (sharedMaterial)
			return importData.instantiatedMaterials[index] = sharedMaterial;
		else
		{
			materialNameOption = kMaterialNameBasedOnModelAndMaterialName;
			materialSearchOption = kMaterialSearchLocal;
		}
	}

	std::string materialName;
	switch(materialNameOption)
	{
	case kMaterialNameBasedOnTextureName:
		materialName = GetFileNameWithoutExtension(importMaterial.texture.path.empty() ? importMaterial.texture.path : importMaterial.texture.relativePath);
		// if material has no texture assigned, then fallback to <materialName>.mat mode
		if (materialName.empty())
			materialName = importMaterial.name;
		break;
	case kMaterialNameBasedOnMaterialName:
		materialName = importMaterial.name;
		break;
	case kMaterialNameBasedOnModelAndMaterialName: {
		//std::string materialFolder = AppendPathName (DeleteLastPathNameComponent (GetAssetPathName ()), "Materials");
		std::string assetName = GetFileNameWithoutExtension(GetAssetPathName());
		materialName = assetName + "-" + (importMaterial.name.empty() ? kMateriaNoName : importMaterial.name);
		} break;
	default:
		ErrorStringMsg("Internal Error: Unknown m_MaterialName mode: %d", m_MaterialName);
		break;
	}

	// This will assign "No Name" to materials without a name
	CleanupMaterialName(materialName);

	const std::string assetPath = DeleteLastPathNameComponent(GetAssetPathName());
	Material* material = NULL;

	// Recursive-up search:
	// MeshFolder/Materials/materialName
	// ../MeshFolder/Materials/materialName
	// ../../MeshFolder/Materials/materialName
	// And so on

	std::vector<std::string> paths;
	std::string currentFolder = assetPath;
	// ToLower is called in the loop, because we need to keep correct upper/lower case of currentFolder
	while(!material && ToLower(currentFolder).find("assets") != std::string::npos)
	{
		const std::string materialPath = AppendPathName(AppendPathName(currentFolder, "Materials"), materialName);
		paths.push_back(materialPath + ".mat");

		material = FindMaterial(materialPath);

		currentFolder = DeleteLastPathNameComponent(currentFolder);

		// we stop iteration if the setting says to search only in MeshFolder/Materials/materialName
		if (materialSearchOption == kMaterialSearchLocal)
			break;
	}

	if (!material && materialSearchOption == kMaterialSearchEverywhere)
		material = FindAssetAnywhere<Material>(paths);

	if (material == NULL)
	{
		// materialPath = "asset path name/Materials/materialName.mat"
		const std::string materialPath = AppendPathName(AppendPathName(assetPath, "Materials"), materialName);

		// TODO : do we need to set this?
		importMaterial.name = materialPath;
		material = CreateMaterialAsset(CreateNewMaterial(importMaterial), importMaterial.name);
	}

	// Fallback to default material!
	if (material == NULL)
		material = GetDefaultMaterial ();

	importData.instantiatedMaterials[index] = material;
	return material;
}

void ConvertGameObjectHierarchyToPrefabUsingPrefabParent(GameObject& root, Prefab* prefab)
{
	prefab->m_IsPrefabParent = true;
	prefab->m_RootGameObject = &root;

	root.m_Prefab = prefab;

	set<SInt32> collectedPtrs;
	CollectPPtrs(root, &collectedPtrs);

	for (set<SInt32>::const_iterator i = collectedPtrs.begin ();i != collectedPtrs.end ();i++)
	{
		EditorExtension* object = dynamic_instanceID_cast<EditorExtension*> (*i);
		if (object != NULL)
			object->m_Prefab = prefab;
		SetReplacePrefabHideFlags(*object);
	}
}

static bool IsRootGameObjectOrComponent (Object& object)
{
	Unity::GameObject* go = dynamic_pptr_cast<Unity::GameObject*> (&object) ;
	Unity::Component* component = dynamic_pptr_cast<Unity::Component*> (&object) ;
	Transform * transform = NULL;
	if ( component )
		transform = &( component->GetComponent(Transform) );
	else if ( go )
		transform = &( go->GetComponent(Transform) );

	return transform == NULL || transform->GetParent() == NULL;
}

const char* GetGameObjectStableIdentifier (Object& object)
{
	if (IsRootGameObjectOrComponent(object))
		return "//RootNode";
	else
		return object.GetName();
}

Prefab& ModelImporter::GeneratePrefab (GameObject& root, const string& prefabName)
{
	// Create Datatemplate
	Prefab &prefab = ProduceAssetObject<Prefab> ();
	prefab.m_IsPrefabParent = true;

	#if !UNITY_RELEASE
	// DataTemplates are *very* special. Call AwakeFromLoad only where it is needed
	prefab.HackSetAwakeWasCalled();
	#endif

	// Name the prefab by the asset name!
	prefab.SetName (prefabName.c_str());

	// Make GameObject hierarchy into prefab objects in prefab
	ConvertGameObjectHierarchyToPrefabUsingPrefabParent(root, &prefab);

	vector<Object*> objects;
	GetObjectArrayFromPrefabRoot(prefab, objects);

	// Sort objects by name so that register asset is better at producing completely deterministic file id's
	sort(objects.begin(), objects.end(), smaller_name_classid());

	for (int i=0;i<objects.size();i++)
	{
		Object& object = *objects[i];
		object.SetHideFlagsObjectOnly(object.GetHideFlags() | kNotEditable);

		RegisterObject (object, GetGameObjectStableIdentifier(object));
	}
	prefab.SetHideFlagsObjectOnly(kNotEditable | kHideInHierarchy);
	root.ActivateAwakeRecursively();

	return prefab;
}

struct ExtractedNameAndClip
{
	const char* name;
	int         nameLength;
	const char* clip;
};

bool ExtractNameAndClipFromName(const string& fileName, ExtractedNameAndClip& extracted)
{
	string::size_type atSign = fileName.find ("@");
	if (atSign != string::npos && atSign+1 != fileName.size ())
	{
		extracted.name = fileName.c_str();
		extracted.nameLength = atSign;
		extracted.clip = fileName.c_str() + atSign + 1;
		return true;
	}
	else
	{
		return false;
	}
}

string GetBaseAssetName (const string& assetPath)
{
	return DeletePathNameExtension(GetLastPathNameComponent(assetPath));
}

static pair<string, string> ExtractNameAndClipFromName(string fileName)
{
	string clip;
	string name;

	ExtractedNameAndClip nameAndClip;
	if (ExtractNameAndClipFromName (fileName, nameAndClip))
	{
		clip.assign (nameAndClip.clip);
		name.assign (nameAndClip.name, nameAndClip.nameLength);
	}
	else
	{
		name = fileName;
	}

	return make_pair (name, clip);
}

static pair<string, string> ExtractNameAndClipFromPath(string fileName)
{
	return ExtractNameAndClipFromName (GetBaseAssetName (fileName));
}

string GetAssetNameWithoutExtension (const std::string& assetPath)
{
	return DeletePathNameExtension(GetLastPathNameComponent(assetPath));
}

void CollectNecessaryReferencedAnimationClipDependencies (const std::string& assetPath, vector<UnityGUID>& dependencies)
{
	UnityGUID parent;
	if (!GetGUIDPersistentManager().PathNameToGUID (DeleteLastPathNameComponent(assetPath), &parent))
		return;

	const Asset* parentAsset = AssetDatabase::Get().AssetPtrFromGUID(parent);
	if (parentAsset == NULL)
		return;

	AssetDatabase& db = AssetDatabase::Get();
	string assetName = GetBaseAssetName(assetPath);

	// Only assets with no @ sign should pull in other animations.
	ExtractedNameAndClip nameAndClip;
	if (ExtractNameAndClipFromName(assetName, nameAndClip))
		return;

	for (int i=0;i<parentAsset->children.size();i++)
	{
		UnityGUID childGUID = parentAsset->children[i];
		const Asset* childAsset = db.AssetPtrFromGUID (childGUID);
		if (childAsset == NULL)
			continue;

		ExtractedNameAndClip nameAndClip;

		// When looking for animations to attach -> the referenced animation needs to have an @ sign
		if (!ExtractNameAndClipFromName(childAsset->mainRepresentation.name, nameAndClip))
			continue;

		if (StrNICmp(nameAndClip.name, assetName.c_str(), nameAndClip.nameLength) == 0)
			dependencies.push_back(childGUID);
	}
}

static UnityStr GetBaseModelOfAtConventionClipPath (const UnityStr& assetPath)
{
	string assetName = GetAssetNameWithoutExtension (assetPath);

	ExtractedNameAndClip nameAndClip;
	if (!ExtractNameAndClipFromName (assetName, nameAndClip))
		return std::string();

	UnityGUID guid;
	if (!GetGUIDPersistentManager().PathNameToGUID (DeleteLastPathNameComponent(assetPath), &guid))
		return std::string();

	AssetDatabase& db = AssetDatabase::Get();

	const Asset* parentAsset = db.AssetPtrFromGUID(guid);
	if (parentAsset == NULL)
		return std::string();

	// Find the asset with the basename matching nameAndClip
	vector<UnityGUID> assetsToReimport;
	for (int i=0;i<parentAsset->children.size();i++)
	{
		UnityGUID childGUID = parentAsset->children[i];
		const Asset* childAsset = db.AssetPtrFromGUID (childGUID);
		if (childAsset == NULL)
			continue;

		// We found a base asset for the animation clip
		UnityStr name(nameAndClip.name, nameAndClip.nameLength);
		if (name == childAsset->mainRepresentation.name)
			return GetAssetPathFromGUID(childGUID);
	}

	return string();
}


void ForceReimportOfAddedAnimationModelFiles (const std::string& assetPath, const std::set<UnityGUID>& added)
{
	string assetName = GetAssetNameWithoutExtension (assetPath);

	ExtractedNameAndClip nameAndClip;
	if (!ExtractNameAndClipFromName (assetName, nameAndClip))
		return;

	UnityGUID guid;
	if (!GetGUIDPersistentManager().PathNameToGUID (DeleteLastPathNameComponent(assetPath), &guid))
		return;

	AssetDatabase& db = AssetDatabase::Get();

	const Asset* parentAsset = db.AssetPtrFromGUID(guid);
	if (parentAsset == NULL)
		return;

	// Find the asset with the basename matching nameAndClip
	vector<UnityGUID> assetsToReimport;
	for (int i=0;i<parentAsset->children.size();i++)
	{
		UnityGUID childGUID = parentAsset->children[i];
		const Asset* childAsset = db.AssetPtrFromGUID (childGUID);
		if (childAsset == NULL)
			continue;

		// We found a base asset for the animation clip
		UnityStr name(nameAndClip.name, nameAndClip.nameLength);
		if (name == childAsset->mainRepresentation.name)
		{
			// This base asset was already import in this import step, since @ animation files are imported first, there is no need to reimport assets that have been imported in the same import step.
			// So we only reimport it if it was not added in this import step.
			if (!added.count(childGUID))
				assetsToReimport.push_back(childGUID);
		}
	}
	AssetInterface::Get().ImportAssets(assetsToReimport);
}

// Forces reimport of the base file for @ animation files
// eg. soldier@run.fbx will cause solider.fbx to be imported when it is added.
static void OnAddedAnimationPostprocessCallback (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved)
{
	for(std::set<UnityGUID>::const_iterator i=added.begin();i != added.end();++i)
	{
		string path = GetAssetPathFromGUID(*i);
		ForceReimportOfAddedAnimationModelFiles	(path, added);
	}

	for(std::map<UnityGUID, std::string>::const_iterator i=moved.begin();i != moved.end();++i)
	{
		string path = GetAssetPathFromGUID(i->first);
		ForceReimportOfAddedAnimationModelFiles	(path, added);
	}
}

namespace
{
	const char* GenerateAnimationsOptionToString(ModelImporter::LegacyGenerateAnimations value)
	{
		switch(value)
		{
		case ModelImporter::kDontGenerateAnimations: return "Don't Import";
		case ModelImporter::kGenerateAnimationsInOriginalRoots: return "Store in Original Roots (Deprecated)";
		case ModelImporter::kGenerateAnimationsInNodes: return "Store in Nodes (Deprecated)";
		case ModelImporter::kGenerateAnimationsInRoot: return "Store in Root (Deprecated)";
		case ModelImporter::kGenerateAnimations: return "Store in Root";
		default:
			ErrorStringMsg("Unsupported GenerateAnimations value: %d", value);
			return "";
		}
	}

	const char* GenerateAnimationTypeToString(ModelImporter::AnimationType value)
	{
		switch(value)
		{
			case ModelImporter::kLegacy: return "Legacy";
			case ModelImporter::kGeneric: return "Generic";
			case ModelImporter::kHumanoid: return "Humanoid";
			default:
				ErrorStringMsg("Unsupported animation type value: %d", value);
				return "";
		}
	}

}

void ModelImporter::GenerateReferencedClipDependencyList ()
{
	m_Output.referencedClips.clear();
	CollectNecessaryReferencedAnimationClipDependencies (GetAssetPathName(), m_Output.referencedClips);
}

/// Connects external animation clips that follow the monster@run.fbx naming scheme.
/// - All animations files named @whatever get attached to the base file with the same name but without @.
/// - When a new animation is added with the @sign we reimport the base file
void ModelImporter::ConnectExternalAnimationClips (GameObject& rootGO)
{
	if (m_AnimationType != kLegacy)
		return;

	if (m_Output.referencedClips.empty())
		return;

	// Only the legacy animation system supports automatically adding animation clips to models,
	// so if mecanim is used skip this part
	if (m_AnimationType != kLegacy)
		return;

		// Build a lookup from root name to to animation component
	// We use this to match other roots to the roots in this scene.
	map<string, Animation*> nameToAnimationComponent;
	for (Roots::iterator i=m_Output.animationRoots.begin();i!=m_Output.animationRoots.end();i++)
	{
		string name = ExtractNameAndClipFromName ((**i).GetName()).first;
		nameToAnimationComponent[name] = (**i).QueryComponent(Animation);
	}

	std::set<std::string> newAnimationClipNames;

	// Loop all meshes in the same folder
	for (int i=0;i<m_Output.referencedClips.size();i++)
	{
		// Make sure we are dealing with an mesh importer
		string path = GetAssetPathFromGUID(m_Output.referencedClips[i]);
		ModelImporter* animationImporter = dynamic_pptr_cast<ModelImporter*> (FindAssetImporterAtPath (GetMetaDataPathFromAssetPath (path)));
		if (animationImporter == NULL)
			continue;

		Assert(animationImporter != this);

		// Go through all roots

		ModelImportOutput& referencedImporterOutput = animationImporter->m_Output;

		for (Roots::iterator i=referencedImporterOutput.animationRoots.begin();i != referencedImporterOutput.animationRoots.end();i++)
		{
			// We need to match the Root game object from the other imported scene,
			// to the scene we are currently importing
			GameObject* otherGO = *i;
			if (otherGO == NULL || otherGO->QueryComponent(Animation) == NULL)
				continue;

			Animation& otherAnimation = otherGO->GetComponent(Animation);
			string gameObjectName = ExtractNameAndClipFromName(otherAnimation.GetName()).first;
			Animation* thisAnimation = nameToAnimationComponent[gameObjectName];
			if (thisAnimation == NULL)
				continue;

			// This is model file - pull the animations from the other scene into this scene
			const Animation::Animations& clips = otherAnimation.GetClips();
			for (Animation::Animations::const_iterator it = clips.begin(); it != clips.end(); ++it)
			{
				PPtr<AnimationClip> otherClip = *it;
				if (!otherClip.IsValid())
					continue;

				// Add the clip only if there is none with that name already
				const std::string& animationClipName = otherClip->GetName();
				AnimationClip* clip = thisAnimation->GetClipWithNameSerialized(animationClipName);
				if (!clip)
				{
					thisAnimation->AddClip(*otherClip);
					if (!thisAnimation->GetClip())
						thisAnimation->SetClip(otherClip);

					newAnimationClipNames.insert(animationClipName);

					// Validate that GlobalScale and GenerateAnimations options match
					if (fabsf(GetGlobalScale() - animationImporter->GetGlobalScale()) > 1e-6f)
					{
						LogImportWarning(Format("GlobalScale (%g) on referenced animation '%s' doesn't match GlobalScale (%g) on this file.",
							animationImporter->GetGlobalScale(), GetLastPathNameComponent(path).c_str(), GetGlobalScale()));
					}

					if (GetAnimationType() != animationImporter->GetAnimationType())
					{
						LogImportWarning(Format("AnimationType option ('%s') on referenced animation '%s' doesn't match AnimationType option ('%s') on this file.",
												GenerateAnimationTypeToString(animationImporter->GetAnimationType()), GetLastPathNameComponent(path).c_str(),
												GenerateAnimationTypeToString(GetAnimationType()))
										 );
					}

					if (GetAnimationType() == kLegacy && GetLegacyGenerateAnimations() != animationImporter->GetLegacyGenerateAnimations())
					{
						LogImportWarning(Format("GenerateAnimations option ('%s') on referenced animation '%s' doesn't match GenerateAnimations option ('%s') on this file.",
							GenerateAnimationsOptionToString(animationImporter->GetLegacyGenerateAnimations()), GetLastPathNameComponent(path).c_str(),
							GenerateAnimationsOptionToString(GetLegacyGenerateAnimations()))
						);
					}

					if (m_Output.hasExtraRoot != referencedImporterOutput.hasExtraRoot)
					{
						LogImportWarning(Format("Imported animation ('%s') doesn't match because there is a different amount of root nodes in the model file. Please unify the model and animation file, otherwise you won't be able to play animation on the model.",
												GetLastPathNameComponent(path).c_str())
										 );
					}
				}
			}
	}
	}

	if (!newAnimationClipNames.empty())
	{
		std::set<Animation*> animationComponents;
		for (Roots::iterator it = m_Output.animationRoots.begin(), end = m_Output.animationRoots.end(); it != end; ++it)
		{
			Animation* animation = (*it)->QueryComponent(Animation);
			if (animation)
			{
				bool res = animationComponents.insert(animation).second;
				Assert(res);
			}
		}

		CalculateClipsBoundsDeprecated(rootGO, newAnimationClipNames);
	}
}



struct SortingNode
{
	string m_Path;
	int m_Depth;
	ImportNode *m_Node;

	SortingNode(string path, int depth, ImportNode* node)
	: m_Path(path)
	, m_Depth(depth)
	, m_Node(node)
	{}
};

bool operator <(const SortingNode &a,const SortingNode &b)
{
	if(a.m_Node->name > b.m_Node->name)
		return false;
	if(a.m_Node->name < b.m_Node->name)
		return true;
	if(a.m_Depth > b.m_Depth)
		return false;
	if(a.m_Depth < b.m_Depth)
		return true;
	if(a.m_Path > b.m_Path)
		return false;
	if(a.m_Path < b.m_Path)
		return true;
	return false;
}

typedef vector<SortingNode> SortingNodes;


static void FillSortingNodes(ImportNode &node, int depth, string path, SortingNodes &nodes)
{
	SortingNode sort(path, depth, &node);
	nodes.push_back(sort);

	for (ImportNodes::iterator i=node.children.begin ();i != node.children.end ();i++)
		FillSortingNodes(*i,depth+1,path+"/"+node.name,nodes);
}

static void GuaranteeUniqueNodeNames(ImportScene& scene)
{
	bool foundDupes = true;
	while (foundDupes)
	{
		foundDupes = false;
		vector<SortingNode> nodes;
		for (ImportNodes::iterator i=scene.nodes.begin ();i != scene.nodes.end ();i++)
			FillSortingNodes(*i,0,"",nodes);

		std::sort(nodes.begin(), nodes.end());
		 
		string lastName;
		int sameNames = 0;
		for (int i=0;i<nodes.size();i++)
		{
			string name = nodes[i].m_Node->name;
			if(name == lastName)
			{
				sameNames++;
				nodes[i].m_Node->name += Format(" %d",sameNames);
				foundDupes = true;
			}
			else
				sameNames = 0;
			lastName = name;
		}
	}
}

void ModelImporter::PatchRecycleIDsFromPrefab()
{
	GetPersistentManager ().LoadFileCompletely (GetMetaDataPath());
	int instanceID = GetPersistentManager ().GetInstanceIDFromPathAndFileID (GetMetaDataPath(), kMaxObjectsPerClassID * ClassID(Prefab));
	Prefab* prefab = dynamic_instanceID_cast<Prefab*>(instanceID);
	if (prefab == NULL)
	{
		// Nothing to patch, this model has not previously been imported, so the root prefab does not exist
		return;
	}

	set<SInt32> collectedPtrs;
	CollectPPtrs(*prefab->GetRootGameObject(), &collectedPtrs);

	for (set<SInt32>::const_iterator i = collectedPtrs.begin ();i != collectedPtrs.end ();i++)
	{
		EditorExtension* object = dynamic_instanceID_cast<EditorExtension*> (*i);
		LocalIdentifierInFileType fileID = GetPersistentManager().GetLocalFileID(object->GetInstanceID());
		string name = GetGameObjectStableIdentifier(*object);

		if (m_FileIDToRecycleName.count(fileID) == 0)
		{
			m_FileIDToRecycleName.insert (make_pair (fileID, name));
			m_RecycleNameToFileID.insert (make_pair (name, fileID));
		}
	}
}

void ModelImporter::GenerateAll (ModelImportData& importData)
{
	GuaranteeUniqueNodeNames(importData.scene);

	ClearPreviousImporterOutputs ();

	ImportScene& scene = importData.scene;

	// Apply 3.5 and before backwards compatibility when there are no .meta files for game object hierachy LocalIdentifierInFile mappings.
	if (m_NeedToPatchRecycleIDsFromPrefab)
	{
		m_NeedToPatchRecycleIDsFromPrefab = false;
		PatchRecycleIDsFromPrefab();
	}

	UnloadAllLoadedAssetsAtPathAndDeleteMetaData(GetMetaDataPath(), this);

	string prefabName = DeletePathNameExtension(GetLastPathNameComponent (GetAssetPathName ()));

	importData.instantiatedMeshes.resize (scene.meshes.size ());
	importData.instantiatedMaterials.resize (scene.materials.size ());

	// Import all nodes (Implicitly generate meshes and materials)
	dynamic_array<PPtr<GameObject> > roots;
	for (ImportNodes::iterator i=scene.nodes.begin ();i != scene.nodes.end ();i++)
		roots.push_back (&InstantiateImportNode (*i, NULL, importData));

	// Override the animation clip's name (Everything after the @ sign in the filename is the remapped take name)
	UnityStr remapTakeName = ExtractNameAndClipFromPath (GetAssetPathName ()).second;

	///
	/// support for old generate animation mode
	///

	if (ShouldImportAnimationDeprecatedWithMultipleRoots ())
		DeprecatedGenerateAndAssignClipsWithMultipleRoots(remapTakeName, scene);

	m_Output.hasExtraRoot = false;
	GameObject* root = NULL;

	// Place a root object around all imported objects so we end up with a single root object!
	// if referenced model has extra root then we add extra root to this animation
	if (roots.size() > 1 || scene.sceneInfo.hasSkeleton)
	{
		root = &CreateGameObjectWithHideFlags (prefabName, true, 0, "Transform", NULL, NULL);

		for (int i=0;i<roots.size();i++)
		{
			Transform* transform = roots[i]->QueryComponent (Transform);
			if (transform)
				transform->SetParent (root->QueryComponent (Transform));
		}

		m_Output.hasExtraRoot = true;
	}
	// Use the one root object as the root but rename it to the prefabname
	else if (roots.size () == 1)
	{
		root = roots[0];
		root->SetName (prefabName.c_str());
	}
	// Generate at least one dummy object.
	else
	{
		root = &CreateGameObjectWithHideFlags (prefabName, true, 0, "Transform", NULL, NULL);
	}

	bool didImportSkinnedMesh = false;
	if (ShouldImportSkinnedMesh ())
	{
		// Import skinned meshes
		for (ImportNodes::iterator i=scene.nodes.begin ();i != scene.nodes.end ();i++)
			didImportSkinnedMesh |= ImportSkinnedMesh (*i, scene, root->GetComponent(Transform));
	}


	// Create animation component on the root and setup culling based on renderers
	// Only if we have a skinned mesh renderer
	if (m_AnimationType == kLegacy)
	{
		AddComponent(*root, "Animation");

		if (didImportSkinnedMesh)
			root->GetComponent(Animation).SetCullingType(Animation::kCulling_BasedOnRenderers);
	}

	// Assign the animation track to clips in the root after generating the root wrapper object
	if (ShouldImportAnimationInRoot ())
	{
		GenerateAnimationClips(*root, remapTakeName, importData);

		ClipAnimations clipAnimations;
		vector<string> internalClipNames;

		if(ShouldSplitAnimations())
		{
			clipAnimations = m_ClipAnimations;
			internalClipNames.resize(m_ClipAnimations.size());
			for (int i=0;i<clipAnimations.size();i++)
				internalClipNames[i] = clipAnimations[i].name;
		}
		else
		{
			clipAnimations.resize(m_Output.importedTakeInfos.size());
			internalClipNames.resize(m_Output.importedTakeInfos.size());

			for(int takeIter = 0; takeIter < m_Output.importedTakeInfos.size(); takeIter++)
			{
				TakeInfo takeInfo = m_Output.importedTakeInfos[takeIter];

				ClipAnimationInfo clipInfo;
				clipInfo.takeName = takeInfo.name;
				clipInfo.name = takeInfo.defaultClipName;
				clipInfo.firstFrame = (ShouldUseCorrectClipTimeValues() ? takeInfo.bakeStartTime : takeInfo.startTime) * takeInfo.sampleRate;
				clipInfo.lastFrame  = (ShouldUseCorrectClipTimeValues() ? takeInfo.bakeStopTime : takeInfo.stopTime  ) * takeInfo.sampleRate;
				clipInfo.wrapMode = m_AnimationWrapMode;

				clipAnimations[takeIter] = clipInfo;
				internalClipNames[takeIter] = takeInfo.name;
			}
		}

		SplitAnimationClips(*root, clipAnimations, internalClipNames, ShouldSplitAnimations() || ShouldUseCorrectClipTimeValues (), importData);
	}

	// Create avatar
	ImportAvatar(*root, didImportSkinnedMesh);

	string error = GenerateLODGroupFromModelImportNamingConvention(*root, m_LODScreenPercentages);
	if (!error.empty())
		LogImportWarning(error);

	if (m_OptimizeGameObjects)
		OptimizeTransformHierarchy(*root, m_ExtraExposedTransformPaths.data(), m_ExtraExposedTransformPaths.size());

	if (m_AnimationType == kLegacy)
		CalculateClipsBoundsDeprecated(*root);
	else
		PrecalculateSkinnedMeshRendererBoundingVolumes(*root);

	// Script based post processing, returns false if the root object has been deleted by the script
	// in that case we simply create a new empty GameObject
	if (!MonoPostprocessMesh (*root, GetAssetPathName()))
		root = &CreateGameObjectWithHideFlags (prefabName, true, 0, "Transform", NULL);

	// Generate prefab
	Prefab& dt = GeneratePrefab (*root, prefabName);

	// Generate m_Roots array
	// - Fetch prefab root
	// - Roots are all children of the prefabroot or the prefab root itself
	GameObject* prefabbedRoot = dt.GetRootGameObject();
	importData.importedRoot = prefabbedRoot;

	m_Output.animationRoots.clear();

	if (roots.size () > 1 && ShouldImportLegacyInOriginalRoots ())
	{
		AssertIf(!prefabbedRoot);
		m_Output.animationRoots.reserve(roots.size ());
		Transform& prefabRootTransform = prefabbedRoot->GetComponent(Transform);
		for (Transform::iterator i=prefabRootTransform.begin();i != prefabRootTransform.end();i++)
			m_Output.animationRoots.push_back(&(**i).GetGameObject());
	}
	else
	{
		m_Output.animationRoots.push_back(prefabbedRoot);
	}

	GenerateReferencedClipDependencyList ();

	// Connect external animations using monster@run.fbx naming scheme
	ConnectExternalAnimationClips(*prefabbedRoot);

	// Add prefab to queue of prefabs that need to be merged
	PPtr<Prefab> prefab (ExtractFinalPPtrForObject (dt).GetInstanceID());
	AssetImportPrefabQueue::Get().QueuePrefab(prefab);
}

void ModelImporter::GenerateAssetData ()
{
	MonoPreprocessMesh (GetAssetPathName());

	string extension = ToLower (GetPathNameExtension (GetAssetPathName ()));
	Roots roots;

	if (CompareApproximately (m_GlobalScale, 0.0F))
		InitDefaultValues();

	const float importStartTime = GetTimeSinceStartup();

	ModelImportData importData;
	bool success = DoMeshImport (importData.scene);

	if (success)
		GenerateAll (importData);
	else
	{
		// Revert to last import
		MarkAllAssetsAsUsed ();
	}

	{
		float importTime = GetTimeSinceStartup() - importStartTime;
		std::string extension = ToLower(GetPathNameExtension(GetAssetPathName()));
		AnalyticsTrackEvent("ModelImporter", "AssetImport", extension, RoundfToInt(importTime));
	}
}


void ModelImporter::ClearPreviousImporterOutputs ()
{
	m_Output = ModelImportOutput ();
}

float ModelImporter::GetDefaultScale()
{
	string extension = ToLower (GetPathNameExtension (GetAssetPathName ()));
	if (extension == "3ds")
		return  0.1F;
	else if (extension == "mb" || extension == "ma" )
		return 1.0F;
	else if ( extension == "max" )
		return 0.01F;
	else if (extension == "fbx")
		return 0.01F;
	else if (extension == "jas")
		return 0.01F;
	else if (extension == "c4d")
		return 0.01F;
	else if (extension == "lxo")
		return 1;
	else
		return 1.0F;
}


void ModelImporter::InitDefaultValues ()
{
	m_GlobalScale = GetDefaultScale();
	m_MeshSettings.swapUVChannels = false;
}

pair<int, int> ModelImporter::GetSplitAnimationRange ()
{
	AssertIf(!ShouldSplitAnimations ());
	pair<int, int> range;
	range.first = m_ClipAnimations.back().firstFrame;
	range.second = m_ClipAnimations.back().lastFrame;

	for (ClipAnimations::iterator i=m_ClipAnimations.begin();i != m_ClipAnimations.end();i++)
	{
		range.first = min<int>(FloorfToInt(i->firstFrame), range.first);
		range.second = max<int>(CeilfToInt(i->lastFrame) , range.second);
	}

	return range;
}

Material* CreateMaterialAsset (Material* material, const std::string& pathName)
{
	if (material == NULL)
		return NULL;

	if (!GetPersistentManager ().IsFileEmpty (pathName) || IsFileCreated (pathName))
		return NULL;

	CreateExternalAssetFolder (DeleteLastPathNameComponent (pathName));

	string folder = DeleteLastPathNameComponent (pathName);
	string assetName = GetLastPathNameComponent (pathName);

	UnityGUID folderGUID;
	if (!GetGUIDPersistentManager ().PathNameToGUID (folder, &folderGUID))
		return NULL;

	// This is needed when calling CreateMaterialAsset from drag and drop, during normal importing will not import immediately and thus not unload the asset.
	PPtr<Material> materialPPtr = material;

	AssetInterface::Get ().CreateSerializedAsset (*material, folderGUID, assetName, "mat", AssetInterface::kWriteAndImport);

	return materialPPtr;
}

void CreateExternalAssetFolder (const string& pathName)
{
	if (IsDirectoryCreated (pathName))
		return;

	if (CreateDirectory (pathName))
		AssetInterface::Get ().ImportAtPath (pathName);
}

static int FindRootIndex(Transform& transform, vector<pair<Transform*, int> >& roots)
{
	for (int i=0;i<roots.size();i++)
	{
		if (IsChildOrSameTransform(transform, *roots[i].first))
			return i;
	}
	return -1;
}

template<class T>
struct SortBySecondPair
{
	bool operator () (const T& lhs, const T& rhs)
	{
		return lhs.second < rhs.second;
	}
};

static void CalculateAllRootBonesSortedByNumberOfChildBones(const dynamic_array<PPtr<Transform> >& allBones, dynamic_array<Transform*>& rootsSortedByNumberOfChildren)
{
	// Sort by deepest transforms -------------wrong order
	std::multimap<int, Transform*> rootsSortedByDepth;
	for (int i=0;i<allBones.size();i++)
	{
		Transform* transform = allBones[i];
		if (transform != NULL)
			rootsSortedByDepth.insert(make_pair(GetTransformDepth(*transform), transform));
	}

	// Figure out the ones with the most child bones
	vector<pair<Transform*, int> > rootsAndNumberOfChildBones;
	for (std::multimap<int, Transform*>::iterator i=rootsSortedByDepth.begin();i != rootsSortedByDepth.end();++i)
	{
		Transform& transform = *i->second;

		int found = FindRootIndex(transform, rootsAndNumberOfChildBones);
		if (found == -1)
			rootsAndNumberOfChildBones.push_back(make_pair(&transform, 1));
		else
			rootsAndNumberOfChildBones[found].second++;
	}

	// Sort by number of child bones
	SortBySecondPair<pair<Transform*, int> > sorter;
	stable_sort(rootsAndNumberOfChildBones.begin(), rootsAndNumberOfChildBones.end(), sorter);

	for (int i=0;i<rootsAndNumberOfChildBones.size();i++)
		rootsSortedByNumberOfChildren.push_back(rootsAndNumberOfChildBones[i].first);
}

static void UpdateAffectedRootTransformDeprecated(Transform*& root, Transform& transform, bool& sendmessageToRoot)
{
	if (root != NULL)
	{
		if (root == &transform)
			return;

		// We need to find the root that encapsulates the old root and the new transform!
		while (root && !IsChildOrSameTransform(transform, *root))
		{
			root = root->GetParent();
			sendmessageToRoot = false;
		}
	}
	else
	{
		root = &transform;
		sendmessageToRoot = true;
	}
}

namespace
{
	///
	/// support for old generate animation mode
	///
	void AssignImportNodeAnimationToClip(const float globalScale, AnimationClip& clip, const Transform& attachToTransform, ImportNodeAnimation& import)
	{
		string path = CalculateTransformPath (GetTransformFromImportAnimation(import), &attachToTransform);

		// Scale the animation curves
		if (!CompareApproximately (globalScale, 1.0F))
		{
			ScaleCurveValue(import.translation[0], globalScale);
			ScaleCurveValue(import.translation[1], globalScale);
			ScaleCurveValue(import.translation[2], globalScale);
		}

		if (import.rotation[0].IsValid () && import.rotation[1].IsValid () && import.rotation[2].IsValid () && import.rotation[3].IsValid ())
		{
			AnimationCurveQuat rotation;

			CombineCurve(import.rotation[0], 0, rotation);
			CombineCurve(import.rotation[1], 1, rotation);
			CombineCurve(import.rotation[2], 2, rotation);
			CombineCurve(import.rotation[3], 3, rotation);
			clip.AddRotationCurve(rotation, path);
		}
		if (import.translation[0].IsValid () && import.translation[1].IsValid () && import.translation[2].IsValid ())
		{
			AnimationCurveVec3 pos;

			CombineCurve(import.translation[0], 0, pos);
			CombineCurve(import.translation[1], 1, pos);
			CombineCurve(import.translation[2], 2, pos);
			clip.AddPositionCurve(pos, path);
		}

		if (import.scale[0].IsValid () && import.scale[1].IsValid () && import.scale[2].IsValid ())
		{
			AnimationCurveVec3 scale;

			CombineCurve(import.scale[0], 0, scale);
			CombineCurve(import.scale[1], 1, scale);
			CombineCurve(import.scale[2], 2, scale);
			clip.AddScaleCurve(scale, path);
		}
	}

	void AssignImportFloatAnimationToClip(AnimationClip& clip, const Transform& attachToTransform, ImportFloatAnimation& import)
	{
		if (import.curve.IsValid())
		{
			// TODO : add hack for Camera child
			std::string path = CalculateTransformPath(GetTransformFromImportAnimation(import), &attachToTransform);
			//const int classID = ClassID(Camera);
			const int classID = Object::StringToClassID(import.className);
			// HACK : we know that we attach camera/light component to child of this node
			// so we need to forward any camera/light animations to that node too
			if (classID == ClassID(Camera))
			{
				if (!path.empty())
					path += "/";
				path += "Camera";
			}
			else if (classID == ClassID(Light))
			{
				if (!path.empty())
					path += "/";
				path += "Light";
			}
			clip.AddFloatCurve(import.curve, path, classID, import.propertyName);
		}
	}

	void AssignImportAnimationToClip(const float globalScale, AnimationClip& clip, const Transform& attachToTransform, ImportBaseAnimation& importAnimation, ImportAnimationType animationType)
	{
		switch (animationType)
		{
		case kImportAnimationType_Node:
			AssignImportNodeAnimationToClip(globalScale, clip, attachToTransform, static_cast<ImportNodeAnimation&>(importAnimation));
			break;
		case kImportAnimationType_Float:
			AssignImportFloatAnimationToClip(clip, attachToTransform, static_cast<ImportFloatAnimation&>(importAnimation));
			break;
		default:
			ErrorString("Uknown animation type");
		}
	}

	template <class TAnimation>
	bool GetNextAnimation(std::list<TAnimation>& animations, const ImportAnimationType animationTypeValue, ImportBaseAnimation*& animation, ImportAnimationType& animationType)
	{
		if (!animations.empty())
		{
			animation = &animations.front();
			animationType = animationTypeValue;
			return true;
		}
		else
			return false;
	}

	template <class TAnimation>
	void CollectAnimationsByNameAndTransform(const float globalScale, AnimationClip& sourceClip, const Transform* attachToTransform, const bool matchRootTransform, typename std::list<TAnimation>& animations, const ImportAnimationType animationsType)
	{
		typename std::list<TAnimation>::iterator it = animations.begin();
		for (; it != animations.end(); )
		{
			Transform* currentTransform = &GetTransformFromImportAnimation(*it);
			if (matchRootTransform)
				currentTransform = &currentTransform->GetRoot();

			if (currentTransform == attachToTransform)
			{
				AssignImportAnimationToClip(globalScale, sourceClip, *attachToTransform, *it, animationsType);
				it = animations.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
}

void ModelImporter::AddAnimationClip(Animation& animation, const std::string& attachToTransformName, AnimationClip& sourceClip,
	const std::string& animationName, const std::string& clipName, int wrapMode, bool setDefaultClip,
	bool clipClip, float firstFrame, float lastFrame, float sampleRate, bool loop, ImportScene& scene)
{
	////////@TODO: This is an incorrect identifier. It should be attachToTransformName can change based on the file name
	///////        We can't change this since it will break existing content. When switching to a hashing based system, we need to ensure
	//             that we do it properly then.
	const std::string clippedIdentifier = animationName + " //// " + attachToTransformName;

	ClipAnimationInfo clipInfo;
	clipInfo.name = clipName;
	clipInfo.loop = loop;
	clipInfo.wrapMode = wrapMode;
	clipInfo.firstFrame = firstFrame;
	clipInfo.lastFrame = lastFrame;

	AnimationClip& clip = CreateAnimClipFromSourceClip(clippedIdentifier, sourceClip, clipInfo, sampleRate, clipClip, animation.GetName(), scene);

	// Add clip to animation
	// - The first clipped animation is the default clip
	animation.AddClip (clip);
	if (!animation.GetClip () || setDefaultClip)
		animation.SetClip (&clip);
}

void ModelImporter::DeprecatedGenerateAndAssignClipsWithMultipleRoots (string remapTakeName, ImportScene& scene)
{
	for (std::vector<ImportAnimationClip>::iterator itClip = scene.animationClips.begin(); itClip != scene.animationClips.end(); ++itClip)
	{
		ImportAnimationClip& clip = *itClip;

		// TODO : this still does look up of AnimationClip - we could get rid of that

		ImportBaseAnimation* baseAnim;
		ImportAnimationType animationType;
		while ( GetNextAnimation(clip.nodeAnimations, kImportAnimationType_Node, baseAnim, animationType) ||
				GetNextAnimation(clip.floatAnimations, kImportAnimationType_Float, baseAnim, animationType))
		{
			Assert(baseAnim);

			AnimationClip& sourceClip = *CreateObjectFromCode<AnimationClip>();
			sourceClip.SetSampleRate(scene.sampleRate);

			Transform* attachToTransform = &GetTransformFromImportAnimation(*baseAnim);

			// Generate the complete animation clip
			// - attached to the individual nodes
			if (m_LegacyGenerateAnimations == kGenerateAnimationsInNodes)
			{
				if (animationType == kImportAnimationType_Node)
				{
					AssignImportAnimationToClip(m_GlobalScale, sourceClip, *attachToTransform, *baseAnim, animationType);
					clip.nodeAnimations.erase(clip.nodeAnimations.begin());
				}

				CollectAnimationsByNameAndTransform(m_GlobalScale, sourceClip, attachToTransform, false, clip.floatAnimations, kImportAnimationType_Float);
			}
			// - attached to the root nodes
			else
			{
				attachToTransform = &attachToTransform->GetRoot();

				CollectAnimationsByNameAndTransform(m_GlobalScale, sourceClip, attachToTransform, true, clip.nodeAnimations, kImportAnimationType_Node);
				CollectAnimationsByNameAndTransform(m_GlobalScale, sourceClip, attachToTransform, true, clip.floatAnimations, kImportAnimationType_Float);
			}

			// Make sure we have a animation component attached
			GameObject* go = &attachToTransform->GetGameObject ();
			AddComponent (*go, "Animation");
			Animation& animation = go->GetComponent (Animation);

			const std::string& attachToTransformName = attachToTransform->GetName();

			// Split animations into several pieces
			if (ShouldSplitAnimations())
			{
				for (ClipAnimations::iterator i=m_ClipAnimations.begin();i != m_ClipAnimations.end();i++)
				{
					AddAnimationClip(
						animation, attachToTransformName, sourceClip, i->name, i->name, i->wrapMode, false,
						true, i->firstFrame, i->lastFrame, sourceClip.GetSampleRate(),  i->loop, scene
					);
				}
			}
			// Don't split animations
			else
			{
				const std::string& clipName = remapTakeName.empty() ? clip.name : remapTakeName;
				const bool setDefaultClip = clip.name == scene.defaultAnimationClipName;
				AddAnimationClip(
					animation, attachToTransformName, sourceClip, clip.name, clipName, m_AnimationWrapMode, setDefaultClip,
					false, 0, 0, sourceClip.GetSampleRate(), false, scene
				);
			}

			DestroySingleObject(&sourceClip);
		}
	}
}

AnimationClip& ModelImporter::CreateAnimClipFromSourceClip( const std::string& clippedIdentifier, AnimationClip& sourceClip, const ClipAnimationInfo& clipInfo, float sampleRate, bool clipClip, const char* animationName, ImportScene& scene)
{
	AnimationClip& clip = ProduceClip(clippedIdentifier, clipInfo.name, scene);

	clip.SetSampleRate(sampleRate);
	clip.SetHideFlags(kNotEditable);
	clip.SetWrapMode(clipInfo.wrapMode);
	clip.SetNameCpp(clipInfo.name);			
	
	clip.AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);

	if (clipClip)
	{
		float singleFrameTime = FrameToTime(1, sourceClip.GetSampleRate());
		if (Abs(clipInfo.firstFrame - clipInfo.lastFrame) < singleFrameTime)
		{
			WarningStringMsg("Model '%s' contains animation clip '%s' which has length of 0 frames (start=%.0f, end=%.0f). "
				"It will result in empty animation. ", animationName, clipInfo.name.c_str(), clipInfo.firstFrame, clipInfo.lastFrame);
		}
		ClipAnimation(sourceClip, clip, FloatFrameToTime(clipInfo.firstFrame, sampleRate), FloatFrameToTime(clipInfo.lastFrame, sampleRate), clipInfo.loop);
	}
	else
		CopyAnimation(sourceClip, clip);

	///////////////////
	//
	// TODO@MECANIM: non muscle clip are keyreduced and compressed after clipping. They are not previewed yet, and the add loop key in the previous call would break the previewing anyway
	//
	//
	clip.SetUseHighQualityCurve(m_AnimationCompression != kAnimationCompressionOptimal);

	if (m_AnimationCompression >= kAnimationCompressionKeyframeReduction && !clip.IsAnimatorMotion())
		ReduceKeyframes(clip, m_AnimationRotationError, m_AnimationPositionError, m_AnimationScaleError, m_AnimationPositionError);

	if (m_AnimationCompression >= kAnimationCompressionKeyframeReductionAndCompression && !clip.IsAnimatorMotion())
		clip.SetCompressionEnabled(true);
	
	return clip;
}

void ModelImporter::RemoveRecycledFileIDForClassID(int classID)
{
	std::map<LocalIdentifierInFileType, UnityStr>::iterator i = m_FileIDToRecycleName.begin();
	for (; i != m_FileIDToRecycleName.end(); ++i)
	{
		if ( i->first == (kMaxObjectsPerClassID * classID))
		{
			m_FileIDToRecycleName.erase(i);
			break;
		}
	}
}

ModelImporter::ModelImportOutput::ModelImportOutput ()
{
	hasExtraRoot = false;
}


static float
EnsureCorrectLightmapUVPackMargin(float margin)
{
	// at some point in 3.x beta we had margin specified as [0..1] number
	// later we switched to integers represetning texels in 1024 texture (more intuitive)
	// alas we failed at upping importer version back then and there are some poor souls stuck with it
	// it will result in crash deep inside uv packing code, so fix the stuff
	// also ensure that margin is [1..64], just in case

	if(margin < 1.0f)
        margin *= 1024.0f;

    if(margin < 1.0f)	margin = 1.0f;
    if(margin > 64.0f)	margin = 64.0f;

    return margin;
}
