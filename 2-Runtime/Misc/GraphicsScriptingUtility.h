#ifndef GRAPHICSSCRIPTINGUTILITY_H_
#define GRAPHICSSCRIPTINGUTILITY_H_

#include "UnityPrefix.h"

#include "External/shaderlab/Library/FastPropertyName.h"

struct ICallString;

ShaderLab::FastPropertyName ScriptingStringToProperty(ICallString& msname);

#endif