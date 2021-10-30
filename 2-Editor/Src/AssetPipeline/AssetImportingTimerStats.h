#ifndef ASSETIMPORTINGTIMING_H
#define ASSETIMPORTINGTIMING_H

#include "Runtime/Profiler/TimeHelper.h"

struct AssetImportingTimerStats
{
	UInt64 cacheServerDownloadedBytes;
	UInt64 cacheServerHashedBytes;
	UInt32 cacheServerRequestedAssets;
	UInt32 cacheServerFoundAssets;
	UInt32 cacheServerUnavailableAssets;
	UInt32 cacheServerNotSupportedAssets;
	
	ABSOLUTE_TIME completeAssetImportTime;
	ABSOLUTE_TIME assetImporting;
	ABSOLUTE_TIME cacheServerIntegrateTime;
	ABSOLUTE_TIME cacheServerMoveFilesTime;
	ABSOLUTE_TIME cacheServerDeleteFilesTime;
	ABSOLUTE_TIME cacheServerHashing;
	ABSOLUTE_TIME cacheServerDownloading;
};

#endif
