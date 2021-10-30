#ifndef RENDERQUEUE_H
#define RENDERQUEUE_H

class Transform;
class Vector4f;
class Matrix4x4f;
class LightmapSettings;
namespace Unity { class Material; }


void SetupObjectMatrix (const Matrix4x4f& m, int transformType);

UInt32 GetCurrentRenderOptions ();
bool CheckShouldRenderPass (int pass, Unity::Material& material);

bool SetupObjectLightmaps (const LightmapSettings& lightmapper, UInt32 lightmapIndex, const Vector4f& lightmapST, bool setMatrix);


// does a dummy render with all shaders & their combinations, so that driver actually creates them
void WarmupAllShaders ();


#endif
