#include "UnityPrefix.h"

#include "Scripting.h"

#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/BaseClasses/RefCounted.h"
#include "Runtime/GameCode/DestroyDelayed.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Scripting/ScriptingObjectOfType.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/ScriptingObjectWithIntPtrField.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"

#if UNITY_EDITOR
# include "Editor/Src/AssetPipeline/MonoCompilationPipeline.h"
#endif

namespace Scripting
{

ScriptingObjectPtr ScriptingWrapperFor(Object* o)
{
#if !ENABLE_SCRIPTING
	return 0;
#else
	if(!o)
		return SCRIPTING_NULL;
	
	ScriptingObjectPtr cachedInstance = o->GetCachedScriptingObject();
	if(cachedInstance != SCRIPTING_NULL)
		return cachedInstance;

	int classID = o->GetClassID();
	if(classID == ClassID(MonoBehaviour))
	{
		AssertIf(static_cast<MonoBehaviour*> (o)->GetInstance());
		return SCRIPTING_NULL;
	}

	ScriptingObjectPtr object = InstantiateScriptingWrapperForClassID(classID);
	return (object != SCRIPTING_NULL) ? ConnectScriptingWrapperToObject(object, o) : SCRIPTING_NULL;
#endif
}

ScriptingObjectPtr ConnectScriptingWrapperToObject(ScriptingObjectPtr object, Object* ptr)
{
#if !ENABLE_SCRIPTING
	return 0;
#else
	// ConnectScriptingWrapperToObject might get called from different threads
	// - References to objects are setup from loading thread.
	//   The Object could already be loaded in memory and in that case it is possible that the main thread might asign a MonoObject* while the loader thread is doing the same.
	// - @TODO: If this is a performance bottleneck we could ensure that the loader thread calls ConnectScriptingWrapperToObject only during safe places.
	
	LockObjectCreation();
	
	AssertIf(object == SCRIPTING_NULL);
	AssertIf(ptr == NULL);
	
	if(ptr->GetCachedScriptingObject() != SCRIPTING_NULL)
	{
		ScriptingObjectPtr res = ptr->GetCachedScriptingObject();
		UnlockObjectCreation();
		
		return res;
	}

	ScriptingObjectOfType<Object> wrapper(object);

#if DEBUGMODE && !UNITY_FLASH && !UNITY_WINRT //in flash, we do not yet initialize chachedPtr to 0
	AssertIf(wrapper.GetCachedPtr() != NULL);
#endif
	
	wrapper.SetCachedPtr(ptr);
	wrapper.SetInstanceID(ptr->GetInstanceID());

#if MONO_QUALITY_ERRORS
	wrapper.SetError(NULL);
#endif
	
	AssertIf(ptr->GetCachedScriptingObject() != SCRIPTING_NULL);
	ptr->SetCachedScriptingObject(object);
	
	UnlockObjectCreation();
	
	return object;
#endif
}

bool BroadcastScriptingMessage(GameObject& go, const char* name, ScriptingObjectPtr param)
{
	bool didSend = SendScriptingMessage(go, name, param);

	Transform* transform = go.QueryComponent(Transform);
	if(transform)
	{
		for(int i = 0; i < transform->GetChildrenCount(); i++ )
			didSend |= BroadcastScriptingMessage(transform->GetChild(i).GetGameObject(), name, param);
	}

	return didSend;
}


bool SendScriptingMessageUpwards(GameObject& go, const char* name, ScriptingObjectPtr param)
{
	bool didSend = SendScriptingMessage(go, name, param);

	Transform* transform = go.QueryComponent(Transform);
	if (transform)
		transform = transform->GetParent();
	
	while(transform)
	{
		didSend |= SendScriptingMessage(transform->GetGameObject(), name, param);
		transform = transform->GetParent();
	}

	return didSend;
}

bool SendScriptingMessage(GameObject& go, const char* name, ScriptingObjectPtr param)
{
	bool didSend = false;
	
	if(!go.IsActive())
		return false;
	
	int instanceID = go.GetInstanceID();

	for(int i = 0; i < go.GetComponentCount(); i++)
	{
		if(go.GetComponentClassIDAtIndex(i) == ClassID(MonoBehaviour))
		{
			MonoBehaviour& behaviour = static_cast<MonoBehaviour&>(go.GetComponentAtIndex(i));

			ScriptingObjectPtr instance = behaviour.GetInstance();
			if(instance)
			{
				ScriptingMethodPtr method = behaviour.FindMethod(name);
				if(method == SCRIPTING_NULL)
					continue;

				behaviour.InvokeMethodOrCoroutineChecked(method, param);
				didSend = true;

				// Check if the gameObject was destroyed.
				if(!PPtr<Object>(instanceID).IsValid())
					return didSend;
			}
		}
	}

	return didSend;
}

bool BroadcastScriptingMessage(GameObject& go, const std::string& methodName, ScriptingObjectPtr param, int options)
{
	bool didSend = BroadcastScriptingMessage(go, methodName.c_str(), param);

#if !DEPLOY_OPTIMIZED
	if(!didSend && options == 0)
		ErrorStringObject(Format("BroadcastMessage %s has no receiver!", methodName.c_str()), &go);
#endif
	
	return didSend;
}

bool SendScriptingMessageUpwards(GameObject& go, const std::string& methodName, ScriptingObjectPtr param, int options)
{
	bool didSend = SendScriptingMessageUpwards(go, methodName.c_str(), param);

#if !DEPLOY_OPTIMIZED
	if(!didSend && options == 0)
		ErrorStringObject(Format("SendMessage %s has no receiver!", methodName.c_str()), &go);
#endif
	
	return didSend;
}

bool SendScriptingMessage(GameObject& go, const std::string& methodName, ScriptingObjectPtr param, int options)
{
	bool didSend = SendScriptingMessage(go, methodName.c_str(), param);

#if !DEPLOY_OPTIMIZED
	if(!didSend && options == 0)
		ErrorStringObject(Format("SendMessage %s has no receiver!", methodName.c_str()), &go);
#endif
	
	return didSend;
}

/// Compares two Object classes.
/// Returns true if both have the same instance id
/// or both are NULL (Null can either mean that the object is gone or that the instanceID is 0)
bool CompareBaseObjects (ScriptingObjectPtr lhs, ScriptingObjectPtr rhs)
{
	int lhsInstanceID = 0;
	int rhsInstanceID = 0;
	bool isLhsNull = true, isRhsNull = true;
	if (lhs)
	{
		lhsInstanceID = GetInstanceIDFromScriptingWrapper (lhs);
		ScriptingObjectOfType<Object> lhsRef(lhs);
		isLhsNull = !lhsRef.IsValidObjectReference();
	}
	if (rhs)
	{
		rhsInstanceID = GetInstanceIDFromScriptingWrapper (rhs);
		ScriptingObjectOfType<Object> rhsRef(rhs);
		isRhsNull = !rhsRef.IsValidObjectReference();
	}
	if (isLhsNull || isRhsNull)
		return isLhsNull == isRhsNull;
	else
		return lhsInstanceID == rhsInstanceID;
}

void DestroyObjectFromScriptingImmediate(Object* object, bool allowDestroyingAssets)
{
#if !DEPLOY_OPTIMIZED		
	if(object && object->IsPersistent() && !allowDestroyingAssets)
	{
		ErrorStringObject ("Destroying assets is not permitted to avoid data loss.\nIf you really want to remove an asset use DestroyImmediate (theObject, true);", object);
		return;
	}
#endif

	DestroyObjectHighLevel(object);
}

void UnloadAssetFromScripting(Object* object)
{
	if(object == NULL)
		return;
	
#if !DEPLOY_OPTIMIZED		
	if(!object->IsPersistent())
	{
		ErrorStringObject ("UnloadAsset can only be used on assets;", object);
		return;
	}

	bool disallowedUnloadTypes = object->IsDerivedFrom(ClassID(Component)) || object->IsDerivedFrom(ClassID(GameObject)) || object->IsDerivedFrom(ClassID(AssetBundle));
	if(disallowedUnloadTypes)
	{
		ErrorStringObject ("UnloadAsset may only be used on individual assets and can not be used on GameObject's / Components or AssetBundles", object);
		return;
	}
#endif
	
	UnloadObject(object);
}

static void DisableBehaviours( GameObject* target )
{
	for ( unsigned compI = 0; compI < target->GetComponentCount(); ++compI ) 
	{
		Behaviour* behaviour = dynamic_pptr_cast<Behaviour*> (&target->GetComponentAtIndex(compI));
		if (behaviour)
			behaviour->SetEnabled(false);
	}
}

// @TODO: Test for DestroyImmediate from Destroy in OnDisable!
void DestroyObjectFromScripting (PPtr<Object> object, float t)
{
	if (!IsWorldPlaying())
	{
		ErrorString("Destroy may not be called from edit mode! Use DestroyImmediate instead.\nAlso think twice if you really want to destroy something in edit mode. Since this will destroy objects permanently.");
		return;
	}
	
	if (object)
	{
#if !DEPLOY_OPTIMIZED
		if (object->IsPersistent ())
		{
			ErrorStringObject ("Destroying assets is not permitted to avoid data loss.\nIf you really want to remove an asset use DestroyImmediate (theObject, true);", object);
			return;
		}
#endif
		
		// if we want to destroy it this frame (t <= 0.0) - we need to imitate "destroy right away" behaviour
		if (t <= 0.0)
		{
			Behaviour* behaviour = dynamic_pptr_cast<Behaviour*> (object);
			if (behaviour)
			{
				// if this Object is some kind of Behaviour, it is already in update queue.
				// if it was updated before we can do nothing, but if not - just disable it
				behaviour->SetEnabled(false);
			}
			
			GameObject* go = dynamic_pptr_cast<GameObject*> (object);
			if (go)
			{
				// if we destroy game object - disable all behaviours (children included)
				DisableBehaviours(go);
				
				////@TODO: This needs to be done recursively
				
				Transform& root = go->GetComponentT<Transform>(ClassID(Transform));
				for ( unsigned childI = 0; childI < root.GetChildrenCount(); ++childI ) 
					DisableBehaviours( root.GetChild(childI).GetGameObjectPtr() );
			}
		}
		
		DestroyObjectDelayed (object, t);
	}
}

static MonoScript* FindScriptableObjectFromClass (ScriptingTypePtr klass, bool errorOnMissingScript = true)
{
	if (klass == SCRIPTING_NULL)
	{
		ErrorString ("Instance couldn't be created because type was null.");
		return NULL;
	}
	
	MonoScript* script = GetMonoScriptManager().FindRuntimeScript(klass);
	if (script == NULL)
	{
		script = GetMonoScriptManager().FindEditorScript(klass);
		if (script == NULL)
		{
			if (errorOnMissingScript)
			{
				ErrorString (Format ("Instance of %s couldn't be created because there is no script with that name.", scripting_class_get_name(klass)));
			}
			return NULL;
		}
	}
	
	if (script->GetScriptType() != kScriptTypeScriptableObjectDerived && script->GetScriptType() != kScriptTypeEditorScriptableObjectDerived)
	{
		ErrorString (Format ("Instance of %s couldn't be created. The the script class needs to derive from ScriptableObject.", scripting_class_get_name(klass)));
		return NULL;
	}
	
	if (script->GetClass() == SCRIPTING_NULL)
	{
		ErrorString (Format ("Instance of %s couldn't be created. All script needs to successfully compile first!", scripting_class_get_name(klass)));
		return NULL;
	}
	
	return script;
}

ScriptingObjectPtr CreateScriptableObjectWithType (ScriptingObjectPtr systemTypeInstance)
{
	ScriptingTypePtr type = GetScriptingTypeRegistry().GetType(systemTypeInstance);
	if(type == NULL)
		return SCRIPTING_NULL;

	MonoBehaviour* behaviour = 0;

	bool errorOnMissingScript = UNITY_EDITOR ? false : true;

	MonoScript* script = FindScriptableObjectFromClass(type, errorOnMissingScript);
	if(script == NULL)
	{
#if UNITY_EDITOR
		MonoImage* image = mono_class_get_image(type);
		std::string imageFileName = mono_image_get_filename(image);

		if (IsEditorOnlyAssembly(imageFileName, true))
		{
			std::string imageName = mono_image_get_name(image);
			std::string id = BuildScriptClassId(imageName, mono_class_get_namespace(type), mono_class_get_name(type));

			behaviour = NEW_OBJECT (MonoBehaviour);
			behaviour->SetClassIdentifier(id);
		}
		else
			return SCRIPTING_NULL;
#else
		return SCRIPTING_NULL;
#endif
	} else
	{
		behaviour = NEW_OBJECT (MonoBehaviour);
		behaviour->SetScript(script);
	}
	
	ResetAndApplyDefaultReferencesOnNewMonoBehaviour(*behaviour);
	return behaviour->GetInstance();
}

ScriptingObjectPtr CreateScriptableObject(const std::string& name)
{	
	MonoScript* script = GetMonoScriptManager().FindRuntimeScript(name);

#if UNITY_EDITOR
	if(script == NULL)
		script = GetMonoScriptManager().FindEditorScript(name);
#endif

	if(script == NULL)
	{
		ErrorString(Format("Instance of %s couldn't be created because there is no script with that name.", name.c_str()));
		return SCRIPTING_NULL;
	}

	if(
	    script->GetScriptType() != kScriptTypeScriptableObjectDerived
#if UNITY_EDITOR
	    && script->GetScriptType() != kScriptTypeEditorScriptableObjectDerived
#endif
	   )
	{
		ErrorString(Format("Instance of %s couldn't be created. The the script class needs to derive from ScriptableObject.", name.c_str()));
		return SCRIPTING_NULL;
	}

	if(script->GetClass() == SCRIPTING_NULL)
	{
		ErrorString(Format("Instance of %s couldn't be created. All script needs to successfully compile first!", name.c_str()));
		return SCRIPTING_NULL;
	}
	
	MonoBehaviour* behaviour = NEW_OBJECT(MonoBehaviour);
	behaviour->SetScript(script);
	
	ResetAndApplyDefaultReferencesOnNewMonoBehaviour(*behaviour);
	
	return behaviour->GetInstance();
}

void CreateEngineScriptableObject(ScriptingObjectPtr object)
{
	// Avoid recursive loop. This object is already created from c++

	if(GetInstanceIDFromScriptingWrapper(object) != 0)
		return;
	
	SCRIPTINGAPI_THREAD_CHECK(ScriptableObject.ctor);
	
	ScriptingClassPtr klass = scripting_object_get_class(object, GetScriptingTypeRegistry());

	const char *classNamespace = scripting_class_get_namespace(klass);
	const char *className = scripting_class_get_name(klass);

	WarningStringMsg("%s%s%s must be instantiated using the ScriptableObject.CreateInstance method instead of new %s.", classNamespace, (classNamespace[0] ? "." : ""), className, className);

	MonoScript* script = FindScriptableObjectFromClass(klass);
	if(script == NULL)
		return;
	
	MonoBehaviour* behaviour = NEW_OBJECT(MonoBehaviour);
	behaviour->SetScript(script, object);

#if UNITY_EDITOR
	if(script->GetScriptType() == kScriptTypeEditorScriptableObjectDerived)
		behaviour->SetEditorHideFlags (MonoBehaviour::kHideScriptPPtr);
#endif
	
	ResetAndApplyDefaultReferencesOnNewMonoBehaviour(*behaviour);
}

ScriptingObjectPtr GetScriptingWrapperForInstanceID(int instanceID)
{
	if(instanceID == 0)
		return SCRIPTING_NULL;
	
	Object* object2 = PPtr<Object>(instanceID);
	return ScriptingWrapperFor(object2);
}

int GetClassIDFromScriptingClass(ScriptingClassPtr klass)
{
	int classID = 0;

	// Look up this classes classID
	ScriptingClassPtr curClass = klass;
	
	const char* className = scripting_class_get_name(curClass);
	const char* nameSpace = scripting_class_get_namespace(curClass);
	const char* kEngineNameSpace = "UnityEngine";

	if(strcmp(nameSpace, kEngineNameSpace) == 0)
	{
		if(strcmp(className, "ScriptableObject") == 0)
			className = "MonoBehaviour";

		classID = Object::StringToClassID(className);
		if(classID != -1)
			return classID;
	}

	// It is not an engine class so check the parents
	classID = -1;

	ScriptingClassPtr parentClass = scripting_class_get_parent(klass, GetScriptingTypeRegistry());
	if(parentClass)
		classID = GetClassIDFromScriptingClass(parentClass);
	
	return classID;
}

ScriptingTypePtr ClassIDToScriptingType(int classID)
{
	return GetScriptingManager().ClassIDToScriptingClass(classID);
}

ScriptingObjectPtr GetScriptingWrapperOfComponentOfGameObject (GameObject& go, const std::string& name)
{
	int classID = Object::StringToClassID(name);
	if (classID != -1 && Object::IsDerivedFromClassID(classID, ClassID(Component)))
	{
		Unity::Component* component = go.QueryComponentT<Unity::Component> (classID);

#if MONO_QUALITY_ERRORS
		if (component == NULL)
			return MonoObjectNULL (classID, MissingComponentString(go, classID));
#endif
		return ScriptingWrapperFor (component);
	}
	
	MonoScript* script = GetMonoScriptManager().FindRuntimeScript(name);
	if (script == NULL)
	{

#if MONO_QUALITY_ERRORS
		if (classID != -1)
			ScriptWarning(Format("GetComponent requires that the requested component inherits from Component or MonoBehaviour.\nYou used GetComponent(%s) which does not inherit from Component.\n", name.c_str()), &go);
#endif

		return SCRIPTING_NULL;
	}
	
	ScriptingTypePtr compareKlass = script->GetClass();
	if (compareKlass == SCRIPTING_NULL)
		return SCRIPTING_NULL;
	
	int count = go.GetComponentCount ();
	for (int i=0;i<count;i++)
	{
		// We are looking only for MonoBehaviours
		int clsID = go.GetComponentClassIDAtIndex (i);
		if (!Object::IsDerivedFromClassID (clsID, ClassID (MonoBehaviour)))
			continue;

		MonoBehaviour& behaviour = static_cast<MonoBehaviour&> (go.GetComponentAtIndex (i));
		ScriptingObjectPtr object = behaviour.GetInstance ();
		
		if(!object)
			continue;

		ScriptingTypePtr klass = scripting_object_get_class(object, GetScriptingTypeRegistry());
		if (scripting_class_is_subclass_of(klass, compareKlass)) 
			return object;
	}
	
	return SCRIPTING_NULL;
}

void LogException(ScriptingExceptionPtr exception, int instanceID, const std::string& error)
{
	StackTraceInfo info;

	scripting_stack_trace_info_for(exception, info);

	if (!error.empty())
		info.condition = error + info.condition;

	DebugStringToFilePostprocessedStacktrace(info.condition.c_str(), info.strippedStacktrace.c_str(), info.stacktrace.c_str(), info.errorNum, info.file.c_str(), info.line, kLog | kScriptingError | kScriptingException, instanceID, 0);
}

static ScriptingArrayPtr CreateScriptingArrayFromScriptingObjects(ScriptingObjectPtr* objects, int size, ScriptingClassPtr typeForArray)
{
	// copy into array
	ScriptingArrayPtr array = CreateScriptingArray<ScriptingObjectPtr> (typeForArray, size);
	for (int i = 0; i < size; i++)
	{
		Scripting::SetScriptingArrayElement (array, i, objects[i]);
	}

	return array;
}

static bool IsActiveSceneObject (Object& o)
{
	if (o.IsPersistent ())
		return false;
	
	GameObject* go = dynamic_pptr_cast<GameObject*> (&o);
	if (go)
		return go->IsActive ();
	
	Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (&o);
	if (com)
	{
		MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&o);
		if (behaviour)
		{
			MonoScript* script = behaviour->GetScript();
			if (script && script->GetScriptType () == kScriptTypeScriptableObjectDerived)
				return true;
			else
				return com->IsActive ();
		}
		else
			return com->IsActive ();
	}
	
