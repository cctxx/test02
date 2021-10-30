#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "ComponentRequirement.h"
#include "GameObjectUtility.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "BatchDeleteObjects.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "BuildSettings.h"
#include "Runtime/Camera/Camera.h"	  //// @TODO: Only used by FindMainCamera, should be moved to a different file?
#include "Runtime/BaseClasses/Tags.h"
#include "Player.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"

#if UNITY_EDITOR
#include "Editor/Src/Prefabs/Prefab.h"
#endif

PROFILER_INFORMATION(gDestroyProfile, "Destroy", kProfilerOther)

using namespace std;
const char* kUnityEngine = "UnityEngine";

#if UNITY_EDITOR

static std::vector<AddComponentCallbackFunction*> gGOAddComponentCallbacks;

void RegisterAddComponentCallback (AddComponentCallbackFunction* callback)
{
	gGOAddComponentCallbacks.push_back(callback);
}

static void InvokeAddComponentCallback (Unity::Component& com)
{
	for (int i=0;i<gGOAddComponentCallbacks.size();i++)
		gGOAddComponentCallbacks[i] (com);
}
#else
inline void InvokeAddComponentCallback (Unity::Component& com) { }
#endif



bool IsComponentSubclassOfMonoClass (Unity::Component& com, MonoClass* requiredClass);
bool IsComponentSubclassOfMonoClass (Unity::Component& com, MonoClass* requiredClass)
{
#if ENABLE_SCRIPTING
	int curComponentClassID = com.GetClassID();
	
	ScriptingClassPtr curKlass = GetMonoManager().ClassIDToScriptingClass (curComponentClassID);

	MonoBehaviour* monoBehaviour = dynamic_pptr_cast<MonoBehaviour*> (&com);
	if (monoBehaviour)
		curKlass = monoBehaviour->GetClass();

	// Check classID against behaviour
	if (curKlass && requiredClass)
	{
		if (requiredClass == curKlass || scripting_class_is_subclass_of (curKlass, requiredClass))
			return true;
	}
#endif
	return false;
}

Unity::Component* AddComponentUnchecked (GameObject& go, int classID, MonoScriptPtr script, string* error)
{
	// Produce object
	Unity::Component* o = static_cast<Unity::Component*>(Object::Produce (classID));
	if (o == NULL)
	{
		if (error)
			*error = Format ("Can't add component because the component '%s' can't be produced.", Object::ClassIDToString (classID).c_str());
		return NULL;
	}
	
	AssertIf (!o->IsDerivedFrom (ClassID (Component)));
	
	o->Reset ();
	go.AddComponentInternal (static_cast<Unity::Component*> (o));
	
	MonoBehaviour* monoBehaviour = dynamic_pptr_cast<MonoBehaviour*> (o);
	if (monoBehaviour)
	{
#if ENABLE_SCRIPTING
		int instanceID = o->GetInstanceID();
		monoBehaviour->SetScript (script);
		// Check if the object has destroyed itself in Awake.
		if (!PPtr<Object> (instanceID).IsValid())
			return NULL;
#else
		AssertIf( script == 0 && "Mono's disabled" );
#endif
		
#if UNITY_EDITOR
		if (!IsInsidePlayerLoop())
			ApplyDefaultReferences(*monoBehaviour, script->GetDefaultReferences());
#endif
	}
	
	o->Reset();
	o->SmartReset();
	o->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
	o->SetDirty ();
	
	InvokeAddComponentCallback (*o);
	
	return static_cast<Unity::Component*> (o);
}

bool CheckForAbstractClass(GameObject& go, int classID, string* error)
{
	if (Object::ClassIDToRTTI(classID)->isAbstract)
	{
		// list all derived components
		string objectList; 
		vector<SInt32> derivedObjects;
		Object::FindAllDerivedClasses (classID, &derivedObjects);
		for (vector<SInt32>::const_iterator it=derivedObjects.begin();it!=derivedObjects.end();++it)
		{
			SInt32 id = (*it);
			objectList += Format("'%s'", Object::ClassIDToString (id).c_str());
			if (it != derivedObjects.end()-1)
					objectList += " or ";				
		}		
		*error = Format ("Adding component failed. Add required component of type %s to the game object '%s' first.", objectList.c_str(), go.GetName ());
		return false;
	}
	return true;
}

