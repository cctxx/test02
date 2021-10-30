#pragma once

#include "Runtime/Serialize/SerializationMetaFlags.h"

bool DoesBuildTargetSupportBlobification (BuildTargetPlatform target, TransferInstructionFlags flags);
bool IsBuildTarget64BitBlob              (BuildTargetPlatform target);