	return true;
}

ScriptingArrayPtr FindObjectsOfType(ScriptingObjectPtr systemTypeInstance, FindMode mode)
{
	ScriptingClassPtr compareKlass = GetScriptingTypeRegistry().GetType(systemTypeInstance);

	if(compareKlass == SCRIPTING_NULL)
	{
		ErrorString("FindAllObjectsOfType: Invalid Type");
		return SCRIPTING_NULL;
	}
	
	int classID = GetClassIDFromScriptingClass(compareKlass);
	if(classID == -1)
	{
		string klassName = scripting_class_get_name(compareKlass);
		ErrorString("FindAllObjectsOfType: The type has to be derived from UnityEngine.Object. Type is " + klassName + ".");
		return SCRIPTING_NULL;
	}
	
	// Gather the derived objects
	vector<SInt32> objects;
	Object::FindAllDerivedObjects(classID, &objects);
	
	std::sort(objects.begin(), objects.end());
	
	// We might need to ignore some objects which are not derived from the mono class but from MonoBehaviour
	// so we store them in a buffer and then copy them into the mono array

	ScriptingObjectPtr* scriptingObjects;

#if UNITY_WINRT
	scriptingObjects = new ScriptingObjectPtr[objects.size ()];
#else
	ALLOC_TEMP(scriptingObjects, ScriptingObjectPtr, objects.size ());
#endif
	
	int count = 0;
	for(int i = 0;i < objects.size (); i++)
	{
		Object& object = *PPtr<Object>(objects[i]);
		if(object.TestHideFlag (Object::kDontSave) && mode != Scripting::kFindAnything)
			continue;

		if(mode == Scripting::kFindActiveSceneObjects && !IsActiveSceneObject (object))
			continue;

		ScriptingObjectPtr mono = ScriptingWrapperFor(&object);  //Todo: this used to call ObjectToScriptingObject, but that
																		//hit a bug in the alchemy compiler
		if(mono)
		{
			// Cubemap is derived from Texture2D in serialization, but not in Scripting
			if (object.IsDerivedFrom (ClassID (MonoBehaviour)) || object.IsDerivedFrom (ClassID (Cubemap)))
			{	
				ScriptingClassPtr klass = scripting_object_get_class (mono, GetScriptingTypeRegistry());
				if (scripting_class_is_subclass_of (klass, compareKlass))
					scriptingObjects[count++] = mono;
			}
			else
			{
				scriptingObjects[count++] = mono;
			}
		}
	}

	ScriptingArrayPtr result = CreateScriptingArrayFromScriptingObjects(scriptingObjects,count,compareKlass);

#if UNITY_WINRT
	delete[]scriptingObjects;
#endif

	return result;
}