static void AddRequiredScriptComponents (GameObject& go, MonoScript* script, std::set<ScriptingClassPtr> &processed)
{
#if ENABLE_MONO || UNITY_WINRT
	ScriptingArrayPtr array = RequiredComponentsOf(script->GetClass());

	if (array)
	{
		for (int j = 0; j < GetScriptingArraySize(array); j++)
		{
			ScriptingObjectPtr requiredClassObject = Scripting::GetScriptingArrayElementNoRef<ScriptingObjectPtr>(array, j);
			if (requiredClassObject == SCRIPTING_NULL)
				continue;

			ScriptingClassPtr requiredClass = GetScriptingTypeRegistry().GetType (requiredClassObject);
	
			if (requiredClass != NULL && processed.find(requiredClass) != processed.end())
				continue;

			// Is that script/component already added to the game obejct?				
			bool needToAdd = true;
			for (int c=0;c<go.GetComponentCount();c++)
			{
				if (IsComponentSubclassOfMonoClass (go.GetComponentAtIndex(c), requiredClass))
				{
					needToAdd = false;
					break;
				}
			}

			if (needToAdd)
			{
				int clsID = -1;
				MonoScript* requiredScript = NULL;
				if (StrICmp (scripting_class_get_namespace (requiredClass), kUnityEngine) == 0)
					clsID = Object::StringToClassID (scripting_class_get_name (requiredClass));
	
				if (clsID == -1 || !Object::IsDerivedFromClassID (clsID, ClassID (Component)))
				{
					// We can't find the script.
					// Not very good but we will just let it slip through
					requiredScript = GetMonoScriptManager().FindRuntimeScript(requiredClass);
					if (requiredScript == NULL )
						continue;
					clsID = ClassID (MonoBehaviour);
				}
		
				string internalError;
				if (!CheckForAbstractClass(go, clsID, &internalError) || AddComponentInternal (go, clsID, requiredScript, processed, &internalError) == NULL)
					ErrorString (internalError);
			}
		}
	}
#endif
}
	

static bool ValidateScriptComponent (MonoScript* script, std::string* error)
{
	// Check if the script is instantiatable
	if (script == NULL)
	{
		if (error)
			*error = Format ("Can't add script behaviour because the script couldn't be found.");
		return false;
	}
		
	MonoScriptType type = (MonoScriptType)script->GetScriptType();
	
	if (type == kScriptTypeMonoBehaviourDerived)
		return true;

	
	// Check if the script is instantiatable
	if (type == kScriptTypeClassNotFound)
	{
		if (error)
		{
#if UNITY_EDITOR
			if (GetMonoManager().HasCompileErrors())
				*error = Format("Can't add script behaviour %s. You need to fix all compile errors in all scripts first!", script->GetName());
			else
#endif					
				*error = Format("Can't add script behaviour %s. The scripts file name does not match the name of the class defined in the script!", script->GetName());
		}
				
		return false;
	}
	// Check if the script is instantiatable
	else
	{
		if (error)
		{
			if (script->IsEditorScript())
				*error = Format ("Can't add script behaviour %s because it is an editor script. To attach a script it needs to be outside the 'Editor' folder.", script->GetName());
			else if (type == kScriptTypeNotInitialized)
				*error = Format("Script %s has not finished compilation yet. Please wait until compilation of the script has finished and try again.", script->GetName());
			else if (type == kScriptTypeClassIsAbstract)
				*error = Format("Can't add script behaviour %s. The script class can't be abstract!", script->GetName());
			else if (type == kScriptTypeClassIsInterface)
				*error = Format("Can't add script behaviour %s. The script can't be an interface!", script->GetName());
			else if (type == kScriptTypeClassIsGeneric)
				*error = Format("Can't add script behaviour %s. Generic MonoBehaviours are not supported!", script->GetName());
			else
				*error = Format("Can't add script behaviour %s. The script needs to derive from MonoBehaviour!", script->GetName());
		}
		return false;
	}
}

bool CanAddComponent (Unity::GameObject& go, int classID)
{
	if (go.CountDerivedComponents (classID) && !DoesComponentAllowMultipleInclusion (classID))
		return false;
	if (go.HasConflictingComponents(classID))
		return false;
	return true;
}

Unity::Component* AddComponent (GameObject& go, int classID, MonoScriptPtr script, string* error)
{
	std::set<ScriptingClassPtr> processed;
	return AddComponentInternal (go, classID, script, processed, error);
}

