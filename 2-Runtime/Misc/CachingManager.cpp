#include "UnityPrefix.h"
#include "CachingManager.h"

#if ENABLE_CACHING

#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "PlatformDependent/CommonWebPlugin/Verification.h"
#include "PlayerSettings.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/File/ApplicationSpecificPersistentDataPath.h"
#include "Runtime/Threads/ThreadUtility.h"

#include <time.h>

static const char *kInfoFileName = "__info";

#if WEBPLUG && !UNITY_PEPPER
#include "Runtime/Utilities/GlobalPreferences.h"
static const char *kCachingEnabledKey = "CachingEnabled";
#endif

#if UNITY_PS3
#include <cell/cell_fs.h>
extern "C" const char* GetBasePath();
#endif

#if UNITY_XENON
#include "XenonPaths.h"
#endif

#if UNITY_OSX || UNITY_LINUX
#include <sys/stat.h>
#elif UNITY_IPHONE
#include "PlatformDependent/iPhonePlayer/iPhoneMisc.h"
namespace iphone {
	std::string GetUserDirectory();
}
#elif UNITY_WINRT
#include "PlatformDependent/MetroPlayer/MetroUtils.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#elif UNITY_WIN && !UNITY_WINRT
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "Shlobj.h"

#define kDefaultPathBufferSize MAX_PATH*4

#define UNITY_DEFINE_KNOWN_FOLDER(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
UNITY_DEFINE_KNOWN_FOLDER(FOLDERID_LocalAppDataLow,     0xA520A1A4, 0x1780, 0x4FF6, 0xBD, 0x18, 0x16, 0x73, 0x43, 0xC5, 0xAF, 0x16);

static std::string gWinCacheBasePath;

string GetWindowsCacheBasePath()
{
	if (gWinCacheBasePath.empty())
	{
		wchar_t *widePath = NULL;

		HRESULT gotPath;

		typedef HRESULT WINAPI SHGetKnownFolderPathFn(REFGUID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPath);
		HMODULE hShell32 = LoadLibrary(TEXT("shell32.dll"));
		if(hShell32)
		{
			SHGetKnownFolderPathFn* pfnSHGetKnownFolderPath = reinterpret_cast<SHGetKnownFolderPathFn*>(GetProcAddress(hShell32, "SHGetKnownFolderPath"));
			if(pfnSHGetKnownFolderPath)
			{
				gotPath = pfnSHGetKnownFolderPath(FOLDERID_LocalAppDataLow, NULL, NULL, &widePath);
			}
			else
			{
				widePath = (wchar_t*)CoTaskMemAlloc(MAX_PATH);
				gotPath = SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, widePath);
			}
			if (!FreeLibrary(hShell32))
				printf_console("Error while freeing shell32 library");;
		}

		if (SUCCEEDED(gotPath))
		{
			ConvertWindowsPathName( widePath, gWinCacheBasePath);
		}

		if (widePath)
			CoTaskMemFree(widePath);
	}
	else
	{
	}
	return gWinCacheBasePath;
}

void CreateDirectoryHelper( const std::string& path )
{
	std::wstring widePath;
	ConvertUnityPathName( path, widePath );
	CreateDirectoryW( widePath.c_str(), NULL );
}
#endif

int GetFolderSizeRecursive (const string& path)
{
	if (!IsDirectoryCreated (path))
		return 0;

	set<string> paths;
	if (!GetFolderContentsAtPath (path, paths))
		return 0;

	int size = 0;
	for (set<string>::iterator i = paths.begin(); i != paths.end(); i++)
	{
		if (IsDirectoryCreated(*i))
			size += GetFolderSizeRecursive(*i);
		else
			size += GetFileLength (*i);
	}

	return size;
}

string Cache::URLToPath (const string& url, int version)
{
	string source = GetLastPathNameComponent(url);

	// strip url parameters
	source = source.substr(0, source.find("?"));

	if (m_IncludePlayerURL)
		source += GetPlayerSettings().absoluteURL;

	if (version != 0)
		source += Format("@%d", version);

	return GenerateHash ((UInt8*)&(source[0]), source.size());
}



