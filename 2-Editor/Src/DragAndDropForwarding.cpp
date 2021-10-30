#include "UnityPrefix.h"
#include "DragAndDropForwarding.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/Selection.h"
#include "Editor/Src/Undo/Undo.h"
#include "SceneInspector.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Misc/TextMesh.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "GUIDPersistentManager.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Platform/Interface/DragAndDrop.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Camera/RenderLayers/GUIText.h"
#include "Runtime/Graphics/Transform.h"
#include "EditorHelper.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Animation/Animator.h"
#include "Runtime/Animation/AnimatorController.h"
#include "Runtime/Animation/Animation.h"
#include "Editor/Src/AssetPipeline/ModelImporter.h"
//#include "Runtime/Audio/AudioManager.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Audio/AudioSource.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Audio/AudioSource.h"
#include "Runtime/Terrain/TerrainData.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Audio/AudioClip.h"
#include "EditorExtensionImpl.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Editor/Src/Utility/ObjectNames.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Dynamics/MeshCollider.h"
#include "Runtime/Dynamics/PhysicMaterial.h"
#include "ValidateProjectStructure.h"
#include "Editor/Src/PackageUtility.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Scripting/Scripting.h"

#if ENABLE_SPRITES
#include "Editor/Src/Application.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"
#endif

#if ENABLE_2D_PHYSICS
#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Physics2D/Physics2DMaterial.h"
#endif

using namespace std;


void SetStatusHint (const std::string& statusHint, float timeout = 0.5F);


set<Object*> gNewSelection;


int HandleMaterialDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* pos);
int HandleShaderDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* pos);
int HandleLodMeshDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* pos);
int HandleSetTransformFatherDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*);
int HandleMoveComponentDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*);
int HandleMeshInSceneDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* pos);
int HandleMonoScriptOnGODrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* pos);
int HandleAnimationOnGameObject (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* pos);
int HandleAudioClipInSceneDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* pos);
int HandleAudioClipOnGameObject (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* pos);
int HandleDynamicsMaterialDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*);

int HandleTextureDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*);

#if ENABLE_2D_PHYSICS
int HandlePhysicsMaterial2DDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*);
#endif

int HandleFontDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*);
int HandleTerrainInSceneDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info);
int CanHandleConnectGameObjectToPrefabDrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info);
int ConnectGameObjectToPrefabDrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info);

int HandleAnimatorControllerOnAnimator (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*);
int HandleAnimationClipOnAnimator (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*);

int InstantiateGOTemplateDrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info);
int InstantiateDataTemplateDrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info);
int HandleStatusBarCollisionPlacement (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info);
Object* RemapDrag (Object* object, int classID);

DragAndDropForwarding::DragAndDropForwarding (int mode)
	: m_Mode (mode)
{
	// Drop assets in scene view
	if (mode == kDragIntoSceneWindow)
	{
		RegisterForward (-1,                           ClassID (Prefab),       InstantiateDataTemplateDrag, kDropAbove | kHasToBePrefabParent, HandleStatusBarCollisionPlacement);
		RegisterForward (-1,                           ClassID (GameObject),		     InstantiateGOTemplateDrag, kDropAbove | kHasToBePrefabParent, HandleStatusBarCollisionPlacement);

		RegisterForward (ClassID (GameObject),      ClassID (Mesh),            HandleMeshInSceneDragAndDrop, kDropAbove, HandleStatusBarCollisionPlacement);

		RegisterForward (ClassID (GameObject),      ClassID (Prefab),       InstantiateDataTemplateDrag, kDropAbove | kHasToBePrefabParent, HandleStatusBarCollisionPlacement);
		RegisterForward (ClassID (GameObject),      ClassID (GameObject),         InstantiateGOTemplateDrag, kDropAbove | kHasToBePrefabParent, HandleStatusBarCollisionPlacement);
	}	
	
	// Drop stuff on the transform hierarchy
	if (mode == kDragIntoTransformHierarchy)
	{
		RegisterForward (-1,                           ClassID (Prefab),       InstantiateDataTemplateDrag, kDropAbove | kHasToBePrefabParent);
		RegisterForward (-1,                           ClassID (GameObject),		     InstantiateGOTemplateDrag, kDropAbove | kHasToBePrefabParent);

		RegisterForward (ClassID (GameObject),      ClassID (Prefab),          ConnectGameObjectToPrefabDrag, kDropAbove | kHasToBePrefabParent, CanHandleConnectGameObjectToPrefabDrag);
		RegisterForward (ClassID (GameObject),      ClassID (GameObject),      ConnectGameObjectToPrefabDrag, kDropAbove | kHasToBePrefabParent, CanHandleConnectGameObjectToPrefabDrag);

		RegisterForward (ClassID (Transform), ClassID (Prefab),    InstantiateDataTemplateDrag, kDropAbove | kHasToBePrefabParent);
		RegisterForward (ClassID (Transform), ClassID (GameObject),      InstantiateGOTemplateDrag, kDropAbove | kHasToBePrefabParent);

		// Drag and drop for changing parenting in transform hierarchy
		RegisterForward (ClassID (Transform), ClassID (Transform), HandleSetTransformFatherDragAndDrop, kDropAbove);
		RegisterForward (-1,                           ClassID (Transform), HandleSetTransformFatherDragAndDrop, kDropAbove);
		RegisterForward (ClassID (GameObject), ClassID (MonoBehaviour), HandleMoveComponentDragAndDrop, kDropAbove);
	}
	
	// Drop into hierarchy or scene (Creating new game objects)
	if (mode == kDragIntoTransformHierarchy || mode == kDragIntoSceneWindow)
	{
		RegisterForward (-1,                           ClassID (Mesh),            HandleMeshInSceneDragAndDrop, kDropAbove);
		RegisterForward (-1,                           ClassID (Font),               HandleFontDragAndDrop, kDropAbove);
		RegisterForward (-1,                           ClassID (AudioClip),          HandleAudioClipInSceneDragAndDrop, kDropAbove);
		RegisterForward (-1,                           ClassID (TerrainData),        HandleTerrainInSceneDragAndDrop, kDropAbove);
	}	

	// Dropping assets on game objects
	RegisterForward (ClassID (Renderer),           ClassID (Texture),            HandleTextureDragAndDrop, kDropUpon);

	RegisterForward (ClassID (Renderer),           ClassID (Material),           HandleMaterialDragAndDrop, kDropUpon);
	RegisterForward (ClassID (Collider),           ClassID (PhysicMaterial),     HandleDynamicsMaterialDragAndDrop, kDropUpon);
#if ENABLE_2D_PHYSICS
	RegisterForward (ClassID (Collider2D),         ClassID (PhysicsMaterial2D),  HandlePhysicsMaterial2DDragAndDrop, kDropUpon);
#endif

	RegisterForward (ClassID (Animator),           ClassID (AnimationClip),      HandleAnimationClipOnAnimator, kDropUpon);
	RegisterForward (ClassID (Animator),           ClassID (AnimatorController), HandleAnimatorControllerOnAnimator, kDropUpon);
	RegisterForward (ClassID (GameObject),         ClassID (AnimationClip),      HandleAnimationOnGameObject, kDropUpon);
	RegisterForward (ClassID (GameObject),		   ClassID (Mesh),               HandleLodMeshDragAndDrop, kDropUpon);
	RegisterForward (ClassID (GameObject),         ClassID (Font),               HandleFontDragAndDrop, kDropUpon);
	RegisterForward (ClassID (GameObject),         ClassID (AudioClip),          HandleAudioClipOnGameObject, kDropUpon);
	RegisterForward (ClassID (GameObject),         ClassID (MonoScript),         HandleMonoScriptOnGODrag, kDropUpon);
	RegisterForward (ClassID (Material),           ClassID (Shader),             HandleShaderDragAndDrop, kDropUpon);
}

