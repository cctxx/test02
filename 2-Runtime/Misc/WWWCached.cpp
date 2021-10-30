#include "UnityPrefix.h"

#if ENABLE_CACHING && ENABLE_WWW
#include "WWWCached.h"
#include "Runtime/Misc/CachingManager.h"
#include "PlatformDependent/CommonWebPlugin/UnityWebStream.h"
#include "Runtime/Misc/AssetBundleUtility.h"
#include "Runtime/Misc/SaveAndLoadHelper.h"
#include "Runtime/Misc/AssetBundle.h"

#if UNITY_EDITOR
#include "Runtime/BaseClasses/IsPlaying.h"
#endif

void WWWCached::StartDownload(bool cached)
{
	if (m_WWW != NULL)
	{
		m_WWW->Release();
		m_WWW = NULL;
	}
	
	printf_console("starting www download: %s\n", m_URL);
	WWWHeaders headers;	
	m_WWW = WWW::Create(m_URL, NULL, 0, headers, true, cached, m_CacheVersion, m_RequestedCRC);
	m_WWW->SetThreadPriority(m_ThreadPriority);
}

bool WWWCached::IsDownloadingDone () const
{
	// Actually, IsDone() ought to be simplified, but this temporary hack
	// alows the class to compile properly.
	return const_cast<WWWCached*>(this)->IsDoneImpl();
}

bool WWWCached::IsDoneImpl()
{
	if (m_Abort)
		return true;
	if (m_WWW != NULL)
	{
		if (m_WWW->IsDone ())
		{
			if (!m_WWW->IsCached())
			{
				// downloaded to memory.
				if (m_WWW->GetError() != NULL)
				{
					m_Error = m_WWW->GetError();
					m_Abort = true;
				}
				// no error: success. we are done.
				else return true;
			}
			else
			{
				// downloaded to cache
				if (m_WWW->GetError() != NULL)
				{
					UnityWebStream* stream = m_WWW->GetUnityWebStream();

					if (stream && !stream->GetError().empty())
					{
						// we might have had an error caching to disk.
						// try memory download instead.
						StartDownload (false);
						return false;
					}
					else
					{
						m_Error = m_WWW->GetError();
						m_Abort = true;
					}
				}
				else
				{
					m_DidDownload = true;
					printf_console("loading from cache: %s\n", m_URL);
					m_CacheRequest = GetCachingManager().LoadCached(m_URL, m_CacheVersion, m_RequestedCRC);
					if (m_CacheRequest == NULL)
					{
						// error: try memory download instead.
						StartDownload (false);
						return false;
					}
				}
			}
			m_WWW->Release();
			m_WWW = NULL;
		}
	}
	else if (m_CacheRequest != NULL)
	{
		if (m_CacheRequest->IsDone ())
		{
			if (m_CacheRequest->DidSucceed ())
			{
				AssetBundle* bundle = m_CacheRequest->GetAssetBundle ();

				////WORKAROUND!
				//// Same workaround as in ExtractAssetBundle (unfortunately, we have two different
				//// places where we load bundles from WWW).
				bool performCompatibilityChecks = true;
				#if UNITY_WEBPLAYER
				const bool isStreamedSceneBundle =
					bundle->m_CachedUnityWebStream->m_Files[0].find ("BuildPlayer-") == 0 ||
					(bundle->m_CachedUnityWebStream->m_Files.size () > 1 && bundle->m_CachedUnityWebStream->m_Files[1].find ("BuildPlayer-") == 0);
				performCompatibilityChecks = !isStreamedSceneBundle;
				#endif
				
				// Verify that we can actually use the bundle and
				// abort if we don't.
				if (performCompatibilityChecks && bundle && !TestAssetBundleCompatibility (*bundle, m_URL, m_Error))
				{
					m_Abort = true;
					return false;
				}
				
				return true;
			}
			else 
			{
				if (!m_CacheRequest->AssetBundleWithSameNameIsAlreadyLoaded ())
				{
					// File not in cache. Try downloading it to the cache
					if (!m_DidDownload)
						StartDownload (GetCachingManager().GetCurrentCache().GetIsReady());		
					else
						// We already tried downloading to the cache without success. Try memory download instead.
						StartDownload (false);
				}
				else
				{
					m_Error = m_CacheRequest->GetError();
					m_Abort = true;
				}
				m_CacheRequest->Release();
				m_CacheRequest = NULL;		
			}
		}
	}
	return false;
}