Unity::Component* AddComponentInternal (GameObject& go, int classID, MonoScriptPtr script, std::set<ScriptingClassPtr> &processed, std::string* error)
{
	// Are we actually derived from go component?
	if (!Object::IsDerivedFromClassID (classID, ClassID (Component)))
	{
		if (error)
			*error = Format ("Can't add component because '%s' is not derived from Component.", Object::ClassIDToString (classID).c_str ());
		return NULL;
	}

	// We can't add a component if it has conflicting components.  This is included when checking if we can add a component but we want to give a specific reason here i.e. a component conflict.
	Unity::Component* conflictingComponent = go.FindConflictingComponentPtr (classID);
	if (conflictingComponent)
	{
		if (error)
			*error = Format ("Can't add component '%s' to %s because it conflicts with the existing '%s' derived component!",
			Object::ClassIDToString (classID).c_str (), go.GetName (), Object::ClassIDToString (conflictingComponent->GetClassID ()).c_str ());
		return NULL;
	}

	// We can't add a component if we are already inserted and the component doesn't allow multiple insertion!
	if (!CanAddComponent (go, classID))
	{
		if (error)
			*error = Format ("Can't add component '%s' to %s because such a component is already added to the game object!", Object::ClassIDToString (classID).c_str (), go.GetName ());
		return NULL;
	}

	// Don't allow adding a component to an imported model
	// @TODO: go.TestHideFlag(Object::kNotEditable) && go.IsPersistent()
	//		-> Is weird. We should cleanup the hide flags to be sensible design
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_5_a1) && go.TestHideFlag(Object::kNotEditable) && go.IsPersistent())
	{
		if (error)
			*error = Format ("Can't add component '%s' to %s because the game object is a generated prefab and can only be modified through an AssetPostprocessor.", Object::ClassIDToString (classID).c_str (), go.GetName ());
		return NULL;
	}

	// Find all required components and add them before!
	const vector_set<int>& requiredCom = FindRequiredComponentsForComponent (classID);
	vector_set<int>::const_iterator i;
	for (i=requiredCom.begin ();i!=requiredCom.end ();i++)
	{
		if (go.CountDerivedComponents (*i))
			continue;

		//TODO: check
		string internalError;
		if (!CheckForAbstractClass(go, *i, &internalError) || AddComponentInternal (go, *i, NULL, processed, &internalError) == NULL)
		{
			ErrorString (internalError);
			return NULL;
		}
	}

	// Find all required components for a script and add them before!
	if (classID == ClassID (MonoBehaviour))
	{
		#if ENABLE_SCRIPTING

		if (!ValidateScriptComponent (script, error))
			return NULL;
		
		Assert(script->GetClass () != SCRIPTING_NULL);
					
		processed.insert(script->GetClass());
		AddRequiredScriptComponents (go, script, processed);
					
		#else
		if( error )
			*error = Format ("Can't add script behaviour because the Mono's disabled.");
		return NULL;
		#endif // ENABLE_SCRIPTING
	}
	
	// this means user is trying to add abstract class directly (AddComponent("Collider"))
	if (Object::ClassIDToRTTI(classID)->isAbstract)
	{
		*error = Format ("Cannot add component of type '%s' because it is abstract. Add component of type that is derived from '%s' instead.", Object::ClassIDToString (classID).c_str (), Object::ClassIDToString (classID).c_str ());
		return NULL;
	}

	return AddComponentUnchecked (go, classID, script, error);
}



Unity::Component* AddComponent (GameObject& go, const char* name, string* error)
{
	if (BeginsWith(name, "UnityEngine."))
		name += strlen("UnityEngine.");	
	
	int classID = Object::StringToClassID (name);
	if (classID != -1 && Object::IsDerivedFromClassID(classID, ClassID(Component)))
	{
		return AddComponent (go, classID, NULL, error);
	}
#if ENABLE_SCRIPTING
	else
	{
		MonoScriptPtr script = GetMonoScriptManager().FindRuntimeScript (name);

		if (script)
			return AddComponent (go, ClassID (MonoBehaviour), script, error);
		
		if (error)
		{
			if (classID == -1)
				*error = Format ("Can't add component because class '%s' doesn't exist!", name);
			else
				*error = Format("Can't add component because '%s' is not derived from Component.", name);
		}

		return NULL;
	}
#else
	return NULL;
#endif
}

// varargs can only be passed around by passing the va_list, so caller is responsible for calling va_start/va_end
void AddComponentsFromVAList (GameObject& go, const char* componentName, va_list componentList)
{
	if (componentName == NULL) 
		return;
	
	string error;
	if (AddComponent (go, componentName, &error) == NULL)
		ErrorString (error);
	
	while (true)
	{
		const char* cur = va_arg (componentList, const char*);
		if (cur == NULL)
			break;
		if (AddComponent (go, cur, &error) == NULL)
			ErrorString (error);
	}
}

void AddComponents (GameObject& go, const char* componentName, ...)
{
	va_list ap;
	va_start (ap, componentName);
	AddComponentsFromVAList (go, componentName, ap);
	va_end (ap);
}

void ActivateGameObject (GameObject& go, const string& name)
{
	go.Reset ();
	go.SetName (name.c_str ());
	go.AwakeFromLoad (kInstantiateOrCreateFromCodeAwakeFromLoad);
	go.Activate ();
}

void SetNameAndResetGameObject (GameObject& go, const string& name)
{
	go.Reset ();
	go.SetName (name.c_str ());
	go.AwakeFromLoad (kInstantiateOrCreateFromCodeAwakeFromLoad);
}

GameObject& CreateGameObject (const string& name, const char* componentName, ...)
{
	// Create game object with name!
	GameObject &go = *NEW_OBJECT (GameObject);

	ActivateGameObject (go, name);

	// Add components with class names!
	va_list ap;
	va_start (ap, componentName);
	AddComponentsFromVAList (go, componentName, ap);
	va_end (ap);

	return go;
}


Unity::GameObject& CreateGameObjectWithVAList (const std::string& name, const char* componentName, va_list list)
{
	va_list componentList;
	va_copy (componentList, list);
	GameObject &go = *NEW_OBJECT (GameObject);

	ActivateGameObject (go, name);
	AddComponentsFromVAList (go, componentName, componentList);
	va_end (componentList);

	return go;
}