Object* RemapDrag (Object* object, int classID)
{
	AssertIf (classID == -1);
	if (object != NULL)
	{
		if (object->IsDerivedFrom (classID))
			return object;

		GameObject* go = dynamic_pptr_cast<GameObject*> (object);
		if (go && go->CountDerivedComponents (classID))
			return go->QueryComponentT<Object> (classID);
	}
	return NULL;	
}

int DragAndDropForwarding::DispatchDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragInfo* dragInfo, DispatchType dispatchType)
{
	AssertIf (draggedObject == NULL);

	if (dispatchType == kDispatchCanHandle && draggedUponObject && !IsUserModifiable (*draggedUponObject))
		return kRejectedDrop;

	for (vector<DragCallback>::iterator i=m_Callbacks.begin ();i != m_Callbacks.end ();i++)
	{
		AssertIf (i->draggedClassID == -1);

		Object* remappedDraggedUponObject = NULL;
		if (i->draggedUponClassID == -1)
		{
			if (draggedUponObject != NULL)
			{
				// drag upon scene but dragUponObject is not null
				continue;
			}
		}
		else
		{
			remappedDraggedUponObject = RemapDrag (draggedUponObject, i->draggedUponClassID);
			if (remappedDraggedUponObject == NULL)
			{
				// draggedUponClassID can't be matched in the dragged upon object
				continue;
			}
		}

		Object* remappedDraggedObject = RemapDrag (draggedObject, i->draggedClassID);
		if (!remappedDraggedObject)
		{
			// Dragged classID can't be matched in the dragged Object
			continue;
		}

		bool needsDataTemplate = i->flags & kHasToBePrefabParent;
		if (dynamic_pptr_cast<Prefab*> (draggedObject))
		{
			if (dynamic_pptr_cast<Prefab*> (draggedObject)->IsPrefabParent () != needsDataTemplate)
				continue;
		}
		else if (dynamic_pptr_cast<EditorExtension*> (draggedObject))
		{
			if (dynamic_pptr_cast<EditorExtension*> (draggedObject)->IsPrefabParent () != needsDataTemplate)
				continue;
		}
		else
			continue;

		switch (dispatchType)
		{
		case kDispatchForward:
			if (i->canHandleCallback == NULL || i->canHandleCallback (remappedDraggedUponObject, remappedDraggedObject, NULL))
				return i->callback (remappedDraggedUponObject, remappedDraggedObject, dragInfo);
			break;

		case kDispatchCanHandle:
			if (i->canHandleCallback)
			{
				int flags = i->canHandleCallback (remappedDraggedUponObject, remappedDraggedObject, NULL);
				if (flags != kRejectedDrop)
					return flags;
			}
			else
				return i->flags & (kDropUpon | kDropAbove);
			break;
		}
	}

	return kRejectedDrop;	
}

int DragAndDropForwarding::CanHandleDragAndDrop (Object* draggedUponObject, Object* draggedObject)
{
	return DispatchDragAndDrop (draggedUponObject, draggedObject, NULL, kDispatchCanHandle);
}

int DragAndDropForwarding::CanHandleDragAndDrop (Object* draggedUponObject)
{
	vector<PPtr<Object> > objects = GetDragAndDrop().GetPPtrs();

	int anyDragSuccesful = kRejectedDrop; 
	for (int i=0;i<objects.size();i++)
	{
		Object* dragged = objects[i];
		if (dragged)
			anyDragSuccesful |= CanHandleDragAndDrop (draggedUponObject, dragged);
	}
	return anyDragSuccesful;
}

