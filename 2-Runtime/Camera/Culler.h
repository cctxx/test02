#ifndef CULLER_H
#define CULLER_H

#include "CullingParameters.h"

struct CullResults;
struct SceneCullingParameters;
class IntermediateRenderers;
namespace Unity { class GameObject; }

void CullScene                     (SceneCullingParameters& cullingParameters, CullResults& cullResults);
void CullIntermediateRenderersOnly (const SceneCullingParameters& cullingParameters, CullResults& results);

bool IsGameObjectFiltered (Unity::GameObject& go, CullFiltering cullFilterMode);

#endif
