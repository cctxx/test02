#include "Runtime/Math/Color.h"

#ifndef GIZMODRAWERS_H
#define GIZMODRAWERS_H

class Object;


void DrawLightGizmo (Object& o, int options, void* userData);
void DrawAudioReverbZoneGizmo (Object& o, int options, void* userData);
void DrawAudioSourceGizmo (Object& o, int options, void* userData);
bool HasPickGizmo (Object& o, int options, void* userData);
void DrawPickGizmo (Object& o, int options, void* userData);
void DrawMonoGizmo (Object& o, int options, void* userData);
bool CanDrawMonoGizmoSelected (Object& o, int options, void* userData);
void DrawMonoGizmoSelected (Object& o, int options, void* userData);
bool CanDrawMonoGizmo (Object& o, int options, void* userData);
#if ENABLE_2D_PHYSICS
void Draw2DColliderGizmo (Object& o, int options, void* userData);
#endif
void DrawAnimationGizmo (Object& o, int options, void* userData);
void DrawSkinnedMeshRendererGizmo (Object& o, int options, void* userData);
void DrawCameraGizmo (Object& o, int options, void* userData);
void DrawProjectorGizmo (Object& o, int options, void* userData);
void DrawControllerGizmo (Object& o, int options, void* userData);
void DrawWindGizmo (Object& o, int options, void* userData);
void DrawOcclusionAreaGizmo (Object& o, int options, void* userData);
void DrawOcclusionAreaGizmoSelected (Object& o, int options, void* userData);
void DrawNavMeshAgentGizmo (Object& o, int options, void* userData);
void DrawNavMeshObstacleGizmo (Object& o, int options, void* userData);
bool CanDrawMonoScriptIcon (Object& o, int options, void* userData);
void DrawMonoScriptIcon (Object& o, int options, void* userData);
bool CanDrawGameObjectIcon (Object& o, int options, void* userData);
void DrawGameObjectIcon (Object& o, int options, void* userData);
void DrawOcclusionPortal (Object& object, int options, void* userData);
void DrawDebugRendererBoundsGizmos (Object& o, int options, void* userData);
void DrawDebugRendererBonesBoundsGizmos (Object& o, int options, void* userData);
bool CanDrawParticleSystemIcon (Object& object, int options, void* userData);
void DrawParticleSystemIcon (Object& object, int options, void* userData);

#endif
