#pragma once

#include "Editor/Src/Utility/BuildPlayerUtility.h"

// Build a player right after the next time scripts have been reloaded.  This is useful in situations
// where the currently active platform and the desired build platform are incompatible such that serializing
// data using the currently loaded scripts will produce incorrect data for the target platform.  In that case,
// do a platform switch manually first and then call this function to have the build be done automatically
// once the switch is complete.
void BuildPlayerAfterNextScriptReload (BuildPlayerSetup& setup);

void ShowBuildPlayerWindow();
void BuildPlayerWithLastSettings();
