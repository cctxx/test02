#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_NETWORK

class BitstreamPacker;
class Rigidbody;
class MonoBehaviour;
class Transform;
class Animation;
struct NetworkMessageInfo;

bool SerializeRigidbody (Rigidbody& rigidbody, BitstreamPacker& packer);

void UnpackTransform (Transform& transform, BitstreamPacker& packer);
bool PackTransform (Transform& transform, BitstreamPacker& packer);

bool SerializeAnimation (Animation& animation, BitstreamPacker& packer);

bool SerializeMono (MonoBehaviour& mono, BitstreamPacker& deltaState, NetworkMessageInfo& timeStamp);

#endif