string GetPlatformCachePath()
{
#if UNITY_OSX
	string result = getenv ("HOME");
	if (result.empty())
		return string();

#if WEBPLUG || UNITY_EDITOR
	return AppendPathName( result, "Library/Caches/Unity");
#else
	// In Standalones use Library/Caches/BundleID to match App Store Requirements
	return AppendPathName( result, "Library/Caches");
#endif

#elif UNITY_LINUX
	string result = getenv ("HOME");
	if (!result.empty())
		return string();

	return AppendPathName( result, ".unity/cache");
#elif UNITY_WINRT
	Platform::String^ path = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
	return AppendPathName(ConvertToUnityPath(path), "UnityCache");
#elif UNITY_WIN && !UNITY_WINRT
	std::string result = GetWindowsCacheBasePath();
	if (result.empty())
	return result;

		result = AppendPathName( result, "Unity" );

		// "Web Player" is wrong, the directory should be "WebPlayer", for consistency with other Unity usage.
		// but if there exits an old "Web Player" directory, use that. If both, a new and old one exists,
		// use the new one, as the old one has probably been created by a FusionFall build after running content with
		// the fixed web player.
		string oldCachePath = AppendPathName( result, "Web Player/Cache" );
		string newCachePath = AppendPathName( result, "WebPlayer/Cache" );
		if (IsDirectoryCreated (newCachePath))
			return newCachePath;
		else if (IsDirectoryCreated (oldCachePath))
			return oldCachePath;
		else
			return newCachePath;

#elif UNITY_IPHONE

	// under ios5 guidelines we should use Library folder for downloaded content
	// we don't want to use caches as they might be purged anyway
	// check apple's Technical Q&A QA1719
	std::string result = iphone::GetLibraryDirectory();
	return result.empty() ? result : AppendPathName( result, "UnityCache" );

#elif UNITY_PS3

	return AppendPathName( GetBasePath(), "/Cache/" );

#else
	string result = systeminfo::GetPersistentDataPath();
	if (result.empty())
		return result;

	return AppendPathName( result, "UnityCache" );
#endif
}

std::string GetDefaultApplicationIdentifierForCache(bool broken)
{
	// * Webplayer defaults to the Shared cache
	// * iOS / Android have dedicated cache folders per app managed by the OS -> thus end up in "Shared" as well.
	if (WEBPLUG || !UNITY_EMULATE_PERSISTENT_DATAPATH)
		return "Shared";
	else
	{
		std::string companyName = GetPlayerSettings().companyName;
		std::string productName = GetPlayerSettings().productName;
		if (broken)
		{
			ConvertToLegalPathNameBroken(companyName);
			ConvertToLegalPathNameBroken(productName);
#if UNITY_OSX && !UNITY_EDITOR
			// In the standalone, we changed the base path to Library/Caches without /Unity,
			// to match app store requirements. Still find old caches in Unity folder.
			return AppendPathName ("Unity",companyName + "_" + productName);
#endif
		}
		else
		{
#if UNITY_OSX && !UNITY_EDITOR
			// In the OS X standalone, return the bundle ID to match App Store requirements.
			return CFStringToString(CFBundleGetIdentifier(CFBundleGetMainBundle()));
#endif
			ConvertToLegalPathNameCorrectly(companyName);
			ConvertToLegalPathNameCorrectly(productName);
		}
		return companyName + "_" + productName;
	}
}

string GetCachingManagerPath(const std::string& path, bool createPath)
{
	std::string cachePath = GetPlatformCachePath();
	if (path.empty() && !createPath)
		return cachePath;

	string fullPath = AppendPathName(cachePath, path);
	if (!createPath)
		return fullPath;

	if (CreateDirectoryRecursive(fullPath))
		return fullPath;
	else
		return string();
}

bool CachingManager::MoveTempFolder (const string& from, const string& to)
{
#if UNITY_OSX || UNITY_IPHONE || UNITY_ANDROID || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
	if (rename(from.c_str(), to.c_str()))
#elif UNITY_WIN
	std::wstring wideFrom, wideTo;
	ConvertUnityPathName(from, wideFrom);
	ConvertUnityPathName(to, wideTo);
	if (!MoveFileExW(wideFrom.c_str(), wideTo.c_str(), MOVEFILE_WRITE_THROUGH))
#elif UNITY_PS3
	if (cellFsRename(from.c_str(), to.c_str()) != CELL_FS_SUCCEEDED)
#elif UNITY_XENON
	char charFrom[kDefaultPathBufferSize];
	char charTo[kDefaultPathBufferSize];

	DeleteFileOrDirectory( to.c_str() );
	ConvertUnityPathName( from.c_str(), charFrom, kDefaultPathBufferSize );
	ConvertUnityPathName( to.c_str(), charTo, kDefaultPathBufferSize );
	if (!MoveFile(charFrom, charTo))
#elif UNITY_PEPPER
	//todo.
#else
#error
#endif
	{
		return false;
	}
	SetFileFlags(to, kFileFlagTemporary, 0);
	return true;
}

time_t CachingManager::GenerateTimeStamp ()
{
	return time(NULL);
}

