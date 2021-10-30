#ifndef CLIPBOUNDSCALCULATOR_H
#define CLIPBOUNDSCALCULATOR_H

#include <set>

namespace Unity
{
class GameObject;
}

void CalculateClipsBoundsDeprecated(Unity::GameObject& gameObject);
void CalculateClipsBoundsDeprecated(Unity::GameObject& gameObject, const std::set<std::string>& animationClipNames);

void PrecalculateSkinnedMeshRendererBoundingVolumes (Unity::GameObject& gameObject);

#endif
