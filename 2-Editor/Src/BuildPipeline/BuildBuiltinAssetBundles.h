#pragma once

#include "Runtime/Serialize/SerializationMetaFlags.h"

bool BuildBuiltinAssetBundles ();
bool BuildBuiltinOldWebAssetBundles ();
bool BuildEditorAssetBundles ();
bool GenerateBuiltinAssetPreviews ();
void BuildExtraResourcesBundleForPlayer (BuildTargetPlatform platform, const std::string& outputDirectory);