void AsyncCachedUnityWebStream::LoadCachedUnityWebStream ()
{
	string subpath = GetCachingManager().GetCurrentCache().URLToPath (m_Url, m_Version);
	string path = GetCachingManager().GetCurrentCache().GetFolder(subpath, false);
	if (path.empty())
	{
		m_Error = "Cannot find cache folder for "+ m_Url;
		return;
	}

	vector<string> fileNames;
	if (!CachingManager::ReadInfoFile (path, NULL, &fileNames))
	{
		m_Error = "Could not read __info file.";
		return;
	}

	// update timestamp.
	time_t timestamp = CachingManager::GenerateTimeStamp ();
	CachingManager::WriteInfoFile (path, fileNames, timestamp);
	GetCachingManager().GetCurrentCache().UpdateTimestamp(path, timestamp);

	if (m_RequestedCRC)
	{
		UInt32 crc = CRCBegin();
		for (vector<string>::iterator i = fileNames.begin(); i != fileNames.end(); i++)
		{
			File fd;
			string filepath = AppendPathName(path, *i);
			if (fd.Open (filepath, File::kReadPermission))
			{
				const size_t kHashReadBufferSize = 64*1024;
				UInt8* buffer = (UInt8*)UNITY_MALLOC(kMemTempAlloc,kHashReadBufferSize*sizeof(UInt8));
				size_t len = GetFileLength(filepath);
				size_t offset = 0;
				do
				{
					size_t readSize = fd.Read (buffer, kHashReadBufferSize);
					offset += readSize;
					crc = CRCFeed (crc, buffer, readSize);
				}
				while (offset < len);
				UNITY_FREE(kMemTempAlloc,buffer);
				fd.Close ();
			}
		}
		crc = CRCDone(crc);
		if (crc != m_RequestedCRC)
		{
			DeleteFileOrDirectory(path);
			m_Error = Format("CRC Mismatch. Expected %lx, got %lx. Will not load cached AssetBundle", m_RequestedCRC, crc);
			return;
		}
	}

	PersistentManager &pm = GetPersistentManager();
	pm.Lock();
	for (vector<string>::iterator i = fileNames.begin(); i != fileNames.end(); i++)
	{
		if (pm.IsStreamLoaded(*i))
		{
			pm.Unlock();
			m_Error = "Cannot load cached AssetBundle. A file of the same name is already loaded from another AssetBundle.";
			m_AssetBundleWithSameNameIsAlreadyLoaded = true;
			return;
		}

		if (!pm.LoadCachedFile(*i, AppendPathName(path, *i)))
		{
			pm.Unlock();
			m_Error = "Cannot load cached AssetBundle.";
			return;
		}
	}
	pm.Unlock();

	// Load resource file from cached path
	const string &firstPath = fileNames.front();
	
	int fileID = 1;
	Assert(m_LoadingAssetBundle == NULL);

	// Check if the first object is PreloadData.
	// If it's true, the stream is a scene assetBundle, just load the AssetBundle from the 2nd object.
	if (pm.GetClassIDFromPathAndFileID(firstPath, fileID) == ClassID(PreloadData))
		fileID = 2;

	// Try loading the asset bundle from disk.
	if (pm.GetClassIDFromPathAndFileID(firstPath, fileID) == ClassID(AssetBundle))
	{
		int instanceID = pm.GetInstanceIDFromPathAndFileID(firstPath, fileID);
		m_LoadingAssetBundle = dynamic_pptr_cast<AssetBundle*> (InstanceIDToObjectThreadSafe(instanceID));
	}

	// Otherwise create one
	// This is for the old scene assetBundle which doesn't contain an AssetBundle object, just for backward compatibility.
	if (m_LoadingAssetBundle == NULL)
	{
		m_LoadingAssetBundle = NEW_OBJECT_FULL (AssetBundle, kCreateObjectFromNonMainThread);
		m_LoadingAssetBundle->Reset();
		m_LoadingAssetBundle->AwakeFromLoadThreaded();
	}
	
	// Create cached web stream
	SET_ALLOC_OWNER(m_LoadingAssetBundle);
	CachedUnityWebStream* cachedStream = UNITY_NEW(CachedUnityWebStream, kMemFile);
	cachedStream->m_Version = m_Version;
	cachedStream->m_Files = fileNames;

	m_LoadingAssetBundle->m_CachedUnityWebStream = cachedStream;
}

void AsyncCachedUnityWebStream::IntegrateMainThread ()
{
	GetPersistentManager().IntegrateAllThreadedObjects();

	if (m_LoadingAssetBundle != NULL)
	{
		if (!m_LoadingAssetBundle->IsInstanceIDCreated())
		{
			Object::AllocateAndAssignInstanceID(m_LoadingAssetBundle);
			m_LoadingAssetBundle->AwakeFromLoad(kDidLoadThreaded);
	}

		m_InstanceID = m_LoadingAssetBundle->GetInstanceID();
		m_LoadingAssetBundle = NULL;
	}

	UnityMemoryBarrier();
	m_Complete = true;
}

