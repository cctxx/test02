#include "UnityPrefix.h"
#include "AsyncOperation.h"

void AsyncOperation::SetCoroutineCallback (	DelayedCall* func, Object* coroutineBehaviour, void* userData, CleanupUserData* cleanup)
{
	m_CoroutineBehaviour = coroutineBehaviour;
	m_CoroutineDone = func;
	m_CoroutineCleanup = cleanup;
	m_CoroutineData = userData;
}

void AsyncOperation::InvokeCoroutine ()
{
	if (m_CoroutineDone != NULL)
	{
		Object* target = m_CoroutineBehaviour;
		if (target)
			m_CoroutineDone(target, m_CoroutineData);
		m_CoroutineCleanup(m_CoroutineData);
		m_CoroutineDone = NULL;
	}
}

void AsyncOperation::CleanupCoroutine ()
{
	if (m_CoroutineDone != NULL)
	{
		m_CoroutineCleanup(m_CoroutineData);
		m_CoroutineDone = NULL;
	}
}

AsyncOperation::~AsyncOperation ()
{
	AssertIf(m_CoroutineDone != NULL);
}

void AsyncOperation::Retain ()
{
	m_RefCount.Retain();
}

void AsyncOperation::Release ()
{
	if (m_RefCount.Release())
		delete this;
}
