#include "UnityPrefix.h"

#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "AssetPreviews.h"
#include "ObjectImages.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Editor/Src/AssetPipeline/ImageConverter.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/Utility/AssetPreviewGeneration.h"

#include "AssetPreviewPostFixFile.h"
#include "Runtime/Misc/ResourceManagerGUIDs.h"

static PreviewTextureManagerCache *gPreviewTextureManagerCache = NULL;

Texture2D* MonoCreateAssetPreviewDontUnload (Object* asset, const std::string& assetPath)
{
	Texture2D* tex = MonoCreateAssetPreview (asset, std::vector<Object*>(), assetPath);
	if (tex)
		tex->SetHideFlags(Object::kHideAndDontSave);
	return tex;
}

PreviewTextureManager::PreviewTextureManager ()
{
	m_AccessCounter = 0;
	m_MaxTexturesLoaded = 50;	
}

PreviewTextureManager::~PreviewTextureManager()
{
	// Kill thread
	if (m_LoadingThread.IsRunning()) 
	{
		m_LoadingThread.WaitForExit (true);
	}

	// Clean up
	for (PreviewTextures::iterator i = m_PreviewTextures.begin(); i != m_PreviewTextures.end(); i++)
		DestroySingleObject (i->second.texture);

	for (std::list<LoadingImage>::iterator it = m_ThreadLoadedImages.begin(); it != m_ThreadLoadedImages.end(); it++)
		delete it->freeImageWrapper;
}

bool PreviewTextureManager::HasAnyNewPreviewTexturesAvailable ()
{
	Mutex::AutoLock lock (m_LoadingMutex);
	return !m_ThreadLoadedImages.empty();
}

bool PreviewTextureManager::IsLoadingPreview (int instanceID)
{
	Mutex::AutoLock lock (m_LoadingMutex);
	
	for (std::list<LoadingImage>::iterator i=m_ThreadLoadedImages.begin();i != m_ThreadLoadedImages.end();++i)
	{
		if (i->instanceID == instanceID)
			return true;
	}

	for (std::list<ImageRequest>::iterator i=m_ImageRequestQueue.begin();i != m_ImageRequestQueue.end();++i)
	{
		if (i->instanceID == instanceID)
			return true;
	}

	return false;
}

bool PreviewTextureManager::IsLoadingPreviews ()
{
	Mutex::AutoLock lock (m_LoadingMutex);

	if (!m_ThreadLoadedImages.empty ())
		return true;

	if (!m_ImageRequestQueue.empty ())
		return true;

	return false;
}

string GetPreviewPathFromGUID (const UnityGUID& guid)
{
	if (guid == GetBuiltinResourcesGUID() || guid == GetBuiltinExtraResourcesGUID())
		return AppendPathName (GetApplicationContentsPath (), "Resources/builtin_previews");
	else
		return GetMetaDataPathFromGUID (guid);
}

void* PreviewTextureManager::LoadingLoop (void* parameters)
{
	PreviewTextureManager& manager = *static_cast<PreviewTextureManager*> (parameters);
	while (!manager.m_LoadingThread.IsQuitSignaled())
	{
		ImageRequest request = manager.GetMostImportantTextureToLoad();
		if (request.instanceID != 0)
		{
			LoadingImage output;
			output.instanceID = request.instanceID;
			output.freeImageWrapper = NULL;
			
			std::auto_ptr<FreeImageWrapper> imageWrapper;
			
			dynamic_array<UInt8> imagebuffer (kMemTempAlloc);
			
			string path = GetPreviewPathFromGUID(request.guid);
			
			if (ExtractAssetPreviewImage (path, request.identifier, imagebuffer))
			{	
				if (!LoadImageFromBuffer(imagebuffer.begin(), imagebuffer.size(), imageWrapper, false))
				{
					ErrorString("Failed to load preview");
				}	
			}
			
			output.freeImageWrapper = imageWrapper.release();
			
			manager.m_LoadingMutex.Lock();
			manager.RemoveFromRequestQueue(request.instanceID);
			manager.m_ThreadLoadedImages.push_back(output);
			manager.m_LoadingMutex.Unlock();
		}
		else
		{
			Thread::Sleep (0.1F);
		}
	}
	
	//should never get here, but kills gcc warning
	return 0;
}

PreviewTextureManager::ImageRequest PreviewTextureManager::GetMostImportantTextureToLoad ()
{
	Mutex::AutoLock lock (m_LoadingMutex);
	
	if (m_ImageRequestQueue.empty())
	{
		return ImageRequest();
	}
	else
	{
		ImageRequest request = m_ImageRequestQueue.front();
		return request;
	}

}

void PreviewTextureManager::RemoveFromRequestQueue (SInt32 instanceID)
{
	Mutex::AutoLock lock (m_LoadingMutex);
	
	if(m_ImageRequestQueue.front().instanceID == instanceID)
		m_ImageRequestQueue.pop_front();
}

