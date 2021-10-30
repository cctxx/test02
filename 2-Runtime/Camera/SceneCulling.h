#pragma once

#include "Runtime/Utilities/dynamic_array.h"

struct CullingOutput;
struct SceneCullingParameters;
class IntermediateRenderer;
struct SceneNode;
class AABB;
class Sphere;
class IntermediateRenderers;

void CullSceneWithUmbra (SceneCullingParameters& cullingParams, CullingOutput& output);
void CullSceneWithoutUmbra (const SceneCullingParameters& cullingParams, CullingOutput& output);

bool IsNodeVisible (const SceneNode& node, const AABB& aabb, const SceneCullingParameters& params);

void CullShadowCastersWithUmbra (const SceneCullingParameters& cullingParams, CullingOutput& output);