int DragAndDropForwarding::ForwardDragAndDrop (Object* draggedUponObject, DragInfo* dragInfo)
{
	vector<PPtr<Object> > objects = GetDragAndDrop().GetPPtrs();

	// Before actually modifying anything, see if the user is dragging stuff out of a prefab
	// and if so, ask for confirmation.  With the new prefab system, this is probably not
	// necessary anymore.
	if (m_Mode == kDragIntoTransformHierarchy &&
		!CheckAndConfirmDragOutOfPrefab(draggedUponObject, objects))
		return 0;

	// We're good to go, so wipe the selection.
	gNewSelection.clear ();

	// Perform the drag.
	int anyDragSuccesful = kRejectedDrop;
	for (int i=0;i<objects.size();i++)
	{
		Object* dragged = objects[i];
		if (dragged)
			anyDragSuccesful |= DispatchDragAndDrop (draggedUponObject, dragged, dragInfo, kDispatchForward);
	}
	
	// Update selection.
	if (anyDragSuccesful && !gNewSelection.empty ())
		Selection::SetSelection (gNewSelection);
	
	return anyDragSuccesful;
}

int DragAndDropForwarding::ForwardDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragInfo* info)
{
	return DispatchDragAndDrop (draggedUponObject, draggedObject, info, kDispatchForward);
}

void DragAndDropForwarding::RegisterForward (int draggedUponObjectClassID, int draggedClassID, DragAndDropCallback* callback, int options, DragAndDropCallback* canHandleCallback)
{
	DragCallback dragCallback;
	dragCallback.draggedClassID = draggedClassID;
	dragCallback.draggedUponClassID = draggedUponObjectClassID;
	dragCallback.callback = callback;
	dragCallback.canHandleCallback = canHandleCallback;
	dragCallback.flags = options;
	m_Callbacks.push_back (dragCallback);
}

///@TODO: With nested prefabs, this is probably not necessary anymore and could/should be removed
bool DragAndDropForwarding::CheckAndConfirmDragOutOfPrefab (Object* draggedUponObject, vector<PPtr<Object> >& objects)
{
	Transform* draggedUponTransform = static_cast<Transform*> (RemapDrag (draggedUponObject, ClassID (Transform)));

	for (int i = 0; i < objects.size(); i++)
	{
		Transform* draggedTransform = static_cast<Transform*> (RemapDrag (objects[i], ClassID (Transform)));

		// Ignore if not a drag out of a prefab.
		if (!IsPrefabInstanceWithValidParent (draggedTransform))
			continue;

		// Ignore if dragged to object that's already its parent.
		if (draggedUponTransform == draggedTransform->GetParent ())
			continue;

		// If it's a drag out of a prefab instance, ask the user for confirmation.
		bool allowed = IsPrefabTransformParentChangeAllowed (*draggedTransform, draggedUponTransform);
		if (!allowed)
		{
			if (!WarnPrefab(draggedTransform))
				return false;

			// We only ask once.
			break;
		}
	}

	return true;
}

static void AddComponentsUndoable (GameObject& go, ClassIDType* classIDs, int count, const std::string& actionName)
{
	bool needsComponent = false;
	for (int i=0;i<count;i++)
	{
		if (go.CountDerivedComponents (classIDs[i]) == 0)
			needsComponent = true;
	}

	if (!needsComponent)
		return;
	
	for (int i=0;i<count;i++)
	{
		if (go.CountDerivedComponents (classIDs[i]) == 0)
			AddComponentUndoable(go, classIDs[i], NULL, NULL);
	}
}

static void AddComponentUndoable (GameObject& go, ClassIDType classID, const std::string& actionName)
{
	AddComponentsUndoable (go, &classID, 1, actionName);
}


int HandleMaterialDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<Material*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<Renderer*> (draggedUponObject) == NULL);
	
	Material& material = *static_cast<Material*> (draggedObject);
	Renderer& renderer = *static_cast<Renderer*> (draggedUponObject);
	
	RecordUndoDiff( &renderer, Append ("Assign Material ", material.GetName()) );
	
	renderer.SetMaterialCount (max (renderer.GetMaterialCount (), 1));
	renderer.SetMaterial (&material, 0);
	
	return kDropUpon;
}

int HandleShaderDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<Shader*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<Material*> (draggedUponObject) == NULL);
	
	Shader& shader = *static_cast<Shader*> (draggedObject);
	Material& material = *static_cast<Material*> (draggedUponObject);
	
	RecordUndoDiff( &material, Append ("Assign Shader ", shader.GetName()) );
	
	material.SetShader(&shader);
	
	return kDropUpon;
}


int HandleDynamicsMaterialDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<PhysicMaterial*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<Collider*> (draggedUponObject) == NULL);
	
	PhysicMaterial& material = *static_cast<PhysicMaterial*> (draggedObject);
	Collider& collider = *static_cast<Collider*> (draggedUponObject);
	
	RecordUndoDiff( &collider, Append ("Assign Physics Material ", material.GetName()) );
	
	collider.SetMaterial (&material);
	
	return kDropUpon;
}

#if ENABLE_2D_PHYSICS
int HandlePhysicsMaterial2DDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<PhysicsMaterial2D*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<Collider2D*> (draggedUponObject) == NULL);
	
	PhysicsMaterial2D& material = *static_cast<PhysicsMaterial2D*> (draggedObject);
	Collider2D& collider = *static_cast<Collider2D*> (draggedUponObject);
	
	RecordUndoDiff( &collider, Append ("Assign Physics Material 2D ", material.GetName()) );
	
	collider.SetMaterial (&material);
	
	return kDropUpon;
}
#endif

int HandleFontDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* dragInfo)
{
	AssertIf (dynamic_pptr_cast<Font*> (draggedObject) == NULL);
	Font& font = *static_cast<Font*> (draggedObject);

	if (draggedUponObject == NULL)
	{
	 	GameObject* go = &CreateGameObject ("GUI Text", "GUIText", NULL);
	 	go->GetComponent (GUIText).SetText ("Gui Text");
	 	go->GetComponent (GUIText).SetFont (&font);
	 	if (dragInfo)
		 	go->GetComponent (Transform).SetPosition (Vector3f (dragInfo->viewportPos.x, dragInfo->viewportPos.y, 0));
		 else
			 go->GetComponent (Transform).SetPosition (Vector3f(0.5F, 0.5F, 0.0F));
		
		RegisterCreatedObjectUndo(go, Append (go->GetName (), " Drag Instantiate"));
		
 		gNewSelection.insert (go);
	 	return kDropAbove;
	}
	else
	{
		AssertIf (dynamic_pptr_cast<GameObject*> (draggedUponObject) == NULL);
		GameObject& go = *static_cast<GameObject*> (draggedUponObject);
		
		std::string actionName = "Assign Font";
		if (go.QueryComponent (TextMesh))
		{
			AddComponentUndoable (go, ClassID(MeshRenderer), actionName);
			RecordUndoDiff (&go.GetComponent (TextMesh), actionName);
			RecordUndoDiff (&go.GetComponent (MeshRenderer), actionName);
			
			go.GetComponent (TextMesh).SetFont(&font);
			go.GetComponent (MeshRenderer).SetMaterialCount (1);
			go.GetComponent (MeshRenderer).SetMaterial(font.GetMaterial(), 0);
			
			return kDropUpon;
		}
		else
		{
			AddComponentUndoable (go, ClassID(GUIText), actionName);

			RecordUndoDiff (&go.GetComponent (GUIText), actionName);
			GUIText& text = go.GetComponent(GUIText);
			text.SetFont(&font);
			
			return kDropUpon;
		}
	}
}

static SHADERPROP (MainTex);
int HandleTextureDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<Texture*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<Renderer*> (draggedUponObject) == NULL);
	
	Texture& texture = *static_cast<Texture*> (draggedObject);
	if (texture.IsDerivedFrom(Object::StringToClassID ("Cubemap")))
		return kRejectedDrop;
	
	Renderer& renderer = *static_cast<Renderer*> (draggedUponObject);
	if (renderer.IsDerivedFrom(Object::StringToClassID ("SpriteRenderer")))
		return kRejectedDrop;

	// Lookup texture path
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	string path = GetGUIDPersistentManager ().AssetPathNameFromAnySerializedPath (pm.GetPathName (texture.GetInstanceID ()));
	if (path.empty ())
		return kRejectedDrop;

	// Convert to material path
	path = TexturePathToMaterialPath (path);

	// Find material
	PPtr<Material> foundMaterial = FindMaterial (path);
	
	// Make sure it actually has the right texture assigned!
	if (foundMaterial.IsValid())
	{
		if (foundMaterial->GetTexture (kSLPropMainTex) != &texture)
			foundMaterial = NULL;
	}
	
	// Create material
	if (!foundMaterial.IsValid())
	{
		Shader* shader = GetScriptMapper().GetDefaultShader();
		if (shader == NULL)
		{
			ErrorString("Failed to find default shader");
			return kRejectedDrop;
		}

		// Get the right shader
		Material* oldMaterial = NULL;
		if (renderer.GetMaterialCount () > 0)
			oldMaterial = renderer.GetMaterial (0);
		if (oldMaterial && oldMaterial->GetShader () && oldMaterial->GetShader () != Shader::GetDefault ())
			shader = oldMaterial->GetShader ();
		
		foundMaterial = Material::CreateMaterial (*shader, 0);
		
		// Assign the texture
		foundMaterial->SetTexture (kSLPropMainTex, &texture);
		
		foundMaterial = CreateMaterialAsset(foundMaterial, path);
	}

	// Assign material
	if (foundMaterial.IsValid())
	{
		RecordUndoDiff( &renderer, Append ("Assign Material ", foundMaterial->GetName()) );
		renderer.SetMaterialCount (max (1, renderer.GetMaterialCount ()));
		renderer.SetMaterial (foundMaterial, 0);
	}
	else
		return kRejectedDrop;

	return kDropUpon;
}

int HandleLodMeshDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<Mesh*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<GameObject*> (draggedUponObject) == NULL);
	
	Mesh& lodMesh = *static_cast<Mesh*> (draggedObject);
	GameObject& go = *static_cast<GameObject*> (draggedUponObject);

	dynamic_array<ClassIDType> classes;
	classes.push_back(ClassID(MeshFilter));
	classes.push_back(ClassID(MeshRenderer));
	
	if (go.QueryComponent (Collider) == NULL)
		classes.push_back(ClassID(MeshCollider));

	string actionName = "Assign Mesh";
	AddComponentsUndoable(go, classes.begin(), classes.size(), actionName);
	
	RecordUndoDiff(go.QueryComponent (MeshFilter), actionName);
	RecordUndoDiff(go.QueryComponent (MeshCollider), actionName);

	go.GetComponent (MeshFilter).SetSharedMesh (&lodMesh);
	if (go.QueryComponent (MeshCollider))
		go.GetComponent (MeshCollider).SetSharedMesh (&lodMesh);
	
	return kDropUpon;
}

