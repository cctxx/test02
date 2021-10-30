#pragma once

#include "Runtime/Camera/CullResults.h"
#include "RenderLoopPrivate.h"
#include <string>

class Shader;

void RenderSceneShaderReplacement (const VisibleNodes& contents, const ShaderReplaceData& shaderReplace);
void RenderSceneShaderReplacement (const VisibleNodes& contents, Shader* shader, const std::string& shaderReplaceTag);
void RenderSceneShaderReplacement (const RenderObjectDataContainer& contents, Shader* shader, const std::string& shaderReplaceTag);