void AsyncCachedUnityWebStream::Perform ()
{
	LoadCachedUnityWebStream();
	m_Progress = 1.0F;
}

AsyncCachedUnityWebStream::~AsyncCachedUnityWebStream ()
{
	AssertIf(!m_Complete);
}

AsyncCachedUnityWebStream::AsyncCachedUnityWebStream ()
{
	m_InstanceID = 0;
	m_Version = 0;
	m_LoadingAssetBundle = NULL;
	m_AssetBundleWithSameNameIsAlreadyLoaded = false;
}

string Cache::GetFolder (const string& path, bool create)
{
	return GetCachingManagerPath(AppendPathName(m_Name, path), create);
}

bool Cache::CleanCache()
{
	if (!m_Ready)
	{
		// If we are still scanning the cache folder size, we might have open
		// file handles, preventing us from deleting the cache folder. As we are deleting the folder,
		// we don't care about it's size any longer anyways.
		m_IndexThread.SignalQuit();
		m_IndexThread.WaitForExit();
	}

	// Cache clean operation needs to make sure things like cache
	// authorization are over:
	Mutex::AutoLock lock (m_Mutex);
	bool cleaned = GetCachingManager().CleanCache(m_Name);

	if ( cleaned )
	{
		m_BytesUsed = 0;
		m_CachedFiles.clear();
	}

	return cleaned;
}

void Cache::SetExpirationDelay (int expiration)
{
	m_CacheExpirationDelay = expiration;
	if (m_CacheExpirationDelay > CachingManager::kMaxCacheExpiration)
	{
		ErrorString(Format("Cache expiration may not be higher then %d", CachingManager::kMaxCacheExpiration));
		m_CacheExpirationDelay = CachingManager::kMaxCacheExpiration;
	}
	WriteCacheInfoFile (true);
}

void Cache::AddToCache (const string &path, int bytes)
{
	time_t lastUsedTime = 0;
	CachingManager::ReadInfoFile (path, &lastUsedTime, NULL);

	if (lastUsedTime > 0 && lastUsedTime < time(NULL) - m_CacheExpirationDelay)
	{
		// the file has expired. Don't add it, kill it.
		DeleteFileOrDirectory (path);
		return;
	}

	Mutex::AutoLock lock (m_Mutex);
	m_BytesUsed += bytes;

	string name = GetLastPathNameComponent (path);
	size_t seperator = name.find_last_of ('@');
	int version = 0;
	if (seperator != string::npos)
	{
		version = StringToInt(name.substr(seperator + 1));
		name = name.substr (0, seperator);
	}

	m_CachedFiles.insert(CachedFile (path, bytes, version, lastUsedTime));
}

void Cache::UpdateTimestamp (const string &path, time_t lastAccessed)
{
	Mutex::AutoLock lock (m_Mutex);

	for (CachedFiles::iterator i=m_CachedFiles.begin();i!=m_CachedFiles.end();i++)
	{
		if (i->path == path)
		{
			CachedFile replacement = *i;
			replacement.lastAccessed = lastAccessed;
			m_CachedFiles.erase(i);
			m_CachedFiles.insert(replacement);
			return;
		}
	}
}

Cache::Cache()
{
	m_BytesUsed = 0;
	m_Expires = 0x7fffffff;
	m_Ready = false;
	m_IncludePlayerURL = true;
	m_CacheExpirationDelay = CachingManager::kMaxCacheExpiration;
}

Cache::~Cache()
{
	m_Ready = true;
	m_IndexThread.WaitForExit();
}

void *Cache::ReadCacheIndexThreaded (void *data)
{
	Cache *cache = (Cache*)data;

	string cachePath = cache->GetFolder ("", false);

	set<string> paths;
	if (GetFolderContentsAtPath (cachePath, paths))
	{
		for (set<string>::iterator i = paths.begin(); i != paths.end() && !cache->m_Ready; i++)
		{
			if (IsDirectoryCreated(*i))
				cache->AddToCache (*i, GetFolderSizeRecursive(*i));
			if (cache->m_IndexThread.IsQuitSignaled())
				break;
		}
	}

	// Write cache index to update last used time.
	// Set a lock on it while we write things.
	Mutex::AutoLock lock (cache->m_Mutex);
	cache->WriteCacheInfoFile (false);
	cache->m_Ready = true;
	return NULL;
}

bool Cache::FreeSpace (size_t bytes)
{
	Mutex::AutoLock lock (m_Mutex);
	CachedFiles::iterator i = m_CachedFiles.begin();
	while (m_BytesAvailable - m_BytesUsed < bytes && i != m_CachedFiles.end())
	{
		CachedFiles::iterator cur = i;
		i++;

		const std::string &path = cur->path;
		if (!IsDirectoryCreated (path) || (!IsFileOrDirectoryInUse (path) && DeleteFileOrDirectory (path)))
		{
			m_BytesUsed -= cur->size;
			m_CachedFiles.erase (cur);
		}
	};

	return m_BytesAvailable - m_BytesUsed >= bytes;
}

