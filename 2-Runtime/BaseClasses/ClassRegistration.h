#pragma once
#include "ClassIDs.h"

struct ClassRegistrationContext
{
	void* explicitlyRegistered;
};

#if DEBUGMODE
#define REGISTER_CLASS(x) \
{ \
	extern void RegisterClass_##x(); \
	RegisterClass_##x(); \
	ValidateRegisteredClassID(context, ClassID(x), #x); \
}
#else
#define REGISTER_CLASS(x) \
{ extern void RegisterClass_##x(); RegisterClass_##x(); }
#endif


EXPORT_COREMODULE void ValidateRegisteredClassID (ClassRegistrationContext& context, int classID, const char* className);
