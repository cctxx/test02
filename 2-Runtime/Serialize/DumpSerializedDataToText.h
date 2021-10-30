#pragma once

#ifndef UNITY_INCLUDE_SERIALIZATION_DUMP
#define UNITY_INCLUDE_SERIALIZATION_DUMP 0
#endif

// Debugging functions that dump the state of an object or typetree/bytearray to stdout
// Used in BinaryToTextFile by defining UNITY_INCLUDE_SERIALIZATION_DUMP
#if UNITY_EDITOR || UNITY_INCLUDE_SERIALIZATION_DUMP || DEBUGMODE
#include <iostream>
#include "Runtime/Utilities/dynamic_array.h"

class TypeTree;

enum DumpOutputMode
{
	kDumpNormal,
	kDumpClean,
};

void DumpSerializedDataToText (const TypeTree& typeTree, dynamic_array<UInt8>& data);
void RecursiveOutput (const TypeTree& type, const UInt8* data, int* offset, int tab, std::ostream& os, DumpOutputMode mode, int pathID, bool doSwap, int arrayIndex);
#endif

