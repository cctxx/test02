#pragma once

// Disable these features on xbox for now
#if 0
#	define VOID_IMPL ;
#	define RET_IMPL(value) ;
#else
#	define VOID_IMPL {}
#	define RET_IMPL(value) { return value; }
#endif