float WWWCached::GetProgress ()  const
{
	if (m_DidDownload)
		return 1.0f; 
	else if (m_CacheRequest != NULL && m_CacheRequest->DidSucceed ())
		return 1.0f;
	else if (m_WWW != NULL)
		return m_WWW->GetProgress();
	else
		return 0.0f;
}

double WWWCached::GetETA () const
{
	if (m_DidDownload)
		return 0.0; 
	else if (m_CacheRequest != NULL && m_CacheRequest->DidSucceed ())
		return 0.0;
	else  if (m_WWW != NULL)
		return m_WWW->GetETA();
	else
		return 0.0;
}

WWWCached::WWWCached (const char* url, int version, UInt32 crc)
	: WWW (false, 0, crc)
{
	m_URL = (char*)malloc(strlen(url)+1);
	AssertIf(!m_URL);
	strcpy(m_URL, url);
	m_Abort = false;
	m_DidDownload = false;
	m_WWW = NULL;
	m_CacheRequest = NULL;
	m_CacheVersion = version;
	m_AssetBundleRetrieved = false;

#if UNITY_EDITOR
	if (!IsWorldPlaying())
	{
		m_Error = "WWW.LoadFromCacheOrDownload is only available in play mode.";
		m_Abort = true;
		return;
	}	
#endif

	if (!GetCachingManager().GetEnabled())
	{
		StartDownload(false);
		return;
	}
	m_CacheRequest = GetCachingManager().LoadCached(m_URL, m_CacheVersion, crc);
	if (m_CacheRequest == NULL)
	{
		// error: try memory download instead.
		StartDownload (false);
		return;
	}
}

WWWCached::~WWWCached () 
{
	free (m_URL);
	if (m_CacheRequest != NULL)
		m_CacheRequest->Release();

	if (m_WWW != NULL)
		m_WWW->Release();
}

const char*  WWWCached::GetError() 
{ 
	if(!m_Error.empty()) 
		return m_Error.c_str(); 
	else if (m_WWW != NULL)
		return m_WWW->GetError(); 
	else
		return NULL;
}

void WWWCached::Cancel()
{
	if (m_WWW != NULL)
		m_WWW->Cancel();

	if ( m_CacheRequest != NULL && !m_AssetBundleRetrieved )
	{
		// If a cached request was canceled early, then we have to wait until
		// the request is complete on the other thread. Otherwise, AssetBundle
		// collisions may occur if we don't unload the asset bundle.
		
		while (!m_CacheRequest->IsDone ())
		{
			GetPreloadManager().UpdatePreloading();
			Thread::Sleep(0.05f);
		}
		
		if (m_CacheRequest->DidSucceed ())
		{
			AssetBundle* bundle = GetAssetBundle();
			if ( bundle )
				UnloadAssetBundle( *bundle, true );
		}
	}
	m_Abort = true;
}

void WWWCached::SetThreadPriority( ThreadPriority priority )
{
	if (m_WWW != NULL)
		m_WWW->SetThreadPriority(priority);
}

void WWWCached::BlockUntilDone ()
{
	while (!IsDone())
	{
		if (m_WWW != NULL)
			m_WWW->BlockUntilDone();
		
		if ( m_CacheRequest != NULL )
			GetPreloadManager().UpdatePreloading();
		
		Thread::Sleep(0.05f);
	}
}

bool WWWCached::HasDownloadedOrMayBlock ()
{
	if (GetError () != NULL)
	{
		ErrorString(Format("You are trying to load data from a www stream which had the following error when downloading.\n%s", GetError()));
		return false;
	}

	if (m_WWW != NULL)
		return m_WWW->HasDownloadedOrMayBlock();
	return true;
}

AssetBundle* WWWCached::GetAssetBundle ()
{
	BlockUntilDone ();
	if (m_WWW && !m_WWW->IsCached())
		return ExtractAssetBundle (*m_WWW);
	else if (m_CacheRequest != NULL)
	{
		m_AssetBundleRetrieved = true;
		return m_CacheRequest->GetAssetBundle();
	}		
	return NULL;
}

const UInt8* WWWCached::GetData()
{
	ErrorString(kWWWCachedAccessError);
	return NULL; 
}
const UInt8* WWWCached::GetPartialData() const
{
	ErrorString(kWWWCachedAccessError);
	return NULL; 
}
size_t WWWCached::GetSize() 
{
	ErrorString(kWWWCachedAccessError);
	return 0; 
}
size_t WWWCached::GetPartialSize() const
{
	ErrorString(kWWWCachedAccessError); 
	return 0; 
}

#endif //ENABLE_CACHING && ENABLE_WWW
