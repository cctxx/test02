#ifndef CACHE_SERVER_COMMUNICATION_H
#define CACHE_SERVER_COMMUNICATION_H


///@TODO: This dependency is stupid. Get rid of it. CacheServerData should be in a seperate header.
#include "CacheServer.h"

struct UnityGUID;
struct MdFour;

enum CacheServerError { kCacheServerSuccess = 0, kCacheServerNotCached = 1, kCacheServerConnectionFailure = 2, kCacheServerInvalidDataFileFailure = 3 };

CacheServerError ConnectToCacheServer();
void DisconnectFromCacheServer();
bool IsConnectedToCacheServer ();

CacheServerError RequestFromCacheServer (const UnityGUID& guid, const MdFour& hash);
CacheServerError DownloadFromCacheServer (const UnityGUID& guid, const MdFour& hash, CacheServerData& output);
CacheServerError UploadToCacheServer (const UnityGUID& guid, const MdFour& hash, const CacheServerData& data);

extern const char* kCacheServerTempFolder;

bool CanConnectToCacheServer();

#endif