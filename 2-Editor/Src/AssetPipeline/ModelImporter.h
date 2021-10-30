#ifndef MESHIMPORTER_H
#define MESHIMPORTER_H

#include "AssetImporter.h"
#include <vector>
#include "Runtime/Math/Color.h"
#include "ImportMesh.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Animation/Avatar.h"
#include "Runtime/Animation/AvatarBuilder.h"
#include "ClipAnimationInfo.h"

class Transform;
class Prefab;
class AnimationClip;
namespace Unity { class GameObject; class Material; }
class Mesh;
class Renderer;
class Animation;
class Matrix4x4f;
class Texture;
class BaseAnimationTrack;
class SerializedProperty;
struct ImportScene;

// Increasing the version number causes an automatic reimport when the last imported version is a different.
// Only increase this number if an older version of the importer generated bad data or the importer applies data optimizations that can only be done in the importer.
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
enum 
{
	kModelImporterVersion = 35
};

struct TakeInfo
{
	UnityStr            name;
	UnityStr            defaultClipName;
	float	            startTime;
	float	            stopTime;
	float				bakeStartTime;
	float				bakeStopTime;
	float	            sampleRate;
	PPtr<AnimationClip> clip;

	DEFINE_GET_TYPESTRING (TakeInfo)

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer)
	{
		transfer.Transfer (name, "name");
		transfer.Transfer (defaultClipName, "defaultClipName");
		TRANSFER(startTime);
		TRANSFER(stopTime);
		TRANSFER(bakeStartTime);
		TRANSFER(bakeStopTime);
		TRANSFER(sampleRate);
		TRANSFER(clip);
	}
};




class ModelImporter : public AssetImporter
{
public:
	enum { 
		kImportQueueModels = 99, 
		kImportQueueAnimations = 100,
		kImportQueue_Last = 101 
	};

public:
	
