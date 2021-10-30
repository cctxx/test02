#ifndef CACHINGMANAGER_H
#define CACHINGMANAGER_H

#include "Runtime/Misc/AssetBundle.h"

class Cache;

#if ENABLE_CACHING

#include "PreloadManager.h"

class CachedUnityWebStream
{
	public:
	SInt32        m_Version;
	std::vector<std::string> m_Files;
};

class AsyncCachedUnityWebStream : public PreloadManagerOperation
{
	int				m_InstanceID;
	AssetBundle*    m_LoadingAssetBundle;
	std::string		m_Url;
	std::string		m_Error;
	int				m_Version;
	UInt32			m_RequestedCRC;
	bool			m_AssetBundleWithSameNameIsAlreadyLoaded;

	friend class CachingManager;

	void LoadCachedUnityWebStream();
public:

	AsyncCachedUnityWebStream ();
	~AsyncCachedUnityWebStream ();

	AssetBundle* GetAssetBundle () { return PPtr<AssetBundle> (m_InstanceID); }
	bool DidSucceed() { return IsDone() && m_InstanceID != 0;}
	bool AssetBundleWithSameNameIsAlreadyLoaded() { return m_AssetBundleWithSameNameIsAlreadyLoaded; }
	virtual void Perform ();

	virtual void IntegrateMainThread ();
	virtual bool HasIntegrateMainThread () { return true; }
	std::string &GetError() { return m_Error; }

#if ENABLE_PROFILER
	virtual std::string GetDebugName () { return "AsyncCachedUnityWebStream"; }
#endif
};

class Cache : public NonCopyable
{
public:
	bool m_IncludePlayerURL;
	string m_Name;
	time_t m_Expires;
	long long m_BytesUsed;
	long long m_BytesAvailable; // bytes allowed
	long long m_MaxLicenseBytesAvailable; // Bytes allowed by the license

	int GetExpirationDelay () { return m_CacheExpirationDelay; }
	void SetExpirationDelay (int expiration);

	void WriteCacheInfoFile (bool updateExpiration);
	string GetFolder (const string& path, bool create);

	void AddToCache (const string &path, int bytes);
	void UpdateTimestamp (const std::string &path, time_t lastAccessed);

	bool CleanCache ();

	bool FreeSpace (size_t bytes);

	void SetMaximumDiskSpaceAvailable (long long maxUsage);
	long long GetMaximumDiskSpaceAvailable () { return m_BytesAvailable; }

	long long GetCachingDiskSpaceUsed () { return m_BytesUsed; }
	long long GetCachingDiskSpaceFree () { return m_BytesAvailable - m_BytesUsed; }
	bool GetIsReady () { return m_Ready; }

	bool ReadCacheIndex (const string& name, bool getSize);
	string URLToPath (const string& url, int version);

	Cache();
	~Cache();

private:
	struct CachedFile
	{
		string path;
		size_t size;
		int version;
		time_t lastAccessed;

		CachedFile()
		{
			size = version = lastAccessed = 0;
		}
		CachedFile(string _path, size_t _size, int _version, time_t _lastAccessed)
		{
			path = _path;
			size = _size;
			version = _version;
			lastAccessed = _lastAccessed;
		}
		bool operator < (const CachedFile& other) const
		{
			return lastAccessed < other.lastAccessed;
		}
	};

	typedef std::multiset<CachedFile> CachedFiles;

	int         m_CacheExpirationDelay;
	Thread      m_IndexThread;
	Mutex       m_Mutex;
	bool        m_Ready;
	CachedFiles m_CachedFiles;

	bool ReadCacheInfoFile (const string& path, time_t *expires, time_t *oldestLastUsedTime);
	static void *ReadCacheIndexThreaded (void *data);
};

class CachingManager
{
public:
	enum AuthorizationLevel
	{
		kAuthorizationNone = 0,
		kAuthorizationUser,
		kAuthorizationAdmin
	};
	enum
	{
		kMaxCacheExpiration = (150 * 86400) //150 days
	};

	CachingManager ();
	~CachingManager ();
	void Reset ();

	std::string GetTempFolder ();
	static bool MoveTempFolder (const string& from, const string& to);
	static time_t GenerateTimeStamp ();
	static int WriteInfoFile (const std::string &path, const std::vector<std::string> &fileNames);
	static int WriteInfoFile (const std::string &path, const std::vector<std::string> &fileNames, time_t lastAccessed);
	static bool ReadInfoFile (const std::string &path, time_t *lastUsedTime, std::vector<std::string> *fileNames);

	std::vector<Cache*> &GetCacheIndices ();

	bool CleanCache (std::string name);

	bool Authorize (const string &name, const string &domain, long long size, int expiration, const string &signature);
	AuthorizationLevel GetAuthorizationLevel ();

	AsyncCachedUnityWebStream* LoadCached (const std::string& url, int version, UInt32 crc);
	bool IsCached (const std::string &url, int version);
	bool MarkAsUsed (const std::string& url, int version);

	bool GetEnabled () { return m_Enabled; }
	void SetEnabled (bool enabled);


	Cache& GetCurrentCache();

	long long GetCachingDiskSpaceUsed ();
	long long GetCachingDiskSpaceFree ();

	void SetMaximumDiskSpaceAvailable (long long maximumAvailable);
	long long GetMaximumDiskSpaceAvailable ();

	int GetExpirationDelay ();
	bool GetIsReady ();

#if UNITY_IPHONE
	void SetNoBackupFlag(const std::string &url, int version);
	void ResetNoBackupFlag(const std::string &url, int version);
#endif

private:
	AuthorizationLevel m_Authorization;
	Cache* m_Cache;
	bool m_Enabled;

	void Dispose ();
	void ClearTempFolder();
	bool VerifyValidDomain (const std::string &domain);
	void SetCurrentCache (const string &name, long long size, bool includePlayerURL);
	void ReadCacheIndices (std::vector<Cache*> &indices, bool getSize);
};

/// Handles the cache for multiple games
class GlobalCachingManager
{
	std::vector<Cache*>* m_CacheIndices;
	void ReadCacheIndices (std::vector<Cache*> &indices, bool getSize);

public:
	GlobalCachingManager ()
	{
		m_CacheIndices = NULL;
	}

	std::vector<Cache*>& GetCacheIndices ();
	void ClearAllExpiredCaches ();
	void RebuildAllCaches ();
	void Dispose ();
};


CachingManager &GetCachingManager();
GlobalCachingManager &GetGlobalCachingManager();

#endif // ENABLE_CACHING
#endif
