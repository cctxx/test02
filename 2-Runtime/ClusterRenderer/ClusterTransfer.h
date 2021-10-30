#pragma once
#if ENABLE_CLUSTER_SYNC
#include "Runtime/Utilities/dynamic_array.h"

class ClusterTransfer
{
public:
	void TransferToBuffer(dynamic_array<UInt8>& buffer);
	void TransferFromBuffer(dynamic_array<UInt8>& buffer);
#ifdef DEBUG
	void TransferToFile(int slaveId);
#endif
};


#endif