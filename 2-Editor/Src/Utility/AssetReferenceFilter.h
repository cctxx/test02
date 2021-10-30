#ifndef ASSET_REFERENCE_FILTER_H_
#define ASSET_REFERENCE_FILTER_H_

struct UnityGUID;

bool IsReferencing (int parentInstanceId, int childInstanceId);
bool IsReferencingAsset (UnityGUID const& parent, UnityGUID const& child);

#endif