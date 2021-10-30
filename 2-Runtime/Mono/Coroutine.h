#ifndef _COROUTINE_H_
#define _COROUTINE_H_
#if ENABLE_SCRIPTING
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Utilities/LinkedList.h"

class AsyncOperation;
class MonoBehaviour;
class Object;


struct Coroutine : public ListElement
{
	ScriptingObjectPtr    m_CoroutineEnumerator;
	int            m_CoroutineEnumeratorGCHandle;
	ScriptingMethodPtr    m_CoroutineMethod;
	ScriptingMethodPtr    m_MoveNext;
	ScriptingMethodPtr    m_Current;
	MonoBehaviour* m_Behaviour;
	int            m_RefCount;
	int            m_IsReferencedByMono;
	bool           m_DoneRunning;
	Coroutine*     m_ContinueWhenFinished;
	Coroutine*     m_WaitingFor;
	AsyncOperation* m_AsyncOperation;
	Coroutine ();
	~Coroutine();
	
	void Run ();
	
	void SetMoveNextMethod(ScriptingMethodPtr method);
	void SetCurrentMethod(ScriptingMethodPtr method);
	static void ContinueCoroutine (Object* o, void* userData);
	static void CleanupCoroutine (void* userData);
	static void CleanupCoroutineGC (void* userData);
	static bool CompareCoroutineMethodName (void* callBackUserData, void* cancelUserdata);

private:
	bool InvokeMoveNext(ScriptingExceptionPtr* exception);
	void ProcessCoroutineCurrent();
	void HandleIEnumerableCurrentReturnValue(ScriptingObjectPtr);
};
#endif //ENABLE_SCRIPTING
#endif