void PreviewTextureManager::ClearTemporaryPreviews()
{
	for (PreviewTextures::iterator it = m_PreviewTextures.begin(); it != m_PreviewTextures.end();)
	{
		if (it->second.temporary)
		{
			if (it->second.texture)
				DestroySingleObject (it->second.texture);
			m_PreviewTextures.erase(it++);
		}
		else
		{
			it++;
		}
	}
}

Texture2D* PreviewTextureManager::UpdatePreviewIfDirty (int instanceID)
{
	Object* asset = Object::IDToPointer (instanceID);
	
	// If 'asset' is null then it has not been loaded and therefore not dirty
	if ( asset && asset->IsPersistent() && asset->IsPersistentDirty ())
	{	
		PreviewTextures::iterator found = m_PreviewTextures.find (instanceID);
		if (found != m_PreviewTextures.end ())
		{
			if (!found->second.temporary)
			{
				// Note: Several asset types have no preview generation function (Fonts, MonoScripts etc.)
				// Even if the texture is null we still add it to the cache to prevent keep trying to create preview for them.
				Texture2D* texture = MonoCreateAssetPreviewDontUnload ( asset, GetAssetPathFromObject(asset) );
				
				DestroySingleObject (found->second.texture);

				found->second.texture = texture;
				found->second.temporary = true;
				found->second.lastAccess = ++m_AccessCounter;
			}
			return found->second.texture;
		}
		else
		{
			if (m_PreviewTextures.size() >= m_MaxTexturesLoaded)
				Cleanup();

			PreviewImage preview;
			// Note: Several asset types have no preview generation function (Fonts, MonoScripts etc.)
			// Even if the texture is null we still add it to the cache to prevent keep trying to create preview for them.
			preview.texture = MonoCreateAssetPreviewDontUnload ( asset, GetAssetPathFromObject(asset) );
			preview.temporary = true;
			preview.lastAccess = ++m_AccessCounter;
			m_PreviewTextures[instanceID] = preview;	

			return preview.texture;
		}
	}

	return NULL;
}


Texture2D *PreviewTextureManager::GetPreview (SInt32 instanceID)
{
	if (instanceID == 0)
		return NULL;
	
	if (Texture2D* texture = UpdatePreviewIfDirty (instanceID))
		return texture;
	
	// Make sure we have a thread that is loading images
	if (!m_LoadingThread.IsRunning())
		m_LoadingThread.Run(LoadingLoop, this);
	
		
	// Grab all loaded images from the loading thread
	m_LoadingMutex.Lock ();
	std::list<LoadingImage>  loadedImages;
	m_ThreadLoadedImages.swap(loadedImages);
	m_LoadingMutex.Unlock ();
	
	// Integrate all images that have been loaded on the main thread
	for (std::list<LoadingImage>::iterator i=loadedImages.begin();i != loadedImages.end();i++)
	{
		LoadingImage& image = *i;
		
		PreviewTextures::iterator found = m_PreviewTextures.find (image.instanceID);
		if (found != m_PreviewTextures.end ()){
			// image allready in the cache. update accescounter and delete source
			found->second.lastAccess = ++m_AccessCounter;
			delete image.freeImageWrapper;
			continue;
		}

		PreviewImage preview;
		preview.texture = NULL;
		preview.lastAccess = ++m_AccessCounter;
		
		if (image.freeImageWrapper != NULL)
		{
			preview.texture = CreateObjectFromCode<Texture2D>(kDefaultAwakeFromLoad, kMemTextureCache);
			preview.texture->SetImage (image.freeImageWrapper->GetImage(), Texture2D::kNoMipmap);
			preview.texture->SetHideFlags(Object::kDontSave);
			delete image.freeImageWrapper;
		}

		m_PreviewTextures[image.instanceID] = preview;
		
		Cleanup ();
	}
	
	// Find the texture, return it if we have it
	// and bump the acccess counter
	PreviewTextures::iterator found = m_PreviewTextures.find (instanceID);
	if (found != m_PreviewTextures.end ())
	{
		found->second.lastAccess = ++m_AccessCounter;
		return found->second.texture;
	}
	else
	{
		Mutex::AutoLock lock (m_LoadingMutex);
		
		ImageRequest request;
		request.instanceID = instanceID;
		
		// if the image has been loaded in the meantime. continue
		PreviewTextures::iterator found = m_PreviewTextures.find (instanceID);
		if (found != m_PreviewTextures.end ())
			return NULL;

		// keep a queue of the most recent requests, never make the queue bigger than kMaxRequestQueueSize
		if (find (m_ImageRequestQueue.begin(), m_ImageRequestQueue.end(), request) == m_ImageRequestQueue.end())
		{
			request.identifier = GetPersistentManager().GetLocalFileID(instanceID);
			request.guid = ObjectToGUID(PPtr<Object> (instanceID));

			m_ImageRequestQueue.push_back(request);
			if (m_ImageRequestQueue.size () > m_MaxTexturesLoaded)
				m_ImageRequestQueue.pop_front();
		}
	}
	
	return NULL;
}

