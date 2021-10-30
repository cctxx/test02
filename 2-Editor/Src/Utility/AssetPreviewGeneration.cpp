#include "UnityPrefix.h"
#include "AssetPreviewGeneration.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Graphics/Texture2D.h"
#include "AssetPreviewPostFixFile.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Scripting.h"


enum { FailedPreviewGen = 0, kNoPreviewAvailable = 1, kPreviewGenerated = 2 };

static int AppendAssetPreviewToMetaData (File& file, Object* asset, const std::vector<Object*>& subAssets,  const std::string& assetPath, const InstanceIDToLocalIdentifier& instanceIDToLocalIdentifier, AssetPreviewImageFooters& images, size_t& fileSize)
{
	if (asset == NULL)
		return kNoPreviewAvailable;
	
	dynamic_array<UInt8> buffer (kMemTempAlloc);
	
	Texture2D* tex = MonoCreateAssetPreview (asset, subAssets, assetPath);
	if (tex == NULL)
		return kNoPreviewAvailable;
	
	if (!instanceIDToLocalIdentifier.count(asset->GetInstanceID()))
		return FailedPreviewGen;
	
	LocalIdentifierInFileType identifier = instanceIDToLocalIdentifier.find(asset->GetInstanceID())->second;
	int result = FailedPreviewGen;
	if (tex->EncodeToPNG(buffer))
	{	
		if (WriteAssetPreviewImage (file, images, identifier, buffer.begin(), buffer.size(), fileSize))
			result = kPreviewGenerated;
		fileSize += buffer.size();
	}
	
	DestroySingleObject(tex);
	
	return result;
}

bool AppendAssetPreviewToMetaData (const std::string& previewPath, LibraryRepresentation& mainAsset, vector<LibraryRepresentation>& objects, const std::string& assetPath, const InstanceIDToLocalIdentifier& instanceIDToLocalIdentifier)
{
	size_t fileSize = ::GetFileLength(previewPath);
	
	File file;
	if (!file.Open(previewPath, File::kAppendPermission))
		return false;
	
	bool success = true;
	int res;
	
	AssetPreviewImageFooters images;
	
	// Append image data to path
	std::vector<Object*> subAssets;
	for (unsigned i=0; i<objects.size (); ++i)
		subAssets.push_back(objects[i].object);
	res = AppendAssetPreviewToMetaData(file, mainAsset.object, subAssets, assetPath, instanceIDToLocalIdentifier, images, fileSize);
	if (res == kPreviewGenerated)
		mainAsset.flags |= LibraryRepresentation::kHasFullPreviewImage;
	else
		mainAsset.flags &= ~LibraryRepresentation::kHasFullPreviewImage;
	if (res == FailedPreviewGen)
		success = false;
	
	subAssets.clear ();
	for (int i=0;i<objects.size();i++)
	{
		res = AppendAssetPreviewToMetaData(file, objects[i].object, subAssets, assetPath, instanceIDToLocalIdentifier, images, fileSize);
		if (res == kPreviewGenerated)
			objects[i].flags |= LibraryRepresentation::kHasFullPreviewImage;
		else
			objects[i].flags &= ~LibraryRepresentation::kHasFullPreviewImage;
		if (res == FailedPreviewGen)
			success = false;
	}
	
	success &= WriteAssetPreviewFooter(file, images);
	
	file.Close();
	
	return success;
}


Texture2D* MonoCreateAssetPreview (Object* asset, const std::vector<Object*>& subAssets, const std::string& assetPath)
{
	if (!asset)
		return NULL;
	
	// Since generating the preview uses rendering device, we need
	// to make rendering is properly enclosed in BeginFrame/EndFrame
	GfxDevice& device = GetGfxDevice();
	bool wasInsideFrame = device.IsInsideFrame();
	if (!wasInsideFrame)
		device.BeginFrame();

	void* params[] = { Scripting::ScriptingWrapperFor(asset), CreateScriptingArrayFromUnityObjects (subAssets, ClassID(Object)), scripting_string_new(assetPath)  };
	ScriptingObjectOfType<Texture2D> preview =  CallStaticMonoMethod ("AssetPreviewUpdater", "CreatePreviewForAsset", params);
	
	if (!wasInsideFrame)
		device.EndFrame();
	
	if (preview)
		return preview;

	return NULL;
}
