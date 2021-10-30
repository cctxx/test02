#include "UnityPrefix.h"
#include "CreateEditor.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Scripting/ScriptingObjectOfType.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Scripting/Scripting.h"

using namespace std;

/// - Must call ResetAndApplyDefaultReferencesOnNewMonoBehaviour on the resulting  MonoBehaviour
static MonoBehaviour* CreateEditorScriptableObjectIncomplete (MonoClass* klass)
{
	MonoScript* script = GetMonoScriptManager().FindEditorScript(klass);
	
	if (script == NULL)
	{
		ErrorString (Format ("Instance of %s couldn't be created because there is no script with that name.", klass ? mono_class_get_name(klass) : ""));
		return NULL;
	}
	
	if (script->GetScriptType() != kScriptTypeEditorScriptableObjectDerived)
	{
		ErrorString (Format ("Instance of %s couldn't be created. The script class needs to derive from ScriptableObject and be placed in the Assets/Editor folder.", script->GetName()));
		return NULL;
	}
	if (script->GetClass() == NULL)
	{
		ErrorString (Format ("Instance of %s couldn't be created. All script needs to successfully compile first!", script->GetName()));
		return NULL;
	}
	
	MonoBehaviour* behaviour = NEW_OBJECT (MonoBehaviour);
	behaviour->SetScript(script);
	behaviour->SetHideFlags (Object::kHideAndDontSave);
	behaviour->SetEditorHideFlags (MonoBehaviour::kHideScriptPPtr);

	return behaviour;
}

static void GetClassesThatCouldServeAsEditorFor(Object* target_native, MonoObject* target_managed, InspectorMode mode, vector<MonoClass*>& result, bool multiEdit)
{
	bool allowCustomInspector = true;
	if (mode == kDebugInspector)
		allowCustomInspector = dynamic_pptr_cast<GameObject*> (target_native) != NULL || dynamic_pptr_cast<AssetImporter*> (target_native) != NULL;
	else if (mode == kAllPropertiesInspector)
		allowCustomInspector = false;

	Material* material = dynamic_pptr_cast<Material*> (target_native);
	if (material && allowCustomInspector)
	{
		MonoClass* klass = NULL;
		const char* materialInspector = material->GetShader ()->GetCustomEditorName ();
		if (materialInspector)
		{
			klass = GetMonoManager().GetMonoClass (materialInspector, "");

			if (!klass)
				klass = GetMonoManager().GetMonoClass (materialInspector, kEditorNameSpace);

			if (klass)
				result.push_back (klass);
		}
	}
	
	MonoClass* klass = NULL;
	if (target_managed != NULL && allowCustomInspector)
	{
		void* params[] = { target_managed, &multiEdit };
		MonoObject* type = CallStaticMonoMethod ("CustomEditorAttributes", "FindCustomEditorType", params);
		
		if (type != NULL)
		{
			klass = GetScriptingTypeRegistry().GetType(type);
			if (klass != NULL)
				result.push_back(klass);
		}
	}
	
	klass = GetMonoManager().GetMonoClass("GenericInspector", "UnityEditor");
	if (klass != NULL)
		result.push_back(klass);
}

MonoBehaviour* CreateInspectorMonoBehaviour (std::vector <PPtr<Object> > &objs, ScriptingObjectPtr forcedClass, InspectorMode mode, bool isHidden)
{
	//Filter out invalid objects (deleted ect)
	std::vector <PPtr<Object> > notNullObjects;
	for(int i=0; i<objs.size(); i++)
	{
		if (objs[i].IsValid())
			notNullObjects.push_back (objs[i]);
	}
	
	if (notNullObjects.empty())
		return NULL;
	
	MonoArray* targets = mono_array_new (mono_domain_get(), GetMonoManager().GetCommonClasses ().unityEngineObject, notNullObjects.size());
	for(int i=0; i<notNullObjects.size(); i++)
		GetMonoArrayElement<MonoObject*>(targets, i) = Scripting::ScriptingWrapperFor(notNullObjects[i]);
	
	// Create temporary replacement object if the script can't be loaded!
	if (notNullObjects.size() && GetMonoArrayElement<MonoObject*>(targets, 0) == NULL && notNullObjects[0]->GetClassID () == ClassID(MonoBehaviour))
	{
		for (int i=0; i<notNullObjects.size(); i++)
		{
			MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (notNullObjects[i]);
			
			ScriptingClassPtr klass = (behaviour->GetGameObjectPtr() == NULL) ? MONO_COMMON.scriptableObject : MONO_COMMON.monoBehaviour;
			GetMonoArrayElement<MonoObject*>(targets, i) = scripting_object_new (klass);
			
			ScriptingObjectOfType<Object>(GetMonoArrayElement<MonoObject*>(targets, i)).SetInstanceID(notNullObjects[i]->GetInstanceID());
		}
	}
	
	vector<MonoClass*> klasses;
	if (forcedClass != NULL)
		klasses.push_back(GetScriptingTypeRegistry().GetType (forcedClass));
	else
		GetClassesThatCouldServeAsEditorFor(*notNullObjects.begin(), GetMonoArrayElement<MonoObject*>(targets, 0), mode, klasses, notNullObjects.size() > 1);
	
	// For all classes that could be the editor for our target, try to create one. we'll use the first one we can succesfully instantiate.
	MonoBehaviour* behaviour = NULL;
	for (int i=0; i!=klasses.size(); i++)
	{
		behaviour = CreateEditorScriptableObjectIncomplete(klasses[i]);
		if (behaviour != NULL)
			break;
	}
	
	if (behaviour == NULL)
		return NULL;
	
	//setup the editor to inspect the correct target.
	ScriptingMethodPtr settarget = behaviour->FindMethod("InternalSetTargets");
	if (settarget)
	{
		void* args[] = { targets };
		MonoException* exc;
		mono_runtime_invoke_profiled (settarget->monoMethod, behaviour->GetInstance(), args, &exc);
		if (exc)
			Scripting::LogException(exc, 0);
	}
	
	ScriptingMethodPtr setHidden = behaviour->FindMethod("InternalSetHidden");
	if (setHidden)
	{
		void* args[] = { &isHidden };
		MonoException* exc;
		mono_runtime_invoke_profiled (setHidden->monoMethod, behaviour->GetInstance(), args, &exc);
		if (exc)
			Scripting::LogException(exc, 0);
	}
	
	ResetAndApplyDefaultReferencesOnNewMonoBehaviour(*behaviour);
	
	behaviour->SetDirty();
	
	SetCustomEditorIsDirty(behaviour, true);
	return behaviour;
}

// This must be kept in sync with the C# Editor class EditorBindings.txt
struct MonoInspectorData
{
	UnityEngineObjectMemoryLayout data;
	MonoObject* m_Target;
	int         m_Dirty;
};


void SetCustomEditorIsDirty (MonoBehaviour* inspector, bool dirty)
{
	if (inspector && inspector->GetInstance())
		ExtractMonoObjectData<MonoInspectorData> (inspector->GetInstance()).m_Dirty = dirty;
}

bool IsCustomEditorDirty (MonoBehaviour* inspector)
{
	if (inspector && inspector->GetInstance())
		return ExtractMonoObjectData<MonoInspectorData> (inspector->GetInstance()).m_Dirty;
	else
		return false;
}

