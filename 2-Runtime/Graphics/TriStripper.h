#ifndef TRISTRIPPER_H
#define TRISTRIPPER_H

#include <vector>
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Modules/ExportModules.h"

bool EXPORT_COREMODULE Stripify (const UInt32* faces, int count, UNITY_TEMP_VECTOR(UInt32)& strip);

/// Destrips a triangle strip into a face list.
/// Adds the triangles from the strip into the trilist
void EXPORT_COREMODULE Destripify (const UInt32* strip, int length, UNITY_TEMP_VECTOR(UInt32)& trilist);
void EXPORT_COREMODULE Destripify (const UInt16* strip, int length, UNITY_TEMP_VECTOR(UInt32)& trilist);
void EXPORT_COREMODULE Destripify (const UInt16* strip, int length, UNITY_TEMP_VECTOR(UInt16)& trilist);

template<typename Tin, typename Tout>
void EXPORT_COREMODULE Destripify(const Tin* strip, int length, Tout* trilist, int capacity);

template<typename T>
int EXPORT_COREMODULE CountTrianglesInStrip (const T* strip, int length);

#endif
