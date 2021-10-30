#pragma once
#include <string>
#include "Editor/Src/VersionControl/VCAsset.h"
#include "Editor/Src/VersionControl/VCChangeSet.h"
#include "Editor/Src/VersionControl/VCMessage.h"

class VCPluginSession;

bool DownloadRevisions(VCPluginSession& p, VCMessages& msgs, VCAssetList& result, const VCAssetList& assets, const VCChangeSetIDs& changeSetIDs, const std::string& targetDir);
