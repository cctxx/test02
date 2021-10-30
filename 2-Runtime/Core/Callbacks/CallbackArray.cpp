#include "UnityPrefix.h"
#include "CallbackArray.h"

CallbackArray::CallbackArray ()
{
	for (int i=0;i<kMaxCallback;i++)
		m_Callbacks[i] = NULL;
}

void CallbackArray::Register (SimpleCallback* callback)
{
	for (int i=0;i<kMaxCallback;i++)
	{
		if (m_Callbacks[i] == NULL)
		{
			m_Callbacks[i] = callback;
			return;
		}
	}
	
	AssertString("Callback registration failed. Not enough space.");
}

void CallbackArray::Unregister (SimpleCallback* callback)
{
	for (int i=0;i<kMaxCallback;i++)
	{
		if (m_Callbacks[i] == callback)
			m_Callbacks[i] = NULL;
	}
}

void CallbackArray::Invoke ()
{
	for (int i=0;i<kMaxCallback;i++)
	{
		if (m_Callbacks[i])
			m_Callbacks[i] ();
	}
}