int HandleAnimatorControllerOnAnimator (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<AnimatorController*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<Animator*> (draggedUponObject) == NULL);
	
	AnimatorController& controller = *static_cast<AnimatorController*> (draggedObject);
	Animator& animator = *static_cast<Animator*> (draggedUponObject);
	
	RecordUndoDiff(&animator, "Assign Controller");
	animator.SetRuntimeAnimatorController(&controller);
	
	return kDropUpon;
}

int HandleAnimationClipOnAnimator (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<AnimationClip*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<Animator*> (draggedUponObject) == NULL);
	
	AnimationClip& clip = *static_cast<AnimationClip*> (draggedObject);
	Animator& animator = *static_cast<Animator*> (draggedUponObject);

	RuntimeAnimatorController* runtimeController = animator.GetRuntimeAnimatorController();
	AnimatorController* controller = animator.GetAnimatorController();
	AnimatorOverrideController* overrideController = animator.GetAnimatorOverrideController();

	if(runtimeController == 0)
	{
		void* params[] = { Scripting::ScriptingWrapperFor(&clip), Scripting::ScriptingWrapperFor(draggedUponObject) };
		AnimatorController* controller = ScriptingObjectToObject<AnimatorController>(CallStaticMonoMethod ("AnimatorController", "CreateAnimatorControllerForClip", params));
		if (controller == NULL)
			return kRejectedDrop;

		RecordUndoDiff(&animator, "Assign Controller");
		animator.SetRuntimeAnimatorController(controller);
	}
	else if(controller)
	{
		void* params[] = { Scripting::ScriptingWrapperFor(controller), Scripting::ScriptingWrapperFor(&clip)};
		ScriptingObjectToObject<AnimatorController>(CallStaticMonoMethod ("AnimatorController", "AddAnimationClipToController", params));		
	}
	else if (overrideController)
	{
		return kRejectedDrop;
	}
	
	return kDropUpon;
}


int HandleAnimationOnGameObject (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<AnimationClip*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<GameObject*> (draggedUponObject) == NULL);

	AnimationClip& clip = *static_cast<AnimationClip*> (draggedObject);
	GameObject& animationTarget = *static_cast<GameObject*> (draggedUponObject);
	
	if(clip.GetAnimationType() == AnimationClip::kLegacy)
	{
		AddComponentUndoable(animationTarget, ClassID(Animation), "Add Animation");
	
		Animation& animation = animationTarget.GetComponent (Animation);

		RecordUndoDiff (&animation, "Assig Animation");
	
		animation.SetClip (&clip);
		animation.AddClip (clip);
	}
	else
	{
		AddComponentUndoable(animationTarget, ClassID(Animator), "Add Animator");
		Animator& animator = animationTarget.GetComponent (Animator);
		RecordUndoDiff (&animator, "Assig Animator");

		HandleAnimationClipOnAnimator( &animator, draggedObject, 0);
	}
		
	return kDropUpon;
}

int HandleMonoScriptOnGODrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<MonoScript*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<GameObject*> (draggedUponObject) == NULL);

	MonoScript& script = *static_cast<MonoScript*> (draggedObject);
	GameObject& go = *static_cast<GameObject*> (draggedUponObject);
	
	string error;
	if (AddComponentUndoable (go, ClassID (MonoBehaviour), &script, &error))
		return kDropUpon;
	else
	{
		DisplayDialog ("Can't add script", error, "Ok");
		return kRejectedDrop;
	}
}

int HandleAudioClipInSceneDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info)
{
	AssertIf (dynamic_pptr_cast<AudioClip*>  (draggedObject) == NULL);
	AssertIf (draggedUponObject != NULL);
	
	AudioClip& clip = *static_cast<AudioClip*> (draggedObject);
	
	// Create a game object to drag onto
	GameObject* go = &CreateGameObject(clip.GetName(), "AudioSource", NULL);

	go->GetComponent (AudioSource).SetAudioClip (&clip);
	if (info)
	{
		go->GetComponent (Transform).SetPosition (info->position);
		go->GetComponent (Transform).SetRotation (info->rotation);
	}

	RegisterCreatedObjectUndo(go, Append (draggedObject->GetName (), " Drag Instantiate"));

	gNewSelection.insert (go);
	return kDropAbove;
}

int HandleTerrainInSceneDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info)
{
	AssertIf (dynamic_pptr_cast<TerrainData*>  (draggedObject) == NULL);
	AssertIf (draggedUponObject != NULL);
	
	TerrainData& terrainData = *static_cast<TerrainData*> (draggedObject);

	ScriptingInvocation invocation("UnityEngine", "Terrain", "CreateTerrainGameObject");
	invocation.AddObject(Scripting::ScriptingWrapperFor(&terrainData));
	
	GameObject* go = ScriptingObjectToObject<GameObject>(invocation.Invoke());
	
	RegisterCreatedObjectUndo(go, Append (draggedObject->GetName (), " Drag Instantiate"));
	
	gNewSelection.insert (go);
	return kDropAbove;
}

int HandleAudioClipOnGameObject (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<AudioClip*>  (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<GameObject*> (draggedUponObject) == NULL);
	
	AudioClip& clip = *static_cast<AudioClip*> (draggedObject);
	GameObject& go  = *static_cast<GameObject*> (draggedUponObject);

	std::string actionName = "Assign AudioClip";
	AddComponentUndoable(go, ClassID(AudioSource), actionName);

	RecordUndoDiff (&go.GetComponent (AudioSource), actionName);
	go.GetComponent (AudioSource).SetAudioClip (&clip);
	
	return kDropUpon;
}