#define kCacheInfoVersion 1
void Cache::WriteCacheInfoFile (bool updateExpiration)
{
	string folderPath = GetFolder ("", false);
	if (!IsDirectoryCreated(folderPath))
		return;

	if (updateExpiration)
		m_Expires = time(NULL) + m_CacheExpirationDelay;

	time_t oldestLastUsedTime = 0;
	if (m_CachedFiles.begin() != m_CachedFiles.end())
		oldestLastUsedTime = m_CachedFiles.begin()->lastAccessed;

	string indexFile = Format("%llu\n%d\n%llu\n", (UInt64)m_Expires, kCacheInfoVersion, (UInt64)oldestLastUsedTime);
	string path = AppendPathName( folderPath, kInfoFileName);

	File f;
	if (!f.Open(path, File::kWritePermission, File::kSilentReturnOnOpenFail|File::kRetryOnOpenFail))
	{
		return;
	}

	SetFileFlags(path, kFileFlagDontIndex, kFileFlagDontIndex);

	if (!f.Write(&indexFile[0], indexFile.size()))
	{
		f.Close();
		return;
	}
	f.Close();
}

#define NEXT_HEADER_FIELD(iterator,file,expectEOF) { (iterator)++; if ((iterator) == (file).end()) return (expectEOF); }
bool Cache::ReadCacheInfoFile (const string& path, time_t *expires, time_t *oldestLastUsedTime)
{
	InputString headerData;
	if (!ReadStringFromFile(&headerData, AppendPathName(path, kInfoFileName)))
		return false;

	vector<string> seperatedHeader = FindSeparatedPathComponents(headerData.c_str (), headerData.size (), '\n');
	vector<string>::iterator read = seperatedHeader.begin();

	if (read == seperatedHeader.end())
		return false;

	//expires
	if (expires)
		*expires = StringToInt(*read);

	NEXT_HEADER_FIELD (read, seperatedHeader, true);

	//version
	int version = StringToInt(*read);
	if (version < 1)
		return false;
	NEXT_HEADER_FIELD (read, seperatedHeader, false);

	//lastUsedTime
	if (oldestLastUsedTime)
		*oldestLastUsedTime = StringToInt(*read);
	NEXT_HEADER_FIELD (read, seperatedHeader, true);
	return true;
}

bool Cache::ReadCacheIndex (const string& name, bool getSize)
{
	m_Name = name;

	string cachePath = GetFolder ( "", false);
	time_t curTime = time(NULL);
	time_t oldestLastUsedTime = curTime;
	m_Expires = curTime + m_CacheExpirationDelay;

	ReadCacheInfoFile (cachePath, &m_Expires, &oldestLastUsedTime);

	if (getSize)
	{
		m_Ready = false;
		m_Mutex.Lock();
		m_BytesUsed = 0;
		m_CachedFiles.clear();
		m_Mutex.Unlock();
		m_IndexThread.Run (ReadCacheIndexThreaded, (void*)this);
	}
	else
		m_Ready = true;

	return true;
}


void Cache::SetMaximumDiskSpaceAvailable (long long maxUsage)
{
	if (maxUsage > m_MaxLicenseBytesAvailable)
	{
		ErrorString("Maximum disk space used exceeds what is allowed by the license");
		return;
	}

	m_BytesAvailable = maxUsage;
}


string CachingManager::GetTempFolder ()
{
	UnityGUID guid;
	guid.Init();
	string guidStr = GUIDToString(guid);
	string temp = "Temp";
	return GetCachingManagerPath (AppendPathName(temp, guidStr), true);
}

bool CachingManager::CleanCache(string name)
{
	string path = GetCachingManagerPath(name, false);

	if ( IsFileOrDirectoryInUse( path ) )
	{
		return false;
	}

	GetGlobalCachingManager().RebuildAllCaches ();

	return DeleteFileOrDirectory(path);
}

#define kInfoVersion 1

int CachingManager::WriteInfoFile (const string &path, const vector<string> &fileNames)
{
	return WriteInfoFile (path, fileNames, GenerateTimeStamp ());
}

