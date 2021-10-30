#pragma once

// Figures out all lights that are visible in the scene
void FindAndCullActiveLights (const SceneCullingParameters& sceneCullParameters, const ShadowCullData& cullData, ActiveLights& outLights);

// Figures out lights for given object, no sorting.
void CullPerObjectLights (const ActiveLights& activeLights, const AABB& globalObjectAABB, const AABB& localObjectAABB, const Matrix4x4f& objectTransform, float invScale, int layerMask, bool lightmappedObject, bool dualLightmapsMode, ObjectLightIndices& outIndices);