	REGISTER_DERIVED_ABSTRACT_CLASS (ModelImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (ModelImporter)

	ModelImporter (MemLabelId label, ObjectCreationMode mode);
	// ~ModelImporter (); declared-by-macro

	virtual void Reset ();

	static void InitializeClass ();
	static void CleanupClass () {}
	
	virtual void GenerateAssetData ();
	virtual void ClearPreviousImporterOutputs ();
		
	typedef std::vector<ClipAnimationInfo> ClipAnimations;

	// OBSOLETE
	enum { kDontGenerateMaterials = 0, kGenerateMaterialPerTexture = 1, kGenerateMaterialPerImportMaterial = 2 };

	enum MaterialName
	{ 
		kMaterialNameBasedOnTextureName = 0, 
		kMaterialNameBasedOnMaterialName = 1, 
		kMaterialNameBasedOnModelAndMaterialName = 2,
		// OBSOLETE
		// option which behaves exactly like kGenerateMaterialPerTexture option (i.e. backwards compatible with 3.4 and earlier versions)
		kMaterialNameBasedOnTextureName_Before35 = 3
	};
	enum MaterialSearch { kMaterialSearchLocal = 0, kMaterialSearchRecursiveUp = 1, kMaterialSearchEverywhere = 2 };

	enum
	{
		kAnimationCompressionOff = 0,
		kAnimationCompressionKeyframeReduction = 1,
		kAnimationCompressionKeyframeReductionAndCompression = 2,
		kAnimationCompressionOptimal = 3
	};

	enum LegacyGenerateAnimations
	{
		kDontGenerateAnimations = 0,
		kGenerateAnimationsInOriginalRoots = 1,
		kGenerateAnimationsInNodes = 2,
		kGenerateAnimationsInRoot = 3,
		kGenerateAnimations = 4
	};

	// The type of animation to support / import
	enum AnimationType
	{
		kNoAnimationType = 0,
		kLegacy          = 1,
		kGeneric         = 2,
		kHumanoid        = 3
	};
	

	// These two are obsolete, use ImportMaterials, MaterialName and MaterialSearch
	int GetGenerateMaterials(bool showErrors = true) const; 
	void SetGenerateMaterials(int value); 


	GET_SET_COMPARE_DIRTY (bool, ImportMaterials, m_ImportMaterials);
	GET_SET_COMPARE_DIRTY (MaterialName, MaterialName, m_MaterialName);
	GET_SET_COMPARE_DIRTY (MaterialSearch, MaterialSearch, m_MaterialSearch);
	GET_SET_COMPARE_DIRTY (LegacyGenerateAnimations, LegacyGenerateAnimations, m_LegacyGenerateAnimations);
	GET_SET_COMPARE_DIRTY (float, GlobalScale, m_GlobalScale);
	GET_SET_COMPARE_DIRTY (bool, UseFileUnits, m_UseFileUnits);
	GET_SET_COMPARE_DIRTY (bool, BakeIK, m_BakeSimulation);
	GET_SET_COMPARE_DIRTY (bool, ImportBlendShapes, m_ImportBlendShapes);
	GET_SET_COMPARE_DIRTY (bool, AddColliders, m_AddColliders);
	GET_SET_COMPARE_DIRTY (int, AnimationWrapMode, m_AnimationWrapMode);
	GET_SET_COMPARE_DIRTY (TangentSpaceOptions, NormalImportMode, m_MeshSettings.normalImportMode);
	GET_SET_COMPARE_DIRTY (TangentSpaceOptions, TangentImportMode, m_MeshSettings.tangentImportMode);
	GET_SET_COMPARE_DIRTY (int, AnimationCompression, m_AnimationCompression);
	GET_SET_COMPARE_DIRTY (float, AnimationRotationError, m_AnimationRotationError);
	GET_SET_COMPARE_DIRTY (float, AnimationPositionError, m_AnimationPositionError);
	GET_SET_COMPARE_DIRTY (float, AnimationScaleError, m_AnimationScaleError);
	GET_SET_COMPARE_DIRTY (int, MeshCompression, m_MeshCompression);
	GET_SET_COMPARE_DIRTY (bool, IsReadable, m_IsReadable);
	GET_SET_COMPARE_DIRTY (bool, OptimizeMesh, m_MeshSettings.optimizeMesh);
	GET_SET_COMPARE_DIRTY (bool, WeldVertices, m_MeshSettings.weldVertices);
	GET_SET_COMPARE_DIRTY (bool, SplitTangentsAcrossUV, m_MeshSettings.splitTangentsAcrossUV);
	GET_SET_COMPARE_DIRTY (bool, SwapUVChannels, m_MeshSettings.swapUVChannels);
	GET_SET_COMPARE_DIRTY (bool, GenerateSecondaryUV, m_MeshSettings.generateSecondaryUV);
	GET_SET_COMPARE_DIRTY (float, SecondaryUVAngleDistortion, m_MeshSettings.secondaryUVAngleDistortion);
	GET_SET_COMPARE_DIRTY (float, SecondaryUVAreaDistortion, m_MeshSettings.secondaryUVAreaDistortion);
	GET_SET_COMPARE_DIRTY (float, SecondaryUVHardAngle, m_MeshSettings.secondaryUVHardAngle);
	GET_SET_COMPARE_DIRTY (float, SecondaryUVPackMargin, m_MeshSettings.secondaryUVPackMargin);
	GET_SET_COMPARE_DIRTY (float, NormalSmoothingAngle, m_MeshSettings.normalSmoothAngle);	
	GET_SET_COMPARE_DIRTY (AnimationType, AnimationType, m_AnimationType);
	GET_SET_COMPARE_DIRTY (ClipAnimations&, ClipAnimations, m_ClipAnimations);
	GET_SET_COMPARE_DIRTY (bool, ImportAnimation, m_ImportAnimation);

	virtual bool IsUseFileUnitsSupported() const { return false; }
	virtual bool IsTangentImportSupported() const = 0;
	
	
	bool ShouldImportAnimations ()const
	{
		if (m_AnimationType == kNoAnimationType)
			return false;
		else if (m_AnimationType == kLegacy)
			return m_ImportAnimation  && m_LegacyGenerateAnimations != kDontGenerateAnimations;
		else
			return m_ImportAnimation;
	}
	
	
	// Deprecated behaviour for where we add animation components:
	
	// * kDontGenerateAnimations -> Animation component on the root
	// * kGenerateAnimationsInOriginalRoots -> Animation component on every node
	// * kGenerateAnimationsInNodes -> Animation component on every node
	// * kGenerateAnimationsInRoot -> Animation component on root node
	// * kGenerateAnimations -> Animation component on root node
	bool ShouldGenerateAnimationComponentOnEveryNode () const
	{
		if (m_AnimationType == kLegacy)
			return m_LegacyGenerateAnimations == kGenerateAnimationsInOriginalRoots || m_LegacyGenerateAnimations == kGenerateAnimationsInNodes;
		else
			return false;
	}
	
	bool ShouldSplitAnimations () const
	{
		return !m_ClipAnimations.empty();
	}
	
	bool ShouldImportSkinnedMesh () const
	{
		if (m_AnimationType == kLegacy)
			return m_LegacyGenerateAnimations != kDontGenerateAnimations;
		
		return m_AnimationType != kNoAnimationType;
	}
	
	bool ShouldUseDeprecatedMoveSkinnedMeshRendererToRootBone () const
	{
		if (m_AnimationType == kLegacy)
			return m_LegacyGenerateAnimations != kGenerateAnimations;
		else
			return false;
	}
	
	bool ShouldImportAnimationDeprecatedWithMultipleRoots () const
	{
		if (m_AnimationType == kLegacy)
			return m_LegacyGenerateAnimations == kGenerateAnimationsInOriginalRoots || m_LegacyGenerateAnimations == kGenerateAnimationsInNodes;
		else
			return false;
	}

	bool ShouldUseCorrectClipTimeValues () const
	{
		// Legacy animatins using time values that do not match the frame numbers in Maya / Max.
		// We keep the behaviour in order to not break project folders.
		if (m_AnimationType == kLegacy)
			return m_LegacyGenerateAnimations == kGenerateAnimations;
		else
			return true;
	}
	
	bool ShouldAdjustClipsByDeprecatedTimeRange()
	{
		if (m_AnimationType == kLegacy && m_ImportAnimation)
		{
			return m_LegacyGenerateAnimations == kGenerateAnimations ? false : !ShouldSplitAnimations();
		}
		else
			return false;
	}
	
	
	bool ShouldImportLegacyInOriginalRoots ()
	{
		if (m_AnimationType == kLegacy && m_ImportAnimation)
			return m_LegacyGenerateAnimations == kGenerateAnimationsInOriginalRoots;
		else
			return false;
	}

	bool ShouldImportAnimationInRoot () const
	{
		if (!m_ImportAnimation)
			return false;
		
		if (m_AnimationType == kLegacy)
			return m_LegacyGenerateAnimations == kGenerateAnimations || m_LegacyGenerateAnimations == kGenerateAnimationsInRoot;
		else
			return true;
	}
	
	
	
	std::pair<int, int> GetSplitAnimationRange ();
	
	ImportMeshSettings& GetMeshSettings () { return m_MeshSettings; }

	virtual bool IsBakeIKSupported () { return false; }

	static void UpdateSkeletonPose(SkeletonBoneList & skeletonBones, SerializedProperty& serializedProperty);

	static void UpdateTransformMask(AvatarMask & mask, SerializedProperty& serializedProperty);


	AnimationClip* GetPreviewAnimationClipForTake (const std::string& takeName);
	
	// Returns queue number for model and animation import 	
	static int GetQueueNumber(const std::string& assetPathName);

	const vector<TakeInfo>& GetImportedTakeInfos() { return m_Output.importedTakeInfos; }
	const vector<UnityGUID>& GetReferencedClips() { return m_Output.referencedClips; }

	std::vector<UnityStr> GetTransformPaths();

	std::string CalculateBestFittingPreviewGameObject();
	
protected:

	virtual bool NeedToRetainFileIDToRecycleNameMapping();

	/// Convert the file into ImportNodes, ImportMaterials and ImportMeshes here!
	/// Return false if the import failed (Will fallback to the last imported version)
	virtual bool DoMeshImport (ImportScene& outputScene) = 0;
	
	typedef std::vector<PPtr<GameObject> > Roots;
	
	struct ModelImportData;
	
	void GenerateAll (ModelImportData& importData);

	Prefab& GeneratePrefab (GameObject& root, const std::string& prefabName);

	GameObject& InstantiateImportNode (const ImportNode& node, Transform* parent, ModelImportData& importData);
	PPtr<Material> InstantiateImportMaterial (int index, Renderer& renderer, ModelImportData& importData);
	void InstantiateImportMesh (int index, const Matrix4x4f& transform, std::vector<Mesh*>& lodMeshes, std::vector<std::vector<int> >& lodMeshesMaterials, ModelImportData& importData);
	void AssignImportMaterials (const ImportNode& node);
	bool ImportSkinnedMesh (const ImportNode& node, ImportScene& importData, const Transform& skeletonRoot);
	void ImportAvatar (GameObject& rootGameObject, bool didImportSkinnedMesh);

	void ImportMuscleClip(GameObject& rootGameObject, AnimationClip** clips, size_t size);
	//void GenerateAvatarCollider(Avatar& avatar, NamedTransform const& namedTransform);
	void GenerateAnimationClips (GameObject& rootGameObject, const std::string& remapTakeName, ModelImportData& importData);
	AnimationClip& ProduceClip (const std::string& internalProduceName, const std::string& animationName, ImportScene& scene);

	void ConnectExternalAnimationClips (GameObject& root);
	void GenerateReferencedClipDependencyList ();

	void SplitAnimationClips (GameObject& root, const ClipAnimations& clipAnimations, const std::vector<std::string>& internalNames, bool clipClip, ModelImportData& importData);

	void InitDefaultValues ();

	float GetDefaultScale();

	

	///
	/// support for old generate animation mode
	///
	void DeprecatedGenerateAndAssignClipsWithMultipleRoots (string remapTakeName, ImportScene& scene);
	void AddAnimationClip(Animation& animation, const std::string& attachToTransformName, AnimationClip& sourceClip, 
		const std::string& animationName, const std::string& clipName, int wrapMode, bool setDefaultClip,
		bool clipClip, float firstFrame, float lastFrame, float sampleRate, bool loop, ImportScene& scene);

	void ValidateAndClearTextureFileName(ImportTexture& texture, const std::string& assetPathName, const Renderer& renderer, const std::string& materialName);
	AnimationClip& CreateAnimClipFromSourceClip(const std::string& clippedIdentifier, AnimationClip& sourceClip, const ClipAnimationInfo& clipInfo, float sampleRate, bool clipClip, const char* animationName, ImportScene& scene);

protected:
	bool m_ImportBlendShapes;
	
	bool                m_ImportMaterials;
	MaterialName        m_MaterialName;
	MaterialSearch      m_MaterialSearch;
	ImportMeshSettings  m_MeshSettings;
	float               m_GlobalScale;
	int                 m_MeshCompression;	
	bool                m_AddColliders;
	bool				m_UseFileUnits;
	bool                m_IsReadable;

	std::vector<float>  m_LODScreenPercentages;
	
	// Animation Import
	
	// if true, AnimationClips are imported, otherwise not.
	bool                       m_ImportAnimation;

	AnimationType              m_AnimationType; ///< enum { None = 0, Legacy = 1, Generic = 2, Humanoid = 3 } The type of animation to support / import. 
	
	// Stores the setting of whether the avatar should be copied from another avatar
	bool                       m_CopyAvatar;

	LegacyGenerateAnimations   m_LegacyGenerateAnimations;
	
	
	ClipAnimations             m_ClipAnimations;
	bool                       m_BakeSimulation;
	int                        m_AnimationCompression;

	float                      m_AnimationRotationError;
	float                      m_AnimationPositionError;
	float                      m_AnimationScaleError;
	int                        m_AnimationWrapMode;
	
	bool                       m_OptimizeGameObjects;
	UNITY_VECTOR(kMemAssetImporter, UnityStr) m_ExtraExposedTransformPaths;

	// When a human description is copied from another avatar the reference to the source avatar is stored here
	PPtr<Avatar>		       m_LastHumanDescriptionAvatarSource;
	
	HumanDescription           m_HumanDescription;
	
	
	///////  ---- Model importer output & state
	
	struct ModelImportOutput
	{
		bool                   hasExtraRoot;
		std::vector<TakeInfo>  importedTakeInfos;
		std::vector<UnityGUID> referencedClips;
		Roots                  animationRoots;		
		ModelImportOutput ();
	};

	struct InstantiatedMesh
	{
		std::vector<Mesh*> lodMeshes;
		Matrix4x4f transform;
		std::vector<std::vector<int> > lodMeshesMaterials;
	};
	
	typedef std::list<InstantiatedMesh>  InstantiatedMeshes;
	struct ModelImportData
	{
		ImportScene                          scene;
		std::vector<InstantiatedMeshes>      instantiatedMeshes;
		std::vector<PPtr<Material> >         instantiatedMaterials;
		PPtr<GameObject>                     importedRoot;	
	};

	// Data generated during scene importing.
	ModelImportOutput     m_Output;
	
private:
private:
	// Due to the way importing packages works, this variable can get lost, it happens in this rare case
	//  -Read model importer settings and set this true
	//  -Write model importer settings, this would update the version of the meta data
	//  -Destroy modelimporter, if editor comes in a low memory situation 
	//  -read model importer settings, now because the meta data version has been updated, this flag would not be set true again
	//  -import model, no patching from prefab would be done and references would be lost
	// The better solution for this problem is to store the .meta files in the package then the name to file id mapping will always be correct
	// See PackageUtility.cpp (ImportPackageStep2) and ASController::CreateAsset
	bool	m_NeedToPatchRecycleIDsFromPrefab; // Not serialized
	typedef std::map<int, std::list<LocalIdentifierInFileType> > FileIDsByClassID;
	typedef std::map<UnityStr,FileIDsByClassID>  NamesToFileIDsByClassID;

	void TransferReadMetaNamesToFileIDs (YAMLMapping* settings, NamesToFileIDsByClassID& textMetaNamesToFileIDs);
	void TransferReadMetaNamesToFileIDsUnity34OrOlder (YAMLMapping* settings, NamesToFileIDsByClassID& textMetaNamesToFileIDs);
	void PatchFileIDsForModo();
	void PatchGenerateAnimationFileIDs();
	void UpgradeTextMetaNameToRecycledNames(const NamesToFileIDsByClassID& textMetaNamesToFileIDs);
	void PatchRecycleIDsFromPrefab();
	
	Material* CreateNewMaterial (const ImportMaterial& importMaterial);	
	PPtr<Material> CreateSharedMaterial(const ImportMaterial& importMaterial);
	Texture* FindTexture (const std::vector<std::string>& suggestions) const;

	template <class TAsset>
	TAsset* FindAssetAnywhere(const std::vector<std::string>& suggestions) const;

	void RemoveRecycledFileIDForClassID(int classID);
	
	template<class T>
	void MaterialBackwardsCompatibility (T& transfer);
	
	template<class T>
	void ReduceKeyframesBackwardsCompatibility (T& transfer);
	
	template<class T>
	void TextNameBackwardsCompatibility (T& transfer);	

	template <class T>
	void MecanimAnimationBackwardsCompatibility(T& transfer);

	template<class T>
	void RecycleIDBackwardsCompatibility (T& transfer);
};

/// The material path is without extension [CONFUSING CHANGE IT!]
string TexturePathToMaterialPath (const string& path);

Material* FindMaterial (const string& path);
void CreateExternalAssetFolder (const string& pathName);
Material* CreateMaterialAsset (Material* material, const std::string& pathName);

void CollectNecessaryReferencedAnimationClipDependencies (const std::string& assetPath, vector<UnityGUID>& dependencies);

#endif