int CachingManager::WriteInfoFile (const string &path, const vector<string> &fileNames, time_t lastAccessed)
{
	string info;
	// write negative info version (positive numbers had been used by previous, unversioned cache functions)
	info += IntToString(-kInfoVersion) + "\n";

	info += IntToString(lastAccessed) + "\n";

	info += IntToString(fileNames.size()) + "\n";

	for (vector<string>::const_iterator i=fileNames.begin();i != fileNames.end();i++)
		info += *i + "\n";

	File headerFile;
	string filename = AppendPathName(path, kInfoFileName);
	if (!headerFile.Open(filename, File::kWritePermission, File::kSilentReturnOnOpenFail|File::kRetryOnOpenFail))
		return 0;

	SetFileFlags(filename, kFileFlagDontIndex, kFileFlagDontIndex);

	if (!headerFile.Write(&info[0], info.size()))
	{
		headerFile.Close();
		return 0;
	}

	headerFile.Close();
	return info.size();
}

bool CachingManager::ReadInfoFile (const string &path, time_t *lastUsedTime, vector<string> *fileNames)
{
	InputString headerData;
	if (!ReadStringFromFile(&headerData, AppendPathName(path, kInfoFileName)))
		return false;

	vector<string> seperatedHeader = FindSeparatedPathComponents(headerData.c_str (), headerData.size (), '\n');
	vector<string>::iterator read = seperatedHeader.begin();

	if (read == seperatedHeader.end())
		return false;

	//version
	int version = StringToInt(*read);
	if (version >= 0)
		return false;

	NEXT_HEADER_FIELD (read, seperatedHeader, false);

	//lastUsedTime
	if (lastUsedTime)
		*lastUsedTime = StringToInt(*read);
	NEXT_HEADER_FIELD (read, seperatedHeader, false);

	if (fileNames != NULL)
	{
		//numFiles
		int numFiles = StringToInt(*read);
		fileNames->resize (numFiles);
		NEXT_HEADER_FIELD (read, seperatedHeader, false);

		//files
		for (int i=0;i<numFiles;i++)
		{
			(*fileNames)[i] = *read;
			NEXT_HEADER_FIELD (read, seperatedHeader, i == numFiles - 1);
		}
	}
	return true;
}

AsyncCachedUnityWebStream* CachingManager::LoadCached (const std::string& url, int version, UInt32 crc)
{
	AsyncCachedUnityWebStream* stream = new AsyncCachedUnityWebStream ();
	stream->m_Url = url;
	stream->m_Version = version;
	stream->m_RequestedCRC = crc;

	GetPreloadManager().AddToQueue(stream);

	return stream;
}

bool CachingManager::IsCached (const string &url, int version)
{
	string subpath = GetCachingManager().GetCurrentCache().URLToPath (url, version);
	string path = GetCachingManager().GetCurrentCache().GetFolder(subpath, false);
	if (path.empty())
		return false;

	vector<string> fileNames;
	if (!CachingManager::ReadInfoFile (path, NULL, &fileNames))
		return false;

	return true;
}

bool CachingManager::MarkAsUsed (const string &url, int version)
{
	string subpath = GetCachingManager().GetCurrentCache().URLToPath (url, version);
	string path = GetCachingManager().GetCurrentCache().GetFolder(subpath, false);
	if (path.empty())
		return false;

	vector<string> fileNames;
	if (!CachingManager::ReadInfoFile (path, NULL, &fileNames))
		return false;

	// update timestamp.
	time_t timestamp = GenerateTimeStamp ();
	CachingManager::WriteInfoFile (path, fileNames, timestamp);
	GetCachingManager().GetCurrentCache().UpdateTimestamp(path, timestamp);

	return true;
}

#if UNITY_IPHONE
// TODO: copy paste
void CachingManager::SetNoBackupFlag(const std::string &url, int version)
{
	string subpath = GetCachingManager().GetCurrentCache().URLToPath (url, version);
	string path = GetCachingManager().GetCurrentCache().GetFolder(subpath, false);

	if (path.empty())
		return;

	vector<string> fileName;
	if (!CachingManager::ReadInfoFile (path, NULL, &fileName))
		return;

	for (vector<string>::iterator i = fileName.begin(); i != fileName.end(); ++i)
		iphone::SetNoBackupFlag(i->c_str());
}

void CachingManager::ResetNoBackupFlag(const std::string &url, int version)
{
	string subpath = GetCachingManager().GetCurrentCache().URLToPath (url, version);
	string path = GetCachingManager().GetCurrentCache().GetFolder(subpath, false);

	if (path.empty())
		return;

	vector<string> fileName;
	if (!CachingManager::ReadInfoFile (path, NULL, &fileName))
		return;

	for (vector<string>::iterator i = fileName.begin(); i != fileName.end(); ++i)
		iphone::ResetNoBackupFlag(i->c_str());
}
#endif


