#ifndef PLATFORMTHREADSPECIFICVALUE_H
#define PLATFORMTHREADSPECIFICVALUE_H

#if UNITY_DYNAMIC_TLS

#include <pthread.h>
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/Utility.h"

class PlatformThreadSpecificValue
{
public:
	PlatformThreadSpecificValue();
	~PlatformThreadSpecificValue();

	void* GetValue() const;
	void SetValue(void* value);

private:
	pthread_key_t m_TLSKey;
};

inline PlatformThreadSpecificValue::PlatformThreadSpecificValue()
{
	int rc = pthread_key_create(&m_TLSKey, NULL);
	DebugAssertIf(rc != 0);
	UNUSED(rc);
}

inline PlatformThreadSpecificValue::~PlatformThreadSpecificValue()
{
	int rc = pthread_key_delete(m_TLSKey);
	DebugAssertIf(rc != 0);
	UNUSED(rc);
}

inline void* PlatformThreadSpecificValue::GetValue() const
{
#if !UNITY_LINUX
	// 0 is a valid key on Linux and POSIX specifies keys as opaque objects,
	// so technically we have no business snopping in them anyway...
	DebugAssertIf(m_TLSKey == 0);
#endif
	return pthread_getspecific(m_TLSKey);
}

inline void PlatformThreadSpecificValue::SetValue(void* value)
{
#if !UNITY_LINUX
	// 0 is a valid key on Linux and POSIX specifies keys as opaque objects,
	// so technically we have no business snopping in them anyway...
	DebugAssertIf(m_TLSKey == 0);
#endif
	pthread_setspecific(m_TLSKey, value);
}

#else

	#error "POSIX doesn't define a static TLS path"

#endif // UNITY_DYNAMIC_TLS

#endif // PLATFORMTHREADSPECIFICVALUE_H
