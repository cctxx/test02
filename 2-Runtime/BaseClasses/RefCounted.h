#pragma once

#include "Configuration/UnityConfigure.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"

class TrackedReferenceBase
{
public:
	int m_MonoObjectReference;
	
	TrackedReferenceBase ()
	{
		m_MonoObjectReference = 0;
	}
	
	~TrackedReferenceBase ()
	{
#if ENABLE_SCRIPTING
		if (m_MonoObjectReference)
		{
			ScriptingObjectPtr target = scripting_gchandle_get_target (m_MonoObjectReference);
			if (target)
			{
				void* nativePointer = 0;
				MarshallNativeStructIntoManaged(nativePointer,target);
				target = SCRIPTING_NULL;
			}
			
			scripting_gchandle_free (m_MonoObjectReference);
			m_MonoObjectReference = 0;
		}
#endif
	}
};