GameObject& CreateGameObjectWithHideFlags (const string& name, bool isActive, int flags, const char* componentName, ...)
{
	// Create game object with name!
	GameObject &go = *NEW_OBJECT (GameObject);

	// HideFlags need to be set before object activation because of a bug where 
	// Unity will not immediately update visible root game object list when 
	// changing hide flags after creation Case: 382530
	go.SetHideFlags (flags);

	if (isActive)
		ActivateGameObject (go, name);
	else
		SetNameAndResetGameObject (go, name);
	
	// Add components with class names!
	va_list ap;
	va_start (ap, componentName);
	AddComponentsFromVAList(go, componentName, ap);
	va_end (ap);

	return go;
}

inline string GetComponentOrScriptName (Unity::Component& com)
{
#if ENABLE_SCRIPTING
	MonoBehaviour* monoBehaviour = dynamic_pptr_cast<MonoBehaviour*> (&com);
	if (monoBehaviour)
	{
		MonoScript* script = monoBehaviour->GetScript();
		if (script)
			return Append (script->GetName(), " (Script)");
	}
#endif	
	return com.GetClassName ();
}

bool CanRemoveComponent(Unity::Component& component, std::string* error)
{
	return CanReplaceComponent(component, -1, error);
}

bool CanReplaceComponent(Unity::Component& component, int replacementClassID, std::string* error)
{
	GameObject* go = component.GetGameObjectPtr ();
	if (go == NULL)
		return false;
	
	int componentIndex = go->GetComponentIndex(&component);
	if (componentIndex == -1)
		return false;
	
	// Starting with Unity 3.2, a transform component has to be attached to every game object!
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_2_a1))
	{
		if (component.GetClassID () == ClassID (Transform))
		{
			if (error)
			{
				const char* goName = go->GetName ();
				const char* message = "Can't destroy Transform component of '%s'. "
					"If you want to destroy the game object, please call 'Destroy' on the game object instead. "
					"Destroying the transform component is not allowed.";

				*error = Format (message, goName);
			}

			return false;
		}
	}
	
	int componentClassID = component.GetClassID ();
	ScriptingClassPtr removeKlass = SCRIPTING_NULL;
	MonoBehaviour* removeMonoBehaviour = dynamic_pptr_cast<MonoBehaviour*> (&component);
	if (removeMonoBehaviour)
		removeKlass = removeMonoBehaviour->GetClass();
	
	int countSameComponentType = 0;
	bool mayRemove = true;
	for (int i=0;i<go->GetComponentCount ();i++)
	{
		int curClassID = go->GetComponentClassIDAtIndex (i);
		const vector_set<int>& requiredCom = FindRequiredComponentsForComponent (go->GetComponentClassIDAtIndex (i));
		
		// if the component we are releasing is needed by another component
		if (requiredCom.count (componentClassID))
		{
			if (error)
			{
				if (!mayRemove)
					*error += ", ";
				*error += Object::ClassIDToString (curClassID);
				mayRemove = false;
			}
		}
		
		// Check component requirement for scripts.
		// - Fetch all component requirement attributes
		// - Check if the class we remove is derived from the requirement
		if (curClassID == ClassID (MonoBehaviour))
		{
			MonoBehaviour* behaviour = (MonoBehaviour*)&go->GetComponentAtIndex(i);
			ScriptingClassPtr scriptClass = behaviour->GetClass();
			if (scriptClass)
			{
#if ENABLE_MONO

				MonoArray* array = RequiredComponentsOf(behaviour);
				
				if (array)
				{
					for (int j=0;j<mono_array_length(array);j++)
					{
						MonoObject* requiredClassObject = GetMonoArrayElement<MonoObject*>(array, j);
						if (requiredClassObject == NULL)
							continue;
						MonoClass* requiredClass = GetScriptingTypeRegistry().GetType(requiredClassObject);
						if (IsComponentSubclassOfMonoClass (component, requiredClass))
						{
							// Find if the new replacement meets the requirements
							bool replacementMeetRequirements = false;
							if (replacementClassID != -1)
							{
								MonoClass* replacementClass = GetMonoManager().ClassIDToScriptingClass(replacementClassID);
								// This check doesn't work with MonoBehaviour type system.
								// But currently component replacement only happens for Collider family.
								// Assert it is a subclass of Collider
								Assert(scripting_class_is_subclass_of(replacementClass, GetMonoManager().ClassIDToScriptingClass(ClassID(Collider))));
								if (replacementClass == requiredClass
									|| scripting_class_is_subclass_of(replacementClass, requiredClass))
								{
									replacementMeetRequirements = true;
								}
							}
							
							// Find if other components of the same GO meets the requirements
							bool otherComponentMeetRequirements = false;
							for (int k = 0; k < go->GetComponentCount(); ++k)
							{
								Unity::Component& otherComponent = go->GetComponentAtIndex(k);
								if (&otherComponent == &component // not the component being removed...
									|| &otherComponent == &go->GetComponentAtIndex(i)) // not the component asking requirements...
								{
									continue;
								}
								if (IsComponentSubclassOfMonoClass(otherComponent, requiredClass))
								{
									otherComponentMeetRequirements = true;
									break;
								}
							}
							
							if (!replacementMeetRequirements && !otherComponentMeetRequirements)
							{
								if (error)
								{
									if (!mayRemove)
										*error += ", ";
									*error += mono_class_get_name(scriptClass);
									*error += " (Script)";
									mayRemove = false;
								}
							}
						}
					}
				}
#endif // ENABLE_MONO
				
				if (removeKlass == scriptClass)
					countSameComponentType ++;
			}
		}
		else
		{
			if (curClassID == componentClassID)
				countSameComponentType ++;
		}
	}
	
	if (mayRemove || countSameComponentType > 1)
	{
		if (error)
			*error = "";
		return true;
	}
	else
	{
		if (error)
			 *error = Format ("Can't remove %s because %s depends on it", GetComponentOrScriptName (component).c_str (), error->c_str ());
		return false;
	}
}

