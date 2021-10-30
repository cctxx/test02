#ifndef GO_CREATION_CPP_H
#define GO_CREATION_CPP_H

#include "Runtime/BaseClasses/GameObject.h"

enum
{
	kPrimitiveSphere = 0,
	kPrimitiveCapsule = 1,
	kPrimitiveCylinder = 2,
	kPrimitiveCube = 3,
	kPrimitivePlane = 4,
	kPrimitiveQuad = 5
};

Unity::GameObject* CreatePrimitive (int type);

#endif
