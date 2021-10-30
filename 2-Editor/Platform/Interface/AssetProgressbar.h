#pragma once

#include <string>

void UpdateAssetProgressbar (float value, std::string const& title, std::string const& text, bool canCancel = false);
void ClearAssetProgressbar ();
bool IsAssetProgressBarCancelPressed ();