#if UNITY_EDITOR

int GetMonoBehaviourEngineTypeTreeVariableCount ()
{
	static int gCount = -1;
	if (gCount == -1)
	{
		MonoBehaviour* temp = NEW_OBJECT(MonoBehaviour);
		temp->HackSetResetWasCalled();
		temp->HackSetAwakeWasCalled();
		
		TypeTree tree;
		if (temp)
		{
			GenerateTypeTree(*temp, &tree);
			DestroySingleObject(temp);
		}
		gCount = CountTypeTreeVariables(tree);
	}
	
	return gCount;
}

struct DisableMonoBehaviourPPtrSerialize
{
	dynamic_bitset override;
	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if (IsTypeTreePPtr (typeTree))
			override[typeTree.m_Index] = false;
		else if (IsTypeTreePPtrArray (typeTree))
			override[typeTree.m_Father->m_Index] = false;
		else if (typeTree.m_Index < GetMonoBehaviourEngineTypeTreeVariableCount())
			override[typeTree.m_Index] = false;
		
		return true;
	}
};

// Resets all non-pptr values.
// PPtr values that are already set to a value stay, otherwise they are set to the value of the default properties
// Does not call AwakeFromLoad so you need to do that yourself
static void ResetMonoBehaviourToScriptDefaults (MonoBehaviour& behaviour)
{
	MonoScript* script = behaviour.GetScript();
	if (script == NULL)
		return;

	if (script->GetScriptType() != kScriptTypeMonoBehaviourDerived && script->GetScriptType() != kScriptTypeScriptableObjectDerived && script->GetScriptType() != kScriptTypeEditorScriptableObjectDerived)
		return;
	
	// When there is a script class available
	if (script->GetClass())
	{
		// Create a clone and write out the pristine state
		MonoBehaviour* clone = NEW_OBJECT (MonoBehaviour);
		clone->HackSetResetWasCalled();
		clone->HackSetAwakeWasCalled();

		clone->SetScript(behaviour.GetScript());
		dynamic_array<UInt8> data(kMemTempAlloc);
		TypeTree typeTree;
		WriteObjectToVector (*clone, &data, kSerializeForPrefabSystem);
		GenerateTypeTree (*clone, &typeTree, kSerializeForPrefabSystem);
		
		DestroySingleObject (clone);

		// Read back replacing everything except pptrs
		ReadObjectFromVector (&behaviour, data, typeTree, kSerializeForPrefabSystem);
	}
	
	ApplyDefaultReferences(behaviour, script->GetDefaultReferences());
}

#endif


void UnloadGameObjectAndComponents (GameObject& go)
{
	LockObjectCreation();
	Assert(go.IsPersistent());
	Assert(!go.IsActive());
	
	for (int i=0;i<go.GetComponentCount();i++)
	{
		if (go.GetComponentAtIndexIsLoaded(i))
		{
			Unity::Component& com = go.GetComponentAtIndex(i);
			delete_object_internal (&com);
		}
	}
	
	delete_object_internal (&go);
	UnlockObjectCreation();
}

bool UnloadGameObjectHierarchy (GameObject& go)
{
	LockObjectCreation();
	Assert(!go.IsActive());
	Transform* transform = go.QueryComponent(Transform);
	//	AssertIf(transform && !transform->IsPersistent());
	for (int i=0;i<transform->GetChildrenCount();i++)
	{
		Transform& child = transform->GetChild(i);
		UnloadGameObjectHierarchy (child.GetGameObject());
	}
	
	///@TODO: This should probably be removed and fixed in the specific components that are misbehaving.
	
	// Run this once to make sure all components are actually loaded.
	// Otherwise, components of incompletely loaded GameObjects may be loaded in the process,
	// and when the reference other, previous Components of the same GO, those will be loaded as well
	// even though they should be unloaded already, causing crashes later on.
	for (int i=0;i<go.GetComponentCount();i++)
		go.GetComponentAtIndex(i);
		
	for (int i=0;i<go.GetComponentCount();i++)
	{
		Unity::Component& com = go.GetComponentAtIndex(i);
		delete_object_internal (&com);
	}
	
	delete_object_internal (&go);
	
	UnlockObjectCreation();
	return true;
}

