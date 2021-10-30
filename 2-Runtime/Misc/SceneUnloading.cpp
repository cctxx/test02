#include "UnityPrefix.h"
#include "SceneUnloading.h"
#include "SaveAndLoadHelper.h"
#include "GameObjectUtility.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/BaseClasses/ManagerContextLoading.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"


PROFILER_INFORMATION (gUnloadScene, "UnloadScene", kProfilerLoading);

void SharkBeginRemoteProfiling ();
void SharkEndRemoteProfiling ();

void UnloadGameScene ()
{
	ABSOLUTE_TIME begin = START_TIME;
	
//	SharkBeginRemoteProfiling ();
	PROFILER_AUTO(gUnloadScene, NULL)
	
	InstanceIDArray objects;
	
	CollectSceneGameObjects (objects);
	
	Object* o;
	// GameObjects first
	for (InstanceIDArray::iterator i=objects.begin ();i != objects.end ();++i)
	{
		o = Object::IDToPointer (*i);
		AssertIf (o && o->IsPersistent ());
		GameObject* go = dynamic_pptr_cast<GameObject*> (o);
		// Only Destroy root level GameObjects. The children will be destroyed
		// as part of them. That way, we ensure that the hierarchy is walked correctly,
		// and all objects in the hieararchy will be marked as deactivated when destruction happens.
		if (go != NULL && go->GetComponent(Transform).GetParent() == NULL)
			DestroyObjectHighLevel (o);
	}
	
	// normal objects whatever they might be after that
	for (InstanceIDArray::iterator i=objects.begin ();i != objects.end ();++i)
	{
		o = Object::IDToPointer (*i);
		AssertIf (o && o->IsPersistent ());
		DestroyObjectHighLevel (o);
	}
	
	objects.clear ();
	CollectLevelGameManagers (objects);
	
	// Gamemanagers & Scene last
	for (InstanceIDArray::iterator i=objects.begin ();i != objects.end ();++i)
	{
		o = Object::IDToPointer (*i);
		AssertIf (o && o->IsPersistent ());
		DestroyObjectHighLevel (o);
	}
	
	GlobalCallbacks::Get().didUnloadScene.Invoke();
	
	ValidateNoSceneObjectsAreLoaded ();

	printf_console("UnloadTime: %f ms\n", AbsoluteTimeToMilliseconds(ELAPSED_TIME(begin)));
	
//	SharkEndRemoteProfiling ();
}
