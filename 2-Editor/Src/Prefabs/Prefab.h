#ifndef PREFAB_H
#define PREFAB_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "PrefabMerging.h"
#include "Runtime/BaseClasses/GameObject.h" ///@TODO: Get rid of this dependency here!
#include "PrefabUtility.h" ///@TODO: Get rid of this dependency here!

class AwakeFromLoadQueue;

class Prefab : public Object
{
	public:
	
	REGISTER_DERIVED_CLASS(Prefab, Object)
	DECLARE_OBJECT_SERIALIZE(Prefab)

	Prefab (MemLabelId label, ObjectCreationMode mode);

	PrefabModification m_Modification;
	PPtr<Prefab>       m_ParentPrefab;
	PPtr<GameObject>   m_RootGameObject;
	
	bool               m_IsPrefabParent;
	bool               m_IsExploded;

	typedef std::set<PPtr<EditorExtension> > DeprecatedObjectContainer;
	DeprecatedObjectContainer    m_DeprecatedObjectsSet;
	
	const PropertyModifications& GetPropertyModifications () const { return m_Modification.m_Modifications; }
	void GetPropertyModificationsForObject(PPtr<Object> target, PropertyModifications& modifications) const;

	PPtr<Prefab> GetParentPrefab () const { return m_ParentPrefab; }
	PPtr<GameObject> GetRootGameObject() { return m_RootGameObject; }
	virtual bool IsPrefabParent () const { return m_IsPrefabParent; }

	
	void PatchPrefabBackwardsCompatibility ();
	
	virtual void AwakeFromLoad (AwakeFromLoadMode mode);
	
	static void InitializeClass ();
	static void CleanupClass () {}
	
	// UnityGUID       m_LastMergeIdentifier;
};

enum PrefabType
{
	kNoPrefabType = 0,
	kPrefabType = 1,
	kModelPrefabType = 2,
	kPrefabInstanceType = 3,
	kModelPrefabInstanceType = 4,
	kMissingPrefabInstanceType = 5,
	kDisconnectedPrefabInstanceType = 6,
	kDisconnectedModelPrefabInstanceType = 7
};
PrefabType GetPrefabType (Object& obj);

/// Returns the topmost game object that has the same prefab parent as /target/
GameObject& FindRootGameObjectWithSameParentPrefab (GameObject& gameObject);

GameObject* ReplacePrefab (GameObject& go, Object* targetPrefab, ReplacePrefabOptions options);

void SetPropertyModifications (Prefab& prefab, PropertyModifications& modifications);

Prefab* CreateEmptyPrefab (const std::string& path);
GameObject* CreatePrefab (const std::string& path, GameObject& gameObjectInstance, ReplacePrefabOptions options);

bool IsProjectPrefab (Object* object);

Object* InstantiatePrefab (Object* prefab);
bool ReconnectToLastPrefab (GameObject& go);
void ConnectToPrefab (GameObject& sourceObject, Object* prefabObject);

void MergeAllPrefabInstances (PPtr<Prefab> parentPrefab);
void MergeAllPrefabInstances (AwakeFromLoadQueue* awakeFromLoadQueue);
void MergeAllPrefabInstances (PPtr<Prefab> prefab, AwakeFromLoadQueue& awakeFromLoadQueue);
void MergePrefabInstance (Prefab& prefab, AwakeFromLoadQueue& awakeFromLoadQueue);
void MergePrefabInstance (Prefab& prefab);

void MergePrefab (Prefab& parentPrefab, Prefab& childPrefab, AwakeFromLoadQueue& awakeFromLoadQueue, std::vector<ObjectData>& output);

void DisconnectAllPrefabInstances ();

bool RevertPrefabInstance (Object& targetObject);
bool SmartResetToInstantiatedPrefabState (EditorExtension& original);

/// Returns the GameObject/Component parent of /source/, or null if it can't be found.
/// This also returns the prefab parent if the prefab has become disconnected, which can then be used to reconnect the prefab.
Object* GetPrefabParentObject (Object* source);

bool IsPrefabInstanceOrDisconnectedInstance (Object* p);


////WRite comment. this is non-intuitiive behaviour but kinda makes sense
bool IsPrefabInstanceWithValidParent (Object* p);

Prefab* GetPrefabFromAnyObjectInPrefab (Object* sourceObject);

bool IsCurrentlyMergingTemplateChanges ();

void DisconnectPrefabInstance (Object* prefabObject);

GameObject* CalculateSingleRootGameObject (const std::set<GameObject*>& gos);

void GetObjectArrayFromPrefabRoot (Prefab& prefab, std::vector<Object*>& instanceObjects);

void AddParentWithSamePrefab (TempSelectionSet& selection);
bool FindValidUploadPrefab (const TempSelectionSet& selection, Prefab** outPrefab, GameObject** outRootGameObject);
GameObject* FindValidUploadPrefabRoot (GameObject& gameObject);


void RecordPrefabInstancePropertyModifications (Object& object);
void RecordPrefabInstancePropertyModificationsAndValidate (Object& object);
bool IsPrefabTransformParentChangeAllowed (Transform& transform, Transform* newParent);
bool IsPrefabGameObjectDeleteAllowed (GameObject& go);

bool IsComponentAddedToPrefabInstance (Object* obj);

/// Is the prefab exploded (All components / properties are shown in the inspector. As opposed to only showing the root game object and all exposed properties in a list)
void SetIsPrefabExploded (Prefab& prefab, bool exploded);

bool IsPrefabEmpty (Prefab& prefab);


void SetReplacePrefabHideFlags (Object& object);

/// Helper function to find the prefab root of an object (used for picking niceness)
/// Returns the root prefab if there is one or the passed game object if there isn't
GameObject* FindPrefabRoot (GameObject* gameObject);

/// Retrieves the parent prefab, also returns the parent prefab if the connection to the prefab has been broken.
Prefab* GetParentPrefabOrDisconnectedPrefab (Object& target);

/// Internal functions

/// On return prefabChildren conains a list of all prefabs that inherit from prefab.
void CalculateAllLoadedPrefabChildren (PPtr<Prefab> prefab, std::vector<Prefab*>& prefabChildren);

void MakePrefabObjectPersistent (Prefab& prefab, Object& object);

void PrefabDestroyObjectCallback (Object& targetObject);
void PrefabSetTransformParentCallback (Transform* obj, Transform* oldParent, Transform* newParent);

// Function is called whenever a component is added to a game object.
// Makes the component persistent and adds it to the prefab.
void AddComponentToPrefabParentObjectNotification (Unity::Component& component);

bool RemovePropertyModification (PropertyModifications& modifications, Object* target, const std::string& propertyPath);
bool HasPrefabOverride (const PropertyModifications& modifications, Object* target, const std::string& propertyPath);
bool HasPrefabOverride (Object* targetObject, const std::string& propertyPath);

#endif