ScriptingObjectPtr TrackedReferenceBaseToScriptingObjectImpl (TrackedReferenceBase* base, ScriptingClassPtr klass)
{
	if (base)
	{
		// In the editor a weak reference (for example animation state)
		// might get leaked when reloading domains while in playmode
		// in that case we just recycle it
		#if UNITY_EDITOR
		if (base->m_MonoObjectReference != 0 && !mono_gchandle_is_in_domain(base->m_MonoObjectReference, mono_domain_get()))
		{
			mono_gchandle_free(base->m_MonoObjectReference);
			base->m_MonoObjectReference = 0;
		}
		#endif
#if ENABLE_MONO && !UNITY_PEPPER
		AssertIf(base->m_MonoObjectReference != 0 && !mono_gchandle_is_in_domain(base->m_MonoObjectReference, mono_domain_get()));
#endif

		// Get cached mono object reference
		ScriptingObjectPtr target = SCRIPTING_NULL;
		if (base->m_MonoObjectReference != 0)
		{	
			target = scripting_gchandle_get_target (base->m_MonoObjectReference);
			if (target)
				return target;
			
			AssertString("This should never happen");
			
			scripting_gchandle_free(base->m_MonoObjectReference);
			base->m_MonoObjectReference = 0;
		}
		
		ScriptingObjectWithIntPtrField<TrackedReferenceBase> newTarget(scripting_object_new(klass));

		base->m_MonoObjectReference = scripting_gchandle_new(newTarget.object);
		newTarget.SetPtr(base);
		
		return newTarget.object;
	}
	else
	{
#if DEPLOY_OPTIMIZED
		return SCRIPTING_NULL;
#else
		ScriptingObjectPtr object = scripting_object_new (klass);
		void* nativePointer = 0;
		MarshallNativeStructIntoManaged(nativePointer,object);
		return object;
#endif
	}		
}

template <class StringType>
ScriptingArrayPtr StringVectorToMono (const std::vector<StringType>& source)
{
	ScriptingArrayPtr arr =  CreateScriptingArray<ScriptingObjectPtr>(MONO_COMMON.string, source.size());
	for (int i = 0; i < source.size();i++) 
	{
		Scripting::SetScriptingArrayElement(arr, i, scripting_string_new(source[i]));
	}
	return arr;
}

template ScriptingArrayPtr StringVectorToMono (const std::vector<std::string>& source);
template ScriptingArrayPtr StringVectorToMono (const std::vector<UnityStr>& source);

ScriptingObjectPtr GetComponentObjectToScriptingObject(Unity::Component* com, Unity::GameObject& go, int classID)
{

#if MONO_QUALITY_ERRORS
	if(com == NULL)
		return MonoObjectNULL(classID, MissingComponentString(go, classID));
#endif
	
	return ScriptingWrapperFor(com);
}

ScriptingObjectPtr GetComponentObjectToScriptingObject(Object* com, Unity::GameObject& go, int classID)
{
	return ScriptingWrapperFor(com);
}

} // namespace Scripting