void SendMessageToEveryone(MessageIdentifier message, MessageData msgData)
{
	// First, collect all GameObjects
	vector<GameObject*> gameObjects;
	Object::FindObjectsOfType (&gameObjects);
	
	// Next, keep a list of all instance IDs, since it is possible for any GameObject
	// to be destroyed when a message is handled.
	set<SInt32> ids;
	for (int i=0;i<gameObjects.size ();i++)
	{

		GameObject* go = gameObjects[i];
		if (go->IsActive ())
			ids.insert(go->GetInstanceID());			
	}
	
	// Send all active gameobjects a message
	for (set<SInt32>::iterator i=ids.begin ();i != ids.end ();i++)
	{
		GameObject* go = static_cast<GameObject*>(Object::IDToPointer(*i));
		if (go && go->IsActive())
			go->SendMessageAny (message, msgData);
	}
}

GameObject* FindGameObjectWithTag (UInt32 tag)
{
	GameObjectList& tagged = GetGameObjectManager().m_TaggedNodes;
	for (GameObjectList::iterator i=tagged.begin();i != tagged.end();i++)
	{
		GameObject& go = **i;
		Assert(go.IsActive() && go.GetTag() != 0);
		if (go.GetTag() == tag)
			return & go;
	}
	return NULL;
}

void FindGameObjectsWithTag (UInt32 tag, std::vector<Unity::GameObject*>& gos)
{
	GameObjectList& tagged = GetGameObjectManager().m_TaggedNodes;
	for (GameObjectList::iterator i=tagged.begin();i != tagged.end();i++)
	{	
		GameObject& go = **i;
		Assert(go.IsActive() && go.GetTag() != 0);

		if (go.GetTag() == tag)
			gos.push_back(&go);
	}
}

Camera* FindMainCamera ()
{
	std::vector<GameObject*> gos;
	FindGameObjectsWithTag(kMainCameraTag, gos);
	for (int i=0;i<gos.size();i++)
	{
		GameObject* go = gos[i];
		Camera* cam = go->QueryComponent(Camera);
		if (cam != NULL && cam->GetEnabled())
			return cam;
	}
	return NULL;
}

void SmartResetObject (Object& object)
{
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&object);
	if (behaviour)
	{
		#if UNITY_EDITOR
		if (!IsWorldPlaying())
			ResetMonoBehaviourToScriptDefaults(*behaviour);
		#endif

		behaviour->Reset();
		behaviour->SmartReset();
		behaviour->AwakeFromLoad(kDefaultAwakeFromLoad);
		behaviour->SetDirty();
	}
	else
	{
		object.Reset();
		object.SmartReset();
		object.AwakeFromLoad(kDefaultAwakeFromLoad);		
		object.SetDirty();
	}
}

Unity::Component* GetComponentWithScript (GameObject& go, int classID, MonoScriptPtr script)
{

	if (classID != ClassID(MonoBehaviour))
		return go.QueryComponentT<Unity::Component>(classID);
#if ENABLE_SCRIPTING
	if (script == NULL)
		return NULL;

	ScriptingClassPtr compareKlass = script->GetClass();
	if (compareKlass == NULL)
		return NULL;
	
	int count = go.GetComponentCount ();
	for (int i=0;i<count;i++)
	{
		// We are looking only for MonoBehaviours
		int clsID = go.GetComponentClassIDAtIndex (i);
		if (!Object::IsDerivedFromClassID (clsID, ClassID (MonoBehaviour)))
			continue;
		
		MonoBehaviour& behaviour = static_cast<MonoBehaviour&> (go.GetComponentAtIndex (i));
		ScriptingObjectPtr object = behaviour.GetInstance ();
		if (object)
		{
			ScriptingClassPtr klass = scripting_object_get_class(object, GetScriptingTypeRegistry());
			if (scripting_class_is_subclass_of (klass, compareKlass))
				return &behaviour;
		}
	}
#endif
	return NULL;
}

void GetComponentsInChildren (const GameObject& gameObject, bool includeInactive, int classID, dynamic_array<Unity::Component*>& outComponents)
{
	// Find components on this game object
	if (includeInactive || gameObject.IsActive())
	{
		for (int i=0;i<gameObject.GetComponentCount();i++)
		{
			if (Object::IsDerivedFromClassID(gameObject.GetComponentClassIDAtIndex(i), classID))
				outComponents.push_back(&gameObject.GetComponentAtIndex(i));
		}
	}
	
	// Recurse children
	Transform* transform = gameObject.QueryComponent(Transform);
	if (transform != NULL)
	{
		for (Transform::iterator i=transform->begin();i != transform->end();++i)
		{
			GameObject& child = (**i).GetGameObject ();
			GetComponentsInChildren(child, includeInactive, classID, outComponents);
		}
	}
}

static void AddToBatchDeleteAndMakeUnpersistent (Object& object, BatchDelete& batchDelete)
{
	if (object.IsPersistent())
		GetPersistentManager ().MakeObjectUnpersistent (object.GetInstanceID (), kDestroyFromFile);

	batchDelete.objects[batchDelete.objectCount++] = &object;
}