void CachingManager::ClearTempFolder ()
{
	string path = GetCachingManagerPath("Temp", false);
	if (!IsDirectoryCreated (path))
		return;

	set<string> tempFolders;
	if (!GetFolderContentsAtPath (path, tempFolders))
		return;

	for (set<string>::iterator i = tempFolders.begin(); i != tempFolders.end(); i++)
	{
		string lockPath = AppendPathName( *i, "__lock" );
		if ( !IsFileCreated( lockPath ) || !IsFileOrDirectoryInUse( *i ) )
		{
			// No lock file exists or the directory was not in use. Delete temp folder.
			DeleteFileOrDirectory(*i);
			continue;
		}
	}
}

void CachingManager::Reset ()
{
	Dispose();

	m_Authorization = kAuthorizationNone;

	string defaultCacheName = GetDefaultApplicationIdentifierForCache(true);
	if (!IsDirectoryCreated (defaultCacheName))
		defaultCacheName = GetDefaultApplicationIdentifierForCache(false);

	#if WEBPLUG
	// Webplugin has a 50mb shared cache and includes the hashed unity3d file as the name
	SetCurrentCache (defaultCacheName, 50 * 1024 * 1024, true);
	#else
	// Other platforms has a 4GB shared cache
	m_Authorization = kAuthorizationUser;
	SetCurrentCache (defaultCacheName, std::numeric_limits<long long>::max(), false);
	#endif

	// - On webplayer and PC we are resonposible for cleaning up the cache automatically
	// - On mobile platforms the OS is responsible for cleaning up left over cache data.
	if (UNITY_EMULATE_PERSISTENT_DATAPATH || WEBPLUG)
		GetGlobalCachingManager().ClearAllExpiredCaches();

	ClearTempFolder ();
#if UNITY_PEPPER
	m_Enabled = false;
#elif WEBPLUG
	m_Enabled = GetGlobalBoolPreference(kCachingEnabledKey, true);
#else
	m_Enabled = true;
#endif
}

CachingManager::CachingManager ()
{
	m_Cache = NULL;
	Reset ();
}

void CachingManager::Dispose ()
{
	GetGlobalCachingManager().Dispose();
	if (m_Cache != NULL)
	{
		delete m_Cache;
		m_Cache = NULL;
	}
}

Cache& CachingManager::GetCurrentCache()
{
	return *m_Cache;
}

CachingManager::~CachingManager ()
{
	Dispose ();
}

bool CachingManager::VerifyValidDomain (const std::string &domain)
{
#if WEBPLUG && !UNITY_PEPPER
	string currentDomain = GetPlayerSettings().absoluteURL;
	if (currentDomain.find("http://") == 0 || currentDomain.find("https://") == 0)
	{
		currentDomain = GetStrippedPlayerDomain();

		//remove http://
		string domainStripped = domain;
		domainStripped.erase(0, 7);

		//remove path
		std::string::size_type pos = domainStripped.find("/", 0);
		if (pos != std::string::npos)
			domainStripped.erase(domainStripped.begin() + pos, domainStripped.end());

		if (currentDomain == "localhost")
			return true;

		//remove subdomain
		string currentDomainStripped = currentDomain;
		currentDomainStripped.erase(0, currentDomainStripped.length()-domainStripped.length());

		if (domainStripped == currentDomainStripped)
			return true;

		ErrorString ("You need to run this player from the "+domain+" domain to be authorized to use the caching API. (currently running from "+currentDomain+" )");
		return false;
	}
	// file:// domain is always valid for testing purposes
	else if (currentDomain.find("file://") == 0)
		return true;
	// absoluteUrl is not a valid url???
	else
	{
		ErrorString ("You need to run this player from the "+domain+" domain to be authorized to use the caching API.");
		return false;
	}
#else
	return true;
#endif
}

void CachingManager::SetCurrentCache (const string &name, long long size, bool includePlayerURL)
{
	if (m_Cache != NULL)
		delete m_Cache;
	m_Cache = new Cache();

	if (IsDirectoryCreated (GetCachingManagerPath(name, false)))
	{
		//update expiration time
		m_Cache->WriteCacheInfoFile (true);
	}

	m_Cache->m_BytesAvailable = size;
	m_Cache->m_MaxLicenseBytesAvailable = size;

	m_Cache->m_IncludePlayerURL = includePlayerURL;
	m_Cache->ReadCacheIndex (name, true);
}