void PreviewTextureManager::RemovePreview (SInt32 instanceID)
{
	// Find the texture, remove it it if we have it
	PreviewTextures::iterator found = m_PreviewTextures.find (instanceID);
	if (found != m_PreviewTextures.end ())
	{
		DestroySingleObject (found->second.texture);
		m_PreviewTextures.erase (found);
	}
}

void PreviewTextureManager::Cleanup ()
{
	while (!m_PreviewTextures.empty() && m_PreviewTextures.size() >= m_MaxTexturesLoaded)
		DeleteOldest();
}

void PreviewTextureManager::DeleteOldest ()
{
	unsigned oldest = std::numeric_limits<unsigned>::max();
	PreviewTextures::iterator found = m_PreviewTextures.end();
	for (PreviewTextures::iterator i = m_PreviewTextures.begin(); i != m_PreviewTextures.end(); i++)
	{
		if (i->second.lastAccess < oldest) 
		{
			oldest = i->second.lastAccess;
			found = i;
		}
	}
	
	if (found != m_PreviewTextures.end())
	{
		DestroySingleObject (found->second.texture);
		m_PreviewTextures.erase (found);
	}
}


void PreviewTextureManager::SetTextureLimit (int limit)
{
	m_MaxTexturesLoaded = limit;
	Cleanup ();
}

bool PreviewTextureManager::HasAnyLoadedPreviews ()
{
	return !m_PreviewTextures.empty();
}


// -----------------------------------------------------------
//	PreviewTextureManagerCache
// -----------------------------------------------------------

PreviewTextureManager* PreviewTextureManagerCache::GetPreviewTextureManagerByID (int clientID)
{
	std::map<int, PreviewTextureManager*>::iterator it = m_PreviewTextureManagers.find (clientID);
	if (it != m_PreviewTextureManagers.end ())
		return it->second;

	//LogString (Format ("Create new PreviewTextureManager used by clientID %d", clientID));

	PreviewTextureManager* previewTextureManager = new PreviewTextureManager();
	m_PreviewTextureManagers[clientID] = previewTextureManager;
	return previewTextureManager;
}

void PreviewTextureManagerCache::DeletePreviewTextureManagerByID (int clientID)
{
	std::map<int, PreviewTextureManager*>::iterator it = m_PreviewTextureManagers.find (clientID);
	if (it != m_PreviewTextureManagers.end ())
	{
		//LogString (Format ("Deleting PreviewTextureManager used by clientID %d", clientID));

		delete it->second;
		m_PreviewTextureManagers.erase (it);
	}
}

void PreviewTextureManagerCache::RemovePreview (int instanceID)
{
	std::map<int, PreviewTextureManager*>::iterator it;
	for (it = m_PreviewTextureManagers.begin (); it != m_PreviewTextureManagers.end (); it++)
	{
		it->second->RemovePreview (instanceID);
	}
}

void PreviewTextureManagerCache::ClearAllTemporaryPreviews ()
{
	std::map<int, PreviewTextureManager*>::iterator it;
	for (it = m_PreviewTextureManagers.begin (); it != m_PreviewTextureManagers.end (); it++)
	{
		it->second->ClearTemporaryPreviews ();
	}
}

bool PreviewTextureManagerCache::HasAnyLoadedPreviews ()
{
	std::map<int, PreviewTextureManager*>::iterator it;
	for (it = m_PreviewTextureManagers.begin (); it != m_PreviewTextureManagers.end (); it++)
	{
		if (it->second->HasAnyLoadedPreviews ())
			return true;
	}
	
	return false;
}


PreviewTextureManagerCache &GetPreviewTextureManagerCache ()
{
	if (gPreviewTextureManagerCache == NULL)
		gPreviewTextureManagerCache = new PreviewTextureManagerCache ();
	return *gPreviewTextureManagerCache;
}


void PostprocessAssetsUpdatePreviews (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, string>& moved)
{
	if (!GetPreviewTextureManagerCache().HasAnyLoadedPreviews ())
		return;
	
	for (std::set<UnityGUID>::const_iterator i=refreshed.begin();i != refreshed.end();i++)
	{
		const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(*i);
		if (asset)
		{
			GetPreviewTextureManagerCache().RemovePreview (asset->mainRepresentation.object.GetInstanceID());
			for (int j=0;j<asset->representations.size();j++)
			{
				GetPreviewTextureManagerCache().RemovePreview (asset->representations[j].object.GetInstanceID());
			}
		}
	}
}
