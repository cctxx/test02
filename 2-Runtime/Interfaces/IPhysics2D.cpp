#include "UnityPrefix.h"
#include "IPhysics2D.h"


// --------------------------------------------------------------------------


static IPhysics2D* gIPhysics2D = NULL;


// --------------------------------------------------------------------------


IPhysics2D* GetIPhysics2D()
{
	return gIPhysics2D;
}


void SetIPhysics2D(IPhysics2D* value)
{
	gIPhysics2D = value;
}