bool CachingManager::Authorize (const string &name, const string &domain, long long size, int expiration, const string &signature)
{
	if (!VerifyValidDomain(domain))
		return false;

	m_Authorization = kAuthorizationNone;

	if (::VerifySignature ("Cache="+name+";Domain="+domain+";Admin", signature))
	{
		m_Authorization = kAuthorizationAdmin;
	}
	else
	{
		string authoriziationString = "Cache="+name+";Domain="+domain+Format(";Size=%lld",size);
		if (::VerifySignature (authoriziationString, signature))
			m_Authorization = kAuthorizationUser;
		else if (::VerifySignature (authoriziationString+Format(";Expiration=%d",expiration), signature))
			m_Authorization = kAuthorizationUser;
		else
		{
			WarningString ("Authorization to use the caching API failed.\n" + authoriziationString);
			return false;
		}
	}

	Assert(m_Authorization != kAuthorizationNone);

	if ( expiration > 0 )
	{
		// expiration in seconds from Jan 1, 1970 UTC (use `date -j mmddYYYY +%s` or `date -j -v +30d +%s` to generate)
		if ( time(NULL) > expiration)
		{
			m_Authorization = kAuthorizationNone;
			WarningString(Format("Authorization for this cache has expired. Please renew your caching authorization. It will still work in the editor, but will fail in any players you build. (%d)", expiration));
			return false;
		}
	}

	SetCurrentCache (name, size, false);

	return true;
}

CachingManager::AuthorizationLevel CachingManager::GetAuthorizationLevel ()
{
	if (!m_Enabled)
		return (m_Authorization == kAuthorizationAdmin) ? kAuthorizationAdmin : kAuthorizationNone;

	return m_Authorization;
}

void CachingManager::SetEnabled (bool enabled)
{
	m_Enabled = enabled;
#if WEBPLUG && !UNITY_PEPPER
	SetGlobalBoolPreference(kCachingEnabledKey, m_Enabled);
#endif
}

long long CachingManager::GetCachingDiskSpaceUsed ()
{
	return GetCurrentCache().GetCachingDiskSpaceUsed();
}

long long CachingManager::GetCachingDiskSpaceFree ()
{
	return GetCurrentCache().GetCachingDiskSpaceFree();
}

void CachingManager::SetMaximumDiskSpaceAvailable (long long maximumAvailable)
{
	return GetCurrentCache().SetMaximumDiskSpaceAvailable(maximumAvailable);
}

long long CachingManager::GetMaximumDiskSpaceAvailable ()
{
	return GetCurrentCache().GetMaximumDiskSpaceAvailable();
}

int CachingManager::GetExpirationDelay ()
{
	return GetCurrentCache().GetExpirationDelay();
}

bool CachingManager::GetIsReady ()
{
	return GetCurrentCache().GetIsReady();
}

CachingManager* gCachingManager = NULL;
CachingManager& GetCachingManager()
{
	if (gCachingManager == NULL)
		gCachingManager = new CachingManager();

	return *gCachingManager;
}


GlobalCachingManager* gGlobalCachingManager = NULL;
GlobalCachingManager &GetGlobalCachingManager()
{
	if (gGlobalCachingManager == NULL)
		gGlobalCachingManager = new GlobalCachingManager();

	return *gGlobalCachingManager;
}


void GlobalCachingManager::RebuildAllCaches ()
{
	// delete the cached vector of caches. Will rebuild on next call to get_index
	if (m_CacheIndices != NULL)
	{
		for (vector<Cache*>::iterator i = m_CacheIndices->begin(); i!=m_CacheIndices->end(); i++)
			delete *i;
		delete m_CacheIndices;
		m_CacheIndices = NULL;
	}
}

void GlobalCachingManager::Dispose ()
{
	if (m_CacheIndices != NULL)
	{
		for (vector<Cache*>::iterator i = m_CacheIndices->begin(); i!=m_CacheIndices->end(); i++)
			delete *i;
		delete m_CacheIndices;
		m_CacheIndices = NULL;
	}
}

std::vector<Cache*>& GlobalCachingManager::GetCacheIndices ()
{
	if (m_CacheIndices == NULL)
	{
		m_CacheIndices = new std::vector<Cache*>();
		ReadCacheIndices (*m_CacheIndices, true);
	}
	return *m_CacheIndices;
}

void GlobalCachingManager::ReadCacheIndices (vector<Cache*> &indices, bool getSize)
{
	indices.clear();
	string path = GetCachingManagerPath("", false);
	if (!IsDirectoryCreated (path))
		return;

	set<string> caches;
	if (!GetFolderContentsAtPath (path, caches))
		return;

	for (set<string>::iterator i = caches.begin(); i != caches.end(); i++)
	{
		if (!IsDirectoryCreated (*i))
			continue;

		string name = GetLastPathNameComponent(*i);
		if (name == "Temp")
			continue;

		Cache *cache = new Cache();
		bool success = cache->ReadCacheIndex (name, getSize);

		// Remove expired caches
		if (cache->m_Expires < time(NULL) || !success)
		{
			DeleteFileOrDirectory(*i);
			delete cache;
		}
		else
			indices.push_back(cache);
	}
}

void GlobalCachingManager::ClearAllExpiredCaches ()
{
	vector<Cache*> indices;
	ReadCacheIndices (indices, false);
	for (vector<Cache*>::iterator i = indices.begin(); i!=indices.end(); i++)
		delete *i;
}

#endif
