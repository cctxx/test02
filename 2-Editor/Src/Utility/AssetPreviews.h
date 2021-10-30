#pragma once

#include "Runtime/Graphics/Image.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Utilities/GUID.h"
#include "Editor/Src/AssetPipeline/ImageConverter.h"

class Texture2D;
class Texture;
class Object;
class FreeImageWrapper;
struct UnityGUID;


class PreviewTextureManager
{
public:	
	PreviewTextureManager ();
	~PreviewTextureManager();

	// If the preview is not already cached then load it assync from HDD. (Will return null until loaded)
	Texture2D* GetPreview (SInt32 instanceID);
	
	// If the preview is not already cached then it is generated and cached. 
	Texture2D* GetTemporaryPreview (SInt32 instanceID);

	// Are we currently in the process of loading a preview for this instanceID?
	bool IsLoadingPreview (int instanceID);
	
	// Are we currently in the process of loading any previews?
	bool IsLoadingPreviews ();
	
	// Use this function to remove a cached texture. This ensures
	// that when a preview is requested it will be loaded from HDD again
	void RemovePreview (SInt32 instanceID);

	// Call this function to ensure that new previews are generated for dirty assets.
	void ClearTemporaryPreviews();

	void SetTextureLimit (int limit);
	bool HasAnyNewPreviewTexturesAvailable ();
	
	// Are there any previews in memory?
	bool HasAnyLoadedPreviews ();
	
private:
	Texture2D* UpdatePreviewIfDirty (int instanceID);

	struct LoadingImage
	{
		SInt32 instanceID;
		FreeImageWrapper* freeImageWrapper;
	};

	struct ImageRequest
	{
		LocalIdentifierInFileType  identifier;
		UnityGUID                  guid;
		SInt32                     instanceID;
		
		ImageRequest () { identifier = 0; instanceID = 0; }
		friend bool operator == (const ImageRequest& lhs, const ImageRequest& rhs) { return lhs.guid == rhs.guid && lhs.identifier == rhs.identifier; }
	};
	
	
	struct PreviewImage
	{
		PreviewImage() : texture(0), lastAccess(0), temporary(false) {}
		Texture2D*		texture;
		unsigned		lastAccess; 
		bool			temporary;	
	};
	
	unsigned m_AccessCounter;
	
	// This is called from the loading thread to get the best next texture to load
	ImageRequest GetMostImportantTextureToLoad ();
	void RemoveFromRequestQueue (SInt32 instanceID);
	
	// All images that are active and in memory
	typedef std::map<SInt32, PreviewImage> PreviewTextures;
	PreviewTextures  m_PreviewTextures;
	
	// This stores all images that have completed thread loading
	// and can now be integrated from the main thread
	// This value is protected by m_LoadingMutex and accesses by both threads
	std::list<LoadingImage>  m_ThreadLoadedImages;
	std::list<ImageRequest>  m_ImageRequestQueue;
	
	// How many textures to load.
	int m_MaxTexturesLoaded;
	
	Mutex m_LoadingMutex;
	Thread m_LoadingThread;
	
	void DeleteOldest ();
	static void* LoadingLoop (void* parameters);
		
	void Cleanup ();
};



// The PreviewTextureManagerCache handles one shared and client owned texture preview managers
// Note: To prevent clients from removing previews from the manager
// that was used by another client, we make sure each client gets his own cache.
// Client is responsible for calling DeletePreviewTextureManagerByID when it is not needed anymore.
class PreviewTextureManagerCache
{
public:
	PreviewTextureManager* GetPreviewTextureManagerByID (int clientID);
	void DeletePreviewTextureManagerByID (int clientID);
	
	void RemovePreview (int instanceID);
	void ClearAllTemporaryPreviews ();
	
	bool HasAnyLoadedPreviews ();
	
private:
	std::map<int, PreviewTextureManager*> m_PreviewTextureManagers; 
};
PreviewTextureManagerCache& GetPreviewTextureManagerCache ();

void PostprocessAssetsUpdatePreviews (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved);
