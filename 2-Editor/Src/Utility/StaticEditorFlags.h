#ifndef STATICEDITORFLAGS_H
#define STATICEDITORFLAGS_H

#include "Runtime/Utilities/Utility.h"

//Needs to be kept up do date with flags in StaticEditorFlags.txt
enum StaticEditorFlags
{	
	kLightmapStatic        = 1 << 0,
	kOccluderStatic        = 1 << 1,
	kBatchingStatic        = 1 << 2,
	kNavigationStatic      = 1 << 3,
	kOccludeeStatic        = 1 << 4,
	kOffMeshLinkGeneration = 1 << 5
};
ENUM_FLAGS(StaticEditorFlags);
#endif
