#ifndef DESTROYDELAYED_H
#define DESTROYDELAYED_H

class Object;

/// Destroys object in time seconds
/// If time is not specified will be destroyed at the end of the frame
void DestroyObjectDelayed (Object* o, float time = -100.0F);

#endif
