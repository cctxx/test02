#ifndef CLONEOBJECT_H
#define CLONEOBJECT_H

#include <vector>
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Allocator/MemoryMacros.h"

class Object;
class Vector3f;
class Quaternionf;

typedef std::pair<SInt32, SInt32> IntPair;
typedef vector_map<SInt32, SInt32, std::less<SInt32>, STL_ALLOCATOR(kMemTempAlloc, IntPair) > TempRemapTable;

/// Clones a vector objects. Cloning will clone all islands of connected objects through pptrs.
/// This will not properly clone editorextensionimpl data and should only be used in gamemode
/// After all objects are loaded, all gameobjects will be Activated and if the original 
/// was in an animation the clone it will be added to it
Object& CloneObject (Object& objects);

/// The same as above, but set position and rotation of the first objects' transform component. 
/// It also performs a delayed activation of all instantiated objects.
Object& InstantiateObject (Object& objects, const Vector3f& worldPos, const Quaternionf& worldRot);

/// Use these in succession if you need to setup some data prior to activation/awake
Object& InstantiateObject (Object& inObject, const Vector3f& worldPos, const Quaternionf& worldRot, TempRemapTable& ptrs);
void AwakeAndActivateClonedObjects (const TempRemapTable& ptrs);

#endif
