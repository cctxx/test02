#pragma once
#include <set>
#include <map>

struct UnityGUID;

extern void VCAutoAddAssetPostprocess(const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved);
