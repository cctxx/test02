#include "AssetHashing.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Threads/ThreadedStreamBuffer.h"
#include "Runtime/Profiler/TimeHelper.h"

#if !UNITY_WIN
#include <aio.h>
#if UNITY_LINUX
#include <errno.h>
#endif
#endif

bool DoesAssetImporterHavePerPlatformFormat (int importerClassID)
{
	// Texture importer has platform overrides
	if (importerClassID == 1006)
		return true;
	
	// Audio importer has platform overrides
	if (importerClassID == 1020)
		return true;
	
	// TrueType Font importer has platform overrides
	if (importerClassID == 1042)
		return true;

	return false;
}

bool DoesAssetImporterHaveSubTargetPlatformFormat (int importerClassID)
{
	// Texture importer has platform overrides
	if (importerClassID == 1006)
		return true;

	return false;
}

#define kHashReadBufferSize (64 * 1024)
#define kHashReadBufferNumEntries (8)

bool Provider(void* dest, ThreadedStreamBuffer::size_t bufferSize, ThreadedStreamBuffer::size_t& bytesWritten, void* userData)
{
	File* fd = static_cast<File*>(userData);
	bytesWritten = fd->Read (((char*)dest), bufferSize);
	return bytesWritten == bufferSize;
}

void BufferFile (ThreadedStreamBuffer &buffer, const string &path)
{
	// todo handle read errors
	// try async read
	File fd;
	fd.Open (path, File::kReadPermission);

	buffer.WriteValueType(fd.GetFileLength());
	buffer.WriteStreamingData(Provider, &fd, 4, kHashReadBufferSize);

	fd.Close ();
}


void Consumer(const void* buffer, ThreadedStreamBuffer::size_t bufferSize, void* userData)
{
	MdFourGenerator* gen = static_cast<MdFourGenerator*>(userData);

	gen->Feed(static_cast<const char*>(buffer), static_cast<int>(bufferSize));
}


void FeedHashFromBuffer (ThreadedStreamBuffer &buffer, UInt64* totalProcessedBytes, const string &path, MdFourGenerator &gen)
{
	int fileLength = buffer.ReadValueType<int>();
	size_t processed = buffer.ReadStreamingData(Consumer, &gen, 4, kHashReadBufferSize); 
	Assert(fileLength == processed);

	*totalProcessedBytes += processed;
};

static MdFour GenerateHashForAsset (ThreadedStreamBuffer *buffer, UInt64* processedBytes, const std::string& assetPath, int importerClassID, const UInt32& importVersionHash, BuildTargetSelection target)
{
	MdFourGenerator gen;
	
	// Metafile contains reference to asset path thus a new cache must be generated when moving an asset
	gen.Feed(assetPath);
	gen.Feed(importVersionHash);
	
	if (buffer)
	{
		FeedHashFromBuffer (*buffer, processedBytes, assetPath, gen);
		FeedHashFromBuffer (*buffer, processedBytes, GetTextMetaDataPathFromAssetPath(assetPath), gen);
	}
	else
	{
		// These are sources to the import
		gen.FeedFromFile(assetPath);
		gen.FeedFromFile(GetTextMetaDataPathFromAssetPath(assetPath));
	}
	
	// Generic version number to push something out of the cache
	gen.Feed((int)kCacheServerDataLayoutVersion);
	
	// Some AssetImporters generated different output depending on the target platform
	if (DoesAssetImporterHavePerPlatformFormat (importerClassID))
	{
		gen.Feed((int)target.platform);
	}
	
	// Texture importer has sub-platform overrides
	if (DoesAssetImporterHaveSubTargetPlatformFormat(importerClassID))
	{
		gen.Feed(target.subTarget);
	}
	
	return gen.Finish();
}

MdFour GenerateHashForAsset (const std::string& assetPath, int importerClassID, const UInt32& importerVersionHash, BuildTargetSelection target)
{
	return GenerateHashForAsset (NULL, NULL, assetPath, importerClassID, importerVersionHash, target);
}

struct PrefetchData
{
	ThreadedStreamBuffer* buffer;
	std::vector<HashAssetRequest>* requests;
	Thread* thread;
};

void* PrefetchHashRequests (void *data)
{
	PrefetchData *prefetchData = (PrefetchData*)data;
	for (int i=0; i<prefetchData->requests->size(); i++)
	{
		HashAssetRequest& request = (*(prefetchData->requests))[i];
		BufferFile (*prefetchData->buffer, request.assetPath);
		BufferFile (*prefetchData->buffer, GetTextMetaDataPathFromAssetPath(request.assetPath));
	}
	return NULL;
}


UInt64 ProcessHashRequests (std::vector<HashAssetRequest> &requests, BuildTargetSelection platform)
{
	ThreadedStreamBuffer* buffer = new ThreadedStreamBuffer(ThreadedStreamBuffer::kModeThreaded, kHashReadBufferSize * kHashReadBufferNumEntries);
	Thread prefetchThread;
	PrefetchData prefetchData;
	prefetchData.buffer = buffer;
	prefetchData.requests = &requests;
	prefetchThread.Run (PrefetchHashRequests, &prefetchData);

	ABSOLUTE_TIME hashingStart = START_TIME;
	printf_console("Hashing assets ... ");

	UInt64 processed = 0;
	for (int i=0;i<requests.size();i++)
	{
		HashAssetRequest& request = requests[i];
				
		request.hash = GenerateHashForAsset (buffer, &processed, request.assetPath, request.importerClassID, request.importerVersionHash, platform);
	}

	prefetchThread.WaitForExit();
	delete buffer;
	printf_console("%f seconds\n", GetElapsedTimeInSeconds(hashingStart));
	
	return processed;
}