static void DestroyGameObjectRecursive (GameObject& gameObject, BatchDelete& batchDelete)
{		
	Assert(!gameObject.IsActive());
	Assert(gameObject.IsDestroying());
	
	Transform* transform = gameObject.QueryComponent (Transform);
	if (transform)
	{
		for (Transform::iterator i=transform->begin();i!=transform->end();i++)
		{
			Transform& transform = **i;
			DestroyGameObjectRecursive(*transform.GetGameObjectPtr(), batchDelete);
		}
	}

	if (gameObject.IsActivating())
	{
		if (transform)
			transform->RemoveFromParent();
		ErrorStringObject("Cannot destroy GameObject while it is being activated or deactivated.", &gameObject);
		return;
	}

	for (int i=0;i<gameObject.GetComponentCount();i++)
	{
		Unity::Component& com = gameObject.GetComponentAtIndex(i);
		AddToBatchDeleteAndMakeUnpersistent (com, batchDelete);
	}
	
	AddToBatchDeleteAndMakeUnpersistent (gameObject, batchDelete);
}

static void PreDestroyRecursive (GameObject& gameObject, size_t* destroyedObjectCount)
{
	if (gameObject.IsActivating())
	{
		ErrorStringObject("Cannot destroy GameObject while it is being activated or deactivated.", &gameObject);
		return;
	}
	
	
	// the callback is only called if the GameObject is
	// really destroyed (not only removed from memory)
	GameObject::InvokeDestroyedCallback(&gameObject);

	if (!IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
		gameObject.Deactivate(kWillDestroyGameObjectDeactivate);

	gameObject.WillDestroyGameObject();
	*destroyedObjectCount += 1 + gameObject.GetComponentCount();
	
	Transform* transform = gameObject.QueryComponent (Transform);
	if (transform)
	{
		for (Transform::iterator i=transform->begin();i!=transform->end();i++)
		{
			Transform& child = **i;
			PreDestroyRecursive(child.GetGameObject(), destroyedObjectCount);
		}
	}
}

#if UNITY_EDITOR
static void DisconnectPrefabInstanceRecursive (GameObject& gameObject)
{
	PrefabDestroyObjectCallback(gameObject);
	
	Transform* transform = gameObject.QueryComponent (Transform);
	if (transform == NULL)
		return;
	
	for (Transform::iterator i=transform->begin();i!=transform->end();i++)
	{
		Transform& child = **i;
		DisconnectPrefabInstanceRecursive(child.GetGameObject());
	}
}
#endif

void DestroyTransformComponentAndChildHierarchy (Transform& transform)
{
	size_t objectCount = 0;
	for (Transform::iterator i=transform.begin();i!=transform.end();i++)
	{
		Transform& child = **i;
		child.GetGameObject().Deactivate(kWillDestroyGameObjectDeactivate);
		PreDestroyRecursive(child.GetGameObject(), &objectCount);
	}
	
	// Remove transform from transform hierarchy
	transform.RemoveFromParent();
	
	BatchDelete batchDelete = CreateBatchDelete (objectCount);
	
	for (Transform::iterator i=transform.begin();i!=transform.end();i++)
	{
		Transform& child = **i;
		DestroyGameObjectRecursive(child.GetGameObject(), batchDelete);
	}
	
	CommitBatchDelete (batchDelete);
}

void DestroyGameObjectHierarchy (GameObject& gameObject)
{
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
	{	
		// Deactivate and mark is being destroyed recursively
		// Send all necessary callbacks etc.
		gameObject.Deactivate(kWillDestroyGameObjectDeactivate);
	}
	
	size_t objectCount = 0;
	PreDestroyRecursive(gameObject, &objectCount);
	
	// Remove transform from transform hierarchy
	Transform* transform = gameObject.QueryComponent(Transform);
	if (transform)
		transform->RemoveFromParent();
	
	BatchDelete batchDelete = CreateBatchDelete (objectCount);
	
	// Destroy the objects (There should be no callbacks happening at this stage anymore)
	DestroyGameObjectRecursive(gameObject, batchDelete);
	
	CommitBatchDelete (batchDelete);
}

void DestroyObjectHighLevel (Object* object, bool forceDestroy)
{	
	PROFILER_AUTO (gDestroyProfile, NULL)
	
	if (object)
	{
		if (object->IsDerivedFrom (ClassID (Component)))
		{
			Unity::Component& component = *static_cast<Unity::Component*> (object);
			MonoBehaviour* monoBehaviour = dynamic_pptr_cast<MonoBehaviour*> (object);
			
			// MonoBehaviour needs per
			if (monoBehaviour && monoBehaviour->IsDestroying())
			{
				ErrorString("Destroying object multiple times. Don't use DestroyImmediate on the same object in OnDisable or OnDestroy.");
				return;
			}
			
			
			GameObject* gameObject = component.GetGameObjectPtr();
			if (gameObject)
			{
				if (GetDisableImmediateDestruction())
				{
					ErrorStringObject ("Destroying components immediately is not permitted during physics trigger/contact, animation event callbacks or OnValidate. You must use Destroy instead.", object);
					return;
				}

				if (gameObject->IsDestroying())
				{
					ErrorString("Destroying object multiple times. Don't use DestroyImmediate on the same object in OnDisable or OnDestroy.");
					return;
				}

				
				if (gameObject->IsActivating())
				{
					ErrorStringObject("Cannot destroy Component while GameObject is being activated or deactivated.", gameObject);
					return;
				}
				
				string error;
				if (!forceDestroy && !CanRemoveComponent(component, &error))
				{
					ErrorStringObject (error, &component);
					return;
				}
				
				PPtr<Unity::Component> componentPPtr = &component;

				if (gameObject->IsActive ())
				{	
					component.Deactivate (kWillDestroySingleComponentDeactivate);
					
					// The game object might get destroyed during the OnDisable / OnDestroy callbacks, so don't rely on it.
					if ((Unity::Component*)componentPPtr != &component)
						return;
				}

				// Deleting a transform component is a special case, we have to destroy
				// the entire child transform hierarchy.
				// This is necessary to keep behaviour consistent for pre 3.2 content
				// Starting with 3.2 we prevent this in CanRemoveComponent
				if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_2_a1) && component.GetClassID() == ClassID (Transform))
				{
					DestroyTransformComponentAndChildHierarchy (static_cast<Transform&> (component));
					
					if ((Unity::Component*)componentPPtr != &component)
						return;
				}	
				
				component.WillDestroyComponent();

				// The game object might get destroyed during the OnDisable / OnDestroy callbacks, so don't rely on it.
				if ((Unity::Component*)componentPPtr != &component)
					return;
				
				int componentIndex = gameObject->GetComponentIndex(&component);
				if (componentIndex != -1)
					component.GetGameObject().RemoveComponentAtIndex (componentIndex);
				else
				{
					ErrorString("Component Removing internal failure");
				}
			}
			else
			{
				component.WillDestroyComponent();
			}
			
			#if UNITY_EDITOR
			PrefabDestroyObjectCallback(component);
			#endif
			
			DestroySingleObject (&component);
		}
		else if (object->IsDerivedFrom (ClassID (GameObject)))
		{
			if (GetDisableImmediateDestruction())
			{
				ErrorStringObject ("Destroying GameObjects immediately is not permitted during physics trigger/contact, animation event callbacks or OnValidate. You must use Destroy instead.", object);
				return;
			}

			GameObject& gameObject = *static_cast<GameObject*>(object);
			if (gameObject.IsDestroying())
			{
				ErrorString("Destroying object multiple times. Don't use DestroyImmediate on the same object in OnDisable or OnDestroy.");
				return;
			}
			
			if (gameObject.IsActivating())
			{
				ErrorStringObject("Cannot destroy GameObject while it is being activated or deactivated.", &gameObject);
				return;
			}
			
			Transform* parent = gameObject.QueryComponent(Transform);
			if (parent)
			{
				parent = parent->GetParent();
				if (parent && parent->GetGameObject().IsActivating())
				{
					ErrorStringObject("Cannot destroy GameObject while it is being activated or deactivated.", &gameObject);
					return;
				}
			}
			#if UNITY_EDITOR
			DisconnectPrefabInstanceRecursive (gameObject);
			#endif

			DestroyGameObjectHierarchy(gameObject);
		}
		else if (object->IsDerivedFrom (ClassID(AssetBundle)))
		{
			ErrorStringObject ("Destroying AssetBundle directly is not permitted.\nUse AssetBundle.UnloadBundle to destroy an asset bundle.", object);
			return;
		}
		else
		{
			DestroySingleObject(object);
		}
	}
}

std::string UnityObjectToString (Object *object)
{
	#if ENABLE_SCRIPTING
	std::string type;

	if (object == NULL) return "null";

	if (object->GetClassID() == ClassID(MonoBehaviour))
		type = dynamic_pptr_cast<MonoBehaviour*>(object)->GetScriptFullClassName();
	else
		type = "UnityEngine."+object->GetClassName();
	
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_2_a1))
		return Format("%s (%s)", object->GetName(), type.c_str());
	else	
		return type;
	#else
	return "UnityObject";
	#endif
}

Unity::Component* FindAncestorComponentExactTypeImpl (Unity::GameObject& gameObject, int classId)
{
	Transform* parent = gameObject.QueryComponent (Transform);
	while (parent != NULL)
	{
		Unity::Component* component = parent->GetGameObject().QueryComponentExactTypeImplementation (classId);
		if (component != NULL)
			return component;
			
		parent = parent->GetParent ();
	}
	return NULL;
}

Unity::Component* FindAncestorComponentImpl (Unity::GameObject& gameObject, int classId)
{
	Transform* parent = gameObject.QueryComponent (Transform);
	while (parent != NULL)
	{
		Unity::Component* component = parent->GetGameObject().QueryComponentImplementation (classId);
		if (component != NULL)
			return component;
			
		parent = parent->GetParent ();
	}
	return NULL;
}

#if ENABLE_SCRIPTING
int ExtractTagThrowing(ICallString& name)
{
	string tagString = name;
	int tag = StringToTag (tagString);
	if (tag != kUndefinedTag)
		return tag;
	else
	{
		Scripting::RaiseMonoException ("Tag: %s is not defined!", tagString.c_str());
		return tag;
	}
}
#endif
