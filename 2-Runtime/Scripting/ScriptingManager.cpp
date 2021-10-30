#include "UnityPrefix.h"
#if ENABLE_SCRIPTING
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include <vector>
#include <stdlib.h>
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/Scripting/Backend/ScriptingMethodFactory.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"

const char* kEngineNameSpace = "UnityEngine";
const char* kEditorNameSpace = "UnityEditor";
const char* kEditorInternalNameSpace = "UnityEditorInternal";

ScriptingClassPtr gClassIDToClass = SCRIPTING_NULL;

using namespace std;

ScriptingManager::ScriptingManager (MemLabelId label, ObjectCreationMode mode, IScriptingTypeProvider* scriptingTypeProvider, IScriptingMethodFactory* scriptingMethodFactory)
	:	GlobalGameManager(label, mode)
{
	m_ScriptingMethodFactory = scriptingMethodFactory;
	m_ScriptingTypeRegistry = UNITY_NEW(ScriptingTypeRegistry(scriptingTypeProvider), kMemManager);
	m_ScriptingMethodRegistry = UNITY_NEW(ScriptingMethodRegistry(scriptingMethodFactory, m_ScriptingTypeRegistry), kMemManager);
	
	//Registering ourselves as the monomanager manually, so that during the execution of the constructor GetMonoManager() will work.
	SetManagerPtrInContext(ManagerContext::kMonoManager, this);
}

ScriptingManager::~ScriptingManager ()
{
	UNITY_DELETE(m_ScriptingTypeRegistry, kMemManager);
	UNITY_DELETE(m_ScriptingMethodRegistry, kMemManager);

	UNITY_DELETE(m_ScriptingMethodFactory, kMemManager);
}

static ScriptingClassPtr FindScriptingClassForClassID(int classID, ScriptingClassPtr baseObject)
{
	const char* className_c;
#if UNITY_FLASH
	if(classID == ClassID(Object))
		className_c = "_Object";	
	else
#endif
	className_c = Object::ClassIDToString (classID).c_str();
	
	// Found a class?
	// Also make sure it derives from object otherwise it's not valid (Eg. RenderSettings only has public accessors and doesn't inherit from Object)	
	
	ScriptingTypeRegistry& typeRegistry = GetScriptingManager().GetScriptingTypeRegistry();

	ScriptingTypePtr result = typeRegistry.GetType(kEngineNameSpace,className_c);

#if UNITY_EDITOR
	if (result== SCRIPTING_NULL)
		result = typeRegistry.GetType (kEditorNameSpace, className_c);
	if (result == SCRIPTING_NULL)		  
		result = typeRegistry.GetType (kEditorInternalNameSpace, className_c);
#endif
	
	if (result != SCRIPTING_NULL && scripting_class_is_subclass_of (result, baseObject))
		return result;

	return SCRIPTING_NULL;
}


static ScriptingClassPtr FindScriptingClassForClassIDRecursive (int classID, ScriptingClassPtr baseObject)
{
	ScriptingClassPtr result = FindScriptingClassForClassID(classID,baseObject);
	
	if (result != SCRIPTING_NULL)
		return result;

	if (classID == ClassID (Object))
		return SCRIPTING_NULL;
	
	return FindScriptingClassForClassIDRecursive (Object::GetSuperClassID (classID),baseObject);
}

void ScriptingManager::RebuildClassIDToScriptingClass ()
{
#if ENABLE_SCRIPTING
	// Collect all engine classes
	vector<SInt32> allEngineClasses;
	Object::FindAllDerivedClasses (ClassID (Object), &allEngineClasses, false);
	
	// Resize lookup table to fit all engine classes
	SInt32 highest = 0;
	for (int i=0;i<allEngineClasses.size ();i++)
		highest = max (allEngineClasses[i], highest);
	m_ClassIDToMonoClass.clear ();
	m_ClassIDToMonoClass.resize (highest +1, SCRIPTING_NULL);
	gClassIDToClass = m_ClassIDToMonoClass[0];
	
	m_ScriptingClassToClassID.clear();
	// Get the mono class from the classID by looking up the mono class by name
	// if a mono class can't be found check the super classes recursively
	ScriptingClassPtr baseObject = GetScriptingTypeRegistry().GetType("UnityEngine","Object");
	for (int i=0;i<allEngineClasses.size ();i++)
	{
		int classID = allEngineClasses[i];
		
		ScriptingClassPtr klass = FindScriptingClassForClassIDRecursive (classID,baseObject);
		AssertIf(klass == SCRIPTING_NULL);
		m_ClassIDToMonoClass[classID] = klass;

		//search again, but without searching basetypes. we need to do this because some enginetypes have no managed wrapper. (EllipsoidParticleEmitter),
		//so they can return the baseclass type.  we want that for m_ClassIDToMonoClass  but we do not want that for m_ScriptingClassToClassID.

		klass = FindScriptingClassForClassID (classID,baseObject);
		if (klass)
			m_ScriptingClassToClassID[klass] = classID;
	}
#endif
}

ScriptingClassPtr ScriptingManager::ClassIDToScriptingClass (int classID)
{
	AssertIf (classID == -1);
	AssertIf (m_ClassIDToMonoClass.size () <= classID);
	return m_ClassIDToMonoClass[classID];
}

int ScriptingManager::ClassIDForScriptingClass (ScriptingClassPtr klass)
{
	ScriptingClassMap::iterator iterator = m_ScriptingClassToClassID.find(klass);
	if (iterator == m_ScriptingClassToClassID.end())
		return -1;

	return iterator->second;
}

ScriptingObjectPtr ScriptingManager::CreateInstance(ScriptingClassPtr klass)
{
	if (!klass)
		return SCRIPTING_NULL;

	return scripting_object_new (klass);
}

ScriptingManager& GetScriptingManager()
{
	return reinterpret_cast<ScriptingManager&> (GetManagerFromContext (ManagerContext::kMonoManager));
}
#endif