int HandleStatusBarCollisionPlacement (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info)
{
	if (IsOptionKeyDown ())
		SetStatusHint ("Release ALT to place directly");
	else
		SetStatusHint ("Hold ALT to place on surface");
	return kDropAbove;
}

int HandleMeshInSceneDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info)
{
	AssertIf (dynamic_pptr_cast<Mesh*> (draggedObject) == NULL);
	Mesh& mesh = *static_cast<Mesh*> (draggedObject);
	
	GameObject& go = CreateGameObject (mesh.GetName (), "Transform", "MeshFilter", "MeshRenderer", NULL);

	Transform &tc = go.GetComponent (Transform);
	tc.SetPosition (Vector3f::zero);	
	tc.SetRotation (Quaternionf::identity ());
	go.GetComponent (MeshFilter).SetSharedMesh (&mesh);
	
	if (info)
	{
		go.GetComponent (Transform).SetPosition (info->position);
		go.GetComponent (Transform).SetRotation (info->rotation);
	}
	
	RegisterCreatedObjectUndo(&go, Append (draggedObject->GetName (), " Drag Instantiate"));
	
	gNewSelection.insert (&go);

	return kDropAbove;
}

int HandleSetTransformFatherDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<Transform*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<Transform*> (draggedUponObject) != draggedUponObject);
		
	Transform& movingObject = *static_cast<Transform*> (draggedObject);
	Transform* newParent = static_cast<Transform*> (draggedUponObject);
	Transform* oldParent = movingObject.GetParent ();

	Assert (!movingObject.IsPrefabParent ());

	// Just ignore if no change in parent for this drag pair.
	if (oldParent == newParent)
		return kRejectedDrop;
		
	string undoName = Append (draggedObject->GetName(), " Parenting");
	if (SetTransformParentUndo (movingObject, newParent, undoName))
	{
		movingObject.MakeEditorValuesLookNice();
		return kDropUpon;
	}
		
	return kRejectedDrop;
}

int HandleMoveComponentDragAndDrop (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo*)
{
	AssertIf (dynamic_pptr_cast<MonoBehaviour*> (draggedObject) == NULL);
	AssertIf (dynamic_pptr_cast<GameObject*> (draggedUponObject) != draggedUponObject);
	
	MonoBehaviour* comp = static_cast<MonoBehaviour*>(draggedObject);
	GameObject* target = static_cast<GameObject*>(draggedUponObject);

	Unity::Component* copy = AddComponentUndoable(*target, comp->GetClassID(), comp->GetScript());
	if (copy == NULL)
		return kRejectedDrop;

	CopySerialized(*comp, *copy);
	DestroyObjectUndoable(comp);
	
	copy->CheckConsistency();
	
	return kDropUpon;
}

int InstantiateGOTemplateDrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info)
{
	AssertIf (dynamic_pptr_cast<GameObject*> (draggedObject) == NULL);
	GameObject& draggedGO = *static_cast<GameObject*> (draggedObject);
	AssertIf (!draggedGO.IsPrefabParent ());
	return InstantiateDataTemplateDrag (draggedUponObject, draggedGO.GetPrefab(), info);
}

int InstantiateDataTemplateDrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info)
{
	AssertIf (dynamic_pptr_cast<Prefab*> (draggedObject) == NULL);
	Prefab& prefabParent = *static_cast<Prefab*> (draggedObject);
	Assert (prefabParent.IsPrefabParent ());
	Transform* newParent = dynamic_pptr_cast<Transform*> (draggedUponObject);

	if (newParent && newParent->IsPrefabParent ())
		return kRejectedDrop;
	
	Prefab* childPrefab = dynamic_pptr_cast<Prefab*> (InstantiatePrefab (&prefabParent));
	if (childPrefab == NULL)
		return kRejectedDrop;
	
	set<Transform*> rootTransforms;
	vector<Object*> rootGameObjects;
	vector<Object*> prefabObjects;
	GetObjectArrayFromPrefabRoot(*childPrefab, prefabObjects);
	for (int i=0;i<prefabObjects.size();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (prefabObjects[i]);
		if (go && go->QueryComponent (Transform) && go->GetComponent (Transform).GetParent () == NULL)
		{
			rootTransforms.insert (go->QueryComponent (Transform));
			rootGameObjects.push_back(go);
		}
		if (go && IsTopLevelGameObject (*go))
			gNewSelection.insert (prefabObjects[i]);
	}

	if (info)
	{
		Vector3f averagePos = Vector3f::zero;
		
		///@TODO: MOVE THIS TO AN APPROPRIATE PLACE
		MonoObject* obj = CallStaticMonoMethod ("Tools", "GetPivotMode");
		if (ExtractMonoObjectData<int> (obj) == 1)
		{
			// Calculate average position
			for (set<Transform*>::iterator i=rootTransforms.begin ();i!=rootTransforms.end ();i++)
				averagePos += (**i).GetPosition ();
			averagePos *= 1.0F / (float)rootTransforms.size ();
		}
		else
			averagePos = GetSelectionCenter (SelectionToDeepHierarchy (rootTransforms));

		// Offset all transforms by replacing the average pos with the position feeded by the editor
		Vector3f offset = -averagePos + info->position;
		for (set<Transform*>::iterator i=rootTransforms.begin ();i!=rootTransforms.end ();i++)
		{
			Transform& transform = **i;
			transform.SetPosition (transform.GetPosition () + offset);
			transform.SetRotation (transform.GetRotation () * info->rotation);
		}
	}

	string undoName = Append (draggedObject->GetName (), " Drag Instantiate");
	for (int i=0;i<rootGameObjects.size();i++)
		RegisterCreatedObjectUndo(rootGameObjects[i], undoName);

	if (newParent)
	{
		for (set<Transform*>::iterator i=rootTransforms.begin ();i != rootTransforms.end ();i++)
		{
			SetTransformParentUndo (**i, newParent, Transform::kLocalPositionStays, undoName);
		}
	}
	
	return kDropAbove;
}

int ConnectGameObjectToPrefabDrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info)
{
	AssertIf (dynamic_pptr_cast<GameObject*> (draggedUponObject) == NULL);
	GameObject& go = *static_cast<GameObject*> (draggedUponObject);
	ConnectToPrefab (go, draggedObject);
	return kDropUpon;
}



int CanHandleConnectGameObjectToPrefabDrag (Object* draggedUponObject, Object* draggedObject, DragAndDropForwarding::DragInfo* info)
{
	if (IsOptionKeyDown () && IsProjectPrefab(draggedObject))
	{
		AssertIf (dynamic_pptr_cast<GameObject*> (draggedUponObject) == NULL);
		GameObject* go = static_cast<GameObject*> (draggedUponObject);
		if (go->QueryComponent (Transform))
			SetStatusHint ("Release alt to insert a new prefab as child of the game object");	
		else
			SetStatusHint ("Release alt to insert a new prefab into the scene");
		
		return kDropUpon;
	}
	else
	{
		SetStatusHint ("Hold alt to connect the game object to the dragged prefab");
		return kRejectedDrop;
	}
}

string DetermineFileNameForNewPrefab(const UnityGUID& targetFolderAsset, GameObject& rootGameObject)
{
	string targetFolderName = GetGUIDPersistentManager ().AssetPathNameFromGUID (targetFolderAsset);

	int iTry = 0;
	while (true)
	{
		string gameObjectName = rootGameObject.GetName();
		if (iTry > 0) 
			gameObjectName.append(Format(" %d",iTry));
		gameObjectName.append(".prefab");
		gameObjectName = MakeFileNameValid(gameObjectName);
		
		string filename = AppendPathName(targetFolderName, gameObjectName);
		if (!IsFileCreated(filename))
			return filename;
		
		iTry++;
	}
}

DragAndDrop::DragVisualMode HandleProjectWindowPrefabDrag (std::vector<PPtr<Object> > dragObjects, Object* dragUpon, const UnityGUID& guidAsset, const Asset* asset, bool perform)
{
	if (dragObjects.empty())
		return DragAndDrop::kDragOperationNone;
	
	// Get the prefab we are dragging onto
	Prefab* dragUponPrefab = dynamic_pptr_cast<Prefab*> (dragUpon);
	if (dynamic_pptr_cast<GameObject*> (dragUpon))
		dragUponPrefab = static_cast<GameObject*> (dragUpon)->GetPrefab();

	// Disallow uploading to generated prefabs for now!
	if (dragUponPrefab != NULL && asset->type == kCopyAsset)
		return DragAndDrop::kDragOperationNone;
	
	// Calculate single root game object from all the objects that we are dragging
	set<GameObject*> draggedGOs;
	bool connectedAfterwards = true;
	for (int i=0;i<dragObjects.size();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (dragObjects[i]);
		if (go != NULL)
		{
			draggedGOs.insert (go);
			if (go->IsPersistent ())
				connectedAfterwards = false;
			
			// Disallow dragging prefab on itself
			if (go->GetPrefab() == PPtr<Prefab> (dragUponPrefab) && dragUponPrefab != NULL)
				return DragAndDrop::kDragOperationNone;
		}
	}
	GameObject* root = CalculateSingleRootGameObject(draggedGOs);
	if (root == NULL)
		return DragAndDrop::kDragOperationNone;

	// Gameobjects got dragged into the assetwindow, so let's autocreate a new prefab.
	bool autoCreatePrefab = false;
	string assetPath = GetGUIDPersistentManager ().AssetPathNameFromGUID (guidAsset);
	if (dragUponPrefab == NULL)
	{
		if (!IsDirectoryCreated (assetPath))
			return DragAndDrop::kDragOperationNone;
		
		autoCreatePrefab = true;
	}
	
	// At this point we allow the drag operation
	if (!perform)
		return DragAndDrop::kDragOperationLink;

	if (autoCreatePrefab)
	{
		string filename = DetermineFileNameForNewPrefab(guidAsset, *root);
		dragUponPrefab = CreateEmptyPrefab (filename);
		
		if (dragUponPrefab == NULL)
			return DragAndDrop::kDragOperationRejected;
	}
	
	ReplacePrefabOptions replacePrefabFlags = kDefaultRelacePrefab;
	if (connectedAfterwards)
		replacePrefabFlags |= kAutoConnectPrefab;

	// If the prefab contains any objects, and the prefab replacement is unsafe ask user first
	bool validUpload = true;
	if (!IsPrefabEmpty(*dragUponPrefab))
	{
		GameObject* rootForValidiation;
		Prefab* prefabForValidation;

		TempSelectionSet draggedGOsObjectList;
		for (set<GameObject*>::iterator i=draggedGOs.begin();i != draggedGOs.end();i++)
			draggedGOsObjectList.insert(*i);
		
		FindValidUploadPrefab (draggedGOsObjectList, &prefabForValidation, &rootForValidiation);
		validUpload = dragUponPrefab == prefabForValidation;
	}
	
	if (!validUpload)
	{
		string dataTemplateName = dragUponPrefab->GetName();
		string alertString = Format("Are you sure you want to replace the contents of the prefab %s with a GameObjects that are not entirely inherited from it.", dataTemplateName.c_str());
		
		bool performReplacement = DisplayDialog ("Possibly unwanted Prefab replacement", alertString, "Replace anyway", "Don't Replace");
		
		if (!performReplacement)
			return DragAndDrop::kDragOperationRejected;
		
		// Use name based prefab replacement, since the prefab is coming from a different source
		replacePrefabFlags |= kReplacePrefabNameBased;
	}
			
	GameObject* generatedPrefab = ReplacePrefab(*root, dragUponPrefab, replacePrefabFlags);
	
	if (autoCreatePrefab)
		SetActiveObject(generatedPrefab);
	
	return DragAndDrop::kDragOperationLink;
}	

