#pragma once

struct GlobalLayeringData
{
	// Per-renderer sorting data.
	SInt16 layer; // Layer order.
	SInt16 order; // In-layer order.
};

inline GlobalLayeringData GlobalLayeringDataCleared () { GlobalLayeringData data = {0,0}; return data;  }

inline bool CompareGlobalLayeringData(const GlobalLayeringData& lhs, const GlobalLayeringData& rhs, bool& result)
{
	if (lhs.layer != rhs.layer)
	{
		result = lhs.layer < rhs.layer;
		return true;
	}
	else if (lhs.order != rhs.order)
	{
		result = lhs.order < rhs.order;
		return true;
	}

	return false;
}
