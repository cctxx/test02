#pragma once

// Same workaround as in OSXPrefix.h.
#define UInt8 MacUInt8
#define SInt8 MacSInt8
#define UInt16 MacUInt16
#define SInt16 MacSInt16
#define UInt32 MacUInt32
#define SInt32 MacSInt32
#define UInt64 MacUInt64
#define SInt64 MacSInt64

// Work around annoying problem in the Quicktime headers where on non-OSX
// platforms TARGET_API_MAC_CARBON is defined in by CoreServices.h and
// ConditionalMacros.h.  Force-include the header here that unconditionally
// #defines the macro so the definition in CoreServices.h is ignored.
#if !UNITY_OSX
#include <Quicktime/ConditionalMacros.h>
#endif

#include <Quicktime/Quicktime.h>
#include <Quicktime/ImageCompression.h>
#include <Quicktime/QuickTimeComponents.h>
#include <Quicktime/QTML.h>

#undef UInt8
#undef SInt8
#undef UInt16
#undef SInt16
#undef UInt32
#undef SInt32
#undef UInt64
#undef SInt64