static bool HasProjectExternalFiles (const vector<std::string>& paths)
{
	for (int i=0;i<paths.size();i++)
	{
		if (IsAbsoluteFilePath(paths[i]) || !StartsWithPath(paths[i], "Assets"))
		{
			return true;
		}
	}
	return false;
}

DragAndDrop::DragVisualMode HandleProjectWindowFileDrag (const std::string& newParentPath, const vector<string>& paths, bool perform)
{
	bool shouldCopyFiles = HasProjectExternalFiles(paths);
	shouldCopyFiles |= IsOptionKeyDown();

	bool succeeded = true;
	if (shouldCopyFiles)
	{
		if (perform)
		{
			if (ValidateDragAndDropDirectoryStructure (paths))
			{
				for (int i=0;i<paths.size();i++)
				{
					string sourceFile = paths[i];

					// Import package instead of copying
					if (0 == StrICmp (GetPathNameExtension (sourceFile), "unitypackage")) {
						if (!ImportPackageGUI (sourceFile)) {
							ErrorString (Format ("Failed importing package %s", sourceFile.c_str ()));
							succeeded = false;
						}
						continue;
					}
					
					string newFileName = GetGUIDPersistentManager().GenerateUniqueAssetPathName(newParentPath, GetLastPathNameComponent (sourceFile));
					
					// Copy file
					if (!CopyFileOrDirectory (sourceFile, newFileName))
					{
						ErrorString (Format ("Failed copying file %s to %s.", sourceFile.c_str (), newFileName.c_str ()));
						succeeded = false;
					}
				}
				
				AssetInterface::Get().Refresh ();
			}
			else
			{
				succeeded = false;
			}
		}
		
		if (succeeded)
			return DragAndDrop::kDragOperationCopy;
		else
			return DragAndDrop::kDragOperationNone;
	}
	else // else move files
	{
		if (perform)
		{
			// The physical file is moved and the asset database is updated when moving the file
			// but any changes to the assets is still kept in memory and are written to the new location
			// when the file is moved so:
			// It is not necessary to save assets when moving them
			AssetInterface::Get ().MoveAssets (paths, newParentPath);
			succeeded = true;
		}
		else
		{
			for (int i=0;i<paths.size();i++)
			{
				string oldAssetPath = paths[i];
				
				// Get the old asset path with original lower/upper case formatting
				if (IsPathCreated (oldAssetPath))
				{
					// Move asset to new location
					string newAssetPath = AppendPathName (newParentPath, GetLastPathNameComponent (oldAssetPath));	
					UnityGUID draggedGUID = GetGUIDPersistentManager ().CreateAsset (oldAssetPath);

					succeeded &= AssetDatabase::Get().ValidateMoveAsset (draggedGUID, newAssetPath).empty();					
				}
			}
		}

		if (succeeded)
			return DragAndDrop::kDragOperationMove;
		else
			return DragAndDrop::kDragOperationNone;
	}
}
	
DragAndDrop::DragVisualMode ProjectWindowDrag (const UnityGUID& guidAsset, const LibraryRepresentation* representation, bool perform)
{
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guidAsset);
	if (asset == NULL)
		return DragAndDrop::kDragOperationNone;

	// Drag assets (Finder like move operation)
	vector<std::string> paths = GetDragAndDrop().GetPaths();
	if (!paths.empty())
	{
		// Get guid + pathname of the folder we are dragging upon
		UnityGUID newParentGUID = guidAsset;
		string newParentPath = GetGUIDPersistentManager ().AssetPathNameFromGUID (newParentGUID);
		if (IsDirectoryCreated (newParentPath))
			return HandleProjectWindowFileDrag(newParentPath, paths, perform);
	}
			
	PPtr<Object> dragUpon = asset->mainRepresentation.object;
	if (representation)
		dragUpon = representation->object;
			
	// Drag and drop of prefabs
	DragAndDrop::DragVisualMode dragResult = HandleProjectWindowPrefabDrag (GetDragAndDrop().GetPPtrs(), dragUpon, guidAsset, asset, perform);
	if (dragResult != DragAndDrop::kDragOperationNone)
		return dragResult;
			
	// Arbitrary drag and drop operations on objects
	if (dragUpon.IsValid() && !GetDragAndDrop().GetPPtrs().empty() && asset->type == kSerializedAsset)
	{
		DragAndDropForwarding dragForward (DragAndDropForwarding::kDragIntoProjectWindow);
		if (perform)
		{
			if (dragForward.ForwardDragAndDrop (dragUpon))
				return DragAndDrop::kDragOperationLink;
		}
		else
		{
			if (dragForward.CanHandleDragAndDrop (dragUpon))
				return DragAndDrop::kDragOperationLink;
		}
	}
	
	return DragAndDrop::kDragOperationNone;
}
