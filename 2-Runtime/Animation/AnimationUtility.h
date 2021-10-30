#pragma once

#include "Runtime/mecanim/types.h"

class AnimationClip;
class Transform;
namespace Unity { class GameObject; }
namespace mecanim { class crc32 ; namespace skeleton { struct Skeleton; } }  


void SampleAnimation (Unity::GameObject& go, AnimationClip& clip, float inTime, int wrapMode);


mecanim::crc32 AppendPathToHash(const mecanim::crc32& nameHash, const char* path);
Transform* FindChildWithID( Transform* transform,  const mecanim::crc32& nameHash, mecanim::uint32_t id);

Transform* FindAvatarRoot(const mecanim::skeleton::Skeleton* skeleton, const mecanim::uint32_t* nameArray, Transform& root, bool hasTransformHierarchy);
