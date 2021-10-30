#include "UnityPrefix.h"
#include "AssetBundleUtility.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Configuration/UnityConfigureOther.h"
#include "Runtime/Serialize/SwapEndianBytes.h"

#if ENABLE_WWW
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Serialize/SwapEndianBytes.h"
#include "PlatformDependent/CommonWebPlugin/FileStream.h"
#include "PlatformDependent/CommonWebPlugin/UnityWebStream.h"
#include "Runtime/Export/WWW.h"
#include "Runtime/Misc/WWWCached.h"
#endif

#include "Runtime/Misc/Player.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Mono/MonoScriptManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Misc/SaveAndLoadHelper.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/GetComponent.h"
#include "Runtime/Threads/ThreadUtility.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"

#include "Runtime/Scripting/ScriptingUtility.h"

#if UNITY_EDITOR
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Runtime/Serialize/BuildTargetVerification.h"
#endif

static char const* kIncompatibleScriptsMsg = "The asset bundle '%s' could not be loaded because it references scripts that are not compatible with the currently loaded ones. Rebuild the AssetBundle to fix this error.";
static char const* kIncompatibleRuntimeClassMsg = "The asset bundle '%s' could not be loaded because it contains run-time classes of incompatible version. Rebuild the AssetBundle to fix this error.";
static char const* kIncompatibleRuntimeMsg = "The asset bundle '%s' could not be loaded because it is not compatible with this newer version of the Unity runtime. Rebuild the AssetBundle to fix this error.";

static bool TestScriptCompatibility (std::vector<AssetBundleScriptInfo> const& scripts)
{
	bool result = true;
	
	#if ENABLE_SCRIPTING
	MonoScriptManager& sm = GetMonoScriptManager();

#if 0 // debug
	{
		MonoScriptManager::AllScripts existing = sm.GetAllRuntimeScripts ();
		printf_console ("LOADED SCRIPTS: %d\n", existing.size ());
		for (MonoScriptManager::AllScripts::iterator it = existing.begin (), end = existing.end (); it != end; ++it)
		{
			printf_console ("  %s (%s) hash: %08x\n", (*it)->GetName(), (*it)->GetClassName().c_str(), (*it)->GetPropertiesHash());
		}
	}
	
	{
		printf_console ("TESTING SCRIPTS: %d\n", scripts.size ());
		for (size_t i=0, size = scripts.size (); i<size; ++i)
			printf_console ("  [%s.]%s hash: %08x\n", scripts[i].nameSpace.c_str(), scripts[i].className.c_str(), scripts[i].hash);
	}
#endif
	
	for (std::vector<AssetBundleScriptInfo>::const_iterator it = scripts.begin (), end = scripts.end (); it != end; ++it)
	{
		if (MonoScript* script = sm.FindRuntimeScript (it->className, it->nameSpace, it->assemblyName))
		{
			UInt32 supported = script->GetPropertiesHash ();
			UInt32 loading = it->hash;
			
			if (supported != loading)
			{
				WarningStringMsg ("AssetBundle loading failed because the %s script serialization hash does not match. Supported: %08x, loading: %08x\n", script->GetScriptFullClassName ().c_str(),  (unsigned int)supported, (unsigned int)loading);
				result = false;
 			}
		}
	}
	#endif
	
	return result;
}

static bool TestRuntimeClassCompatibility (std::vector<std::pair<int, UInt32> > const& classes)
{
	bool result = true;
	for (size_t i=0, size = classes.size (); i<size; ++i)
	{
		int classID = classes[i].first;
		UInt32 loading = classes[i].second;

		UInt32 supported = GetBuildSettings ().GetHashOfClass (classID);
		if (supported != 0 && loading != supported)
		{
			WarningStringMsg ("AssetBundle loading failed because the %s class serialization hash does not match. Supported: %08x, loading: %08x\n", Object::ClassIDToString(classID).c_str (), (unsigned int)supported, (unsigned int)loading);
			result = false;
		}
	}

	return result;
}

bool TestAssetBundleCompatibility (AssetBundle& bundle, const std::string& bundleName, std::string& error)
{
	error = string();

	// We only report a single type of incompatibility here even when there are multiple
	// incompatibilities present in the bundle.  However, since the solution to each
	// incompatibility is rebuilding the asset bundle which will also get rid of
	// whatever other incompatibilities are present in the bundle, we don't need to
	// worry about that.

	// Test script class compatibility.
	if (!TestScriptCompatibility (bundle.m_ScriptCompatibility))
	{
		error = Format (kIncompatibleScriptsMsg, bundleName.c_str ());
		return false;
	}

	// Test runtime class compatibility.
	if (!TestRuntimeClassCompatibility (bundle.m_ClassCompatibility))
	{
		error = Format (kIncompatibleRuntimeClassMsg, bundleName.c_str ());
		return false;
	}

#if !UNITY_WEBPLAYER // We do not allow breaking backwards-compatibility in the webplayer at this point.

	bool isEditorTargetingWebPlayer = false;
	#if UNITY_EDITOR
		isEditorTargetingWebPlayer =
			   GetEditorUserBuildSettingsPtr ()
			&& IsWebPlayerTargetPlatform (GetEditorUserBuildSettings ().GetActiveBuildTarget ());
	#endif

	// Check against our runtime compatibility version to see if there's
	// been some more profound changes to the runtime that prevent old
	// asset bundles from working.
	//
	// In the case of being in the editor and targeting the webplayer, we
	// suppress the check given that the webplayer will load the bundle
	// correctly.
	if (!isEditorTargetingWebPlayer && bundle.m_RuntimeCompatibility < AssetBundle::CURRENT_RUNTIME_COMPATIBILITY_VERSION)
	{
		error = Format (kIncompatibleRuntimeMsg, bundleName.c_str ());
		return false;
	}

#endif

	return true;
}

static AssetBundle* FindAssetBundleObject (std::string const& assetBundlePath)
{
	PersistentManager& pm = GetPersistentManager();

	// An AssetBundle can be the first (AssetBundle) or the second object (StreamedLevel assets file)
	const int fileID1 = 1, fileID2 = 2;

	int fileID = 0;
	if (pm.GetClassIDFromPathAndFileID(assetBundlePath, fileID1) == ClassID(AssetBundle))
		fileID = fileID1;
	else if (pm.GetClassIDFromPathAndFileID(assetBundlePath, fileID2) == ClassID(AssetBundle))
		fileID = fileID2;
	else
	{
		// Old streamed scene asset bundle that has no AssetBundle object.
		return NULL;
	}

	int instanceID = pm.GetInstanceIDFromPathAndFileID (assetBundlePath, fileID);
	return dynamic_instanceID_cast<AssetBundle*> (instanceID);
}

static AssetBundle* InitializeAssetBundle (const std::string& assetBundleName,
										   const std::string& assetBundlePath,
										   AssetBundle::UncompressedFileInfoContainer* uncompressedFiles,
										   UnityWebStream* webStream,
										   bool performCompatibilityChecks = true)
{
	PersistentManager& pm = GetPersistentManager();
	UNUSED(pm);
	
	// Locate AssetBundle object.
	AssetBundle* assetBundle = FindAssetBundleObject (assetBundlePath);
	if (!assetBundle)
	{
		// We used to not include AssetBundle objects in streamed scene
		// asset bundles that had type trees.  This is no longer the case
		// but to handle this case, we create an AssetBundle object on
		// demand (like before).

		assetBundle = NEW_OBJECT (AssetBundle);
		assetBundle->Reset ();
		assetBundle->AwakeFromLoad (kInstantiateOrCreateFromCodeAwakeFromLoad);

		// Set to incompatible runtime version.  Will only load on
		// webplayer.
		assetBundle->m_RuntimeCompatibility = 0;
	}

	if (assetBundle->m_UncompressedFileInfo)
		UNITY_DELETE (assetBundle->m_UncompressedFileInfo, kMemFile);

	// Associate uncompressed files with asset bundle, if we have them
	// (will only be the case for local asset bundles).
	if (uncompressedFiles)
	{
		UNITY_TRANSFER_OWNERSHIP (uncompressedFiles, kMemFile, assetBundle);
		assetBundle->m_UncompressedFileInfo = uncompressedFiles;
	}

	// Associate webstream with asset bundle, if we have one
	// (will only be the case for downloaded asset bundles).
#if ENABLE_WWW
	if (webStream)
	{
		assetBundle->m_UnityWebStream = webStream;
		assetBundle->m_UnityWebStream->Retain ();
		webStream->SetCachedAssetBundle (assetBundle->GetInstanceID());
	}
#endif

	// Make sure the asset bundle can be loaded into this
	// build of the player.
	if (performCompatibilityChecks)
	{
		string errorMessage;
		bool isCompatible = TestAssetBundleCompatibility (*assetBundle, assetBundleName, errorMessage);
		if (!isCompatible)
		{
			ErrorString (errorMessage);				
			UnloadAssetBundle (*assetBundle, true);
			return NULL;
		}
	}
	
	return assetBundle;
}

#if ENABLE_WWW
static bool LooksLikeStreamedSceneBundle (FileStream* data)
{
	if (data->m_Files.size () == 0)
		return false;

	// If first or second file name starts with "BuildPlayer-", it's
	// probably a streamed scene bundle.  In case it has extra resources,
	// that will be the first file.
	const bool firstOrSecondFileIsSceneData =
		data->m_Files[0].name.find ("BuildPlayer-") == 0 ||
		(data->m_Files.size () > 1 && data->m_Files[1].name.find ("BuildPlayer-") == 0);

	return firstOrSecondFileIsSceneData;
}

static bool LooksLikeCustomAssetBundle (FileStream* data)
{
	if (data->m_Files.size () == 0)
		return false;

	// If the first file name starts with "CustomAssetBundle" or "CAB",
	// it's probably a custom asset bundle.
	const bool isCustomAssetBundle =
		data->m_Files[0].name.find ("CustomAssetBundle") == 0 ||
		data->m_Files[0].name.find ("CAB" ) == 0;

	return isCustomAssetBundle;
}

static bool LooksLikeValidAssetBundle (FileStream* data)
{
	return LooksLikeStreamedSceneBundle (data)
	    || LooksLikeCustomAssetBundle( data );
}

static AssetBundle* ExtractAssetBundle(UnityWebStream* unityweb, const char* url, bool performCompatibilityChecks = true)
{
	if (!unityweb || !unityweb->GetFileStream())
		return NULL;

	FileStream* data = (FileStream*)unityweb->GetFileStream();

	// Return last asset bundle instance if already accessed.
	if (data->m_Files.size() >= 1 && dynamic_instanceID_cast<AssetBundle*> (unityweb->GetCachedAssetBundle()))
		return dynamic_instanceID_cast<AssetBundle*> (unityweb->GetCachedAssetBundle());

	///@TODO: the UnityWebStream should contain information on if the file is an asset bundle instead
	if (!LooksLikeValidAssetBundle (data))
	{
		ErrorString("The unity3d file is not a valid asset bundle.");
		return NULL;
	}

	PersistentManager& pm = GetPersistentManager();
	pm.Lock();

	for (FileStream::iterator i=data->begin();i != data->end();i++)
	{
		if (pm.IsStreamLoaded(i->name))
		{
			pm.Unlock();
			ErrorString(Format("The asset bundle '%s' can't be loaded because another asset bundle with the same files are already loaded", url));
			return NULL;
		}
	}

	// Load all memory streams
	for (FileStream::iterator i=data->begin();i != data->end();i++)
	{
		if (!pm.LoadMemoryBlockStream(i->name, i->blocks, i->offset, i->end, url))
		{
			pm.Unlock();
			ErrorString(Format("The asset bundle '%s' can't be loaded because it was not built with the right version or build target.", url));
			return NULL;
		}
	}

	pm.Unlock();

	////WORKAROUND!
	//// There was a bug in 4.2 where script compatibility information was *always* written
	//// out into streamed scene asset bundles which in turn makes us always check these
	//// hashes on loading and reject bundles if they don't match.  As we *always* build
	//// streamed scene bundles with type trees when targeting the webplayer, this makes us
	//// not load bundles we can actually load.  Work around this issue here by disabling
	//// compatibility checks altogether when loading a streamed scene bundle into the webplayer.
	#if UNITY_WEBPLAYER
	if (LooksLikeStreamedSceneBundle (data))
		performCompatibilityChecks = false;
	#endif

	const string &path = data->begin ()->name;
	return InitializeAssetBundle (url, path, NULL, unityweb, performCompatibilityChecks);
}

AssetBundle *ExtractAssetBundle(WWW &www)
{
	if (!www.HasDownloadedOrMayBlock ())
		return NULL;
	www.BlockUntilDone();
	
	#if ENABLE_CACHING
	if (www.GetType() == kWWWTypeCached)
		return static_cast<WWWCached&>(www).GetAssetBundle();
	#endif
	
	return ExtractAssetBundle(www.GetUnityWebStream(), www.GetUrl());
}

AssetBundleCreateRequest::AssetBundleCreateRequest( const UInt8* dataPtr, int dataSize )
	: m_UnityWebStream(0)
	, m_AssetBundle(0)
	, m_EnableCompatibilityChecks (true)
{
	UnityWebStreamHeader header;

	int result = ParseStreamHeader (header, dataPtr, dataPtr + dataSize);
	if (result)
	{
		if (result == 1)
		{
			ErrorString("Asset bundle is incomplete");
		}
		else if(result == 2)
		{
			ErrorString("Error parsing asset bundle");
		}

		m_Progress =  1.f;
		UnityMemoryBarrier();
		m_Complete = true;
		return;
	}

	m_UnityWebStream = UNITY_NEW_AS_ROOT(UnityWebStream(NULL, 0, 0), kMemFile, "WebStream", "");
#if SUPPORT_THREADS
	m_UnityWebStream->SetDecompressionPriority(GetPreloadManager().GetThreadPriority());
#endif
	m_UnityWebStream->Retain();

	m_UnityWebStream->FeedDownloadData(dataPtr, dataSize, true);

	GetPreloadManager().AddToQueue(this);
}

AssetBundleCreateRequest::~AssetBundleCreateRequest()
{
	if (m_UnityWebStream != NULL)
		m_UnityWebStream->Release();
}

void AssetBundleCreateRequest::Perform ()
{
	if (m_UnityWebStream == NULL)
		return;
	m_UnityWebStream->WaitForThreadDecompression();
}
void AssetBundleCreateRequest::IntegrateMainThread ()
{
	m_AssetBundle = ExtractAssetBundle(m_UnityWebStream, "<unknown>", m_EnableCompatibilityChecks);
	UnityMemoryBarrier();
	m_Complete = true;
}

float AssetBundleCreateRequest::GetProgress ()
{
	if (m_UnityWebStream == NULL)
		return 1.f;
	m_UnityWebStream->UpdateProgress();
	m_Progress = m_UnityWebStream->GetProgressUntilLoadable();
	return m_Progress;
}

#endif

static void ForcePreload (AssetBundle& bundle, const AssetBundle::AssetInfo& info)
{
	for (int i=0;i<info.preloadSize;i++)
	{
		PPtr<Object> preload = bundle.m_PreloadTable[i + info.preloadIndex];
		preload.IsValid();
	}
}

///@TODO: For 4.0 we should remove this function and make AssetBundle.Load use the LoadAsync code path and wait for completion.

static void ProcessAssetBundleEntries(AssetBundle& bundle, AssetBundle::range entries, ScriptingObjectPtr systemTypeInstance, vector<Object*>& output, bool stopAfterOne)
{
#if ENABLE_SCRIPTING
	ScriptingClassPtr klass = GetScriptingTypeRegistry().GetType(systemTypeInstance);

	for (AssetBundle::iterator i=entries.first;i != entries.second;i++)
	{
		Object* obj = i->second.asset;
		if (obj == NULL)
			continue;
		
		ScriptingObjectPtr o = Scripting::ScriptingWrapperFor(obj);
		if (o && scripting_class_is_subclass_of(scripting_object_get_class(o, GetScriptingTypeRegistry()), klass))
		{
			ForcePreload(bundle, i->second);
			output.push_back(obj);
			if (stopAfterOne) return;
		}
			
		Unity::GameObject* go = dynamic_pptr_cast<GameObject*> (obj);
		if (go != NULL)
		{
			o = ScriptingGetComponentOfType(*go, systemTypeInstance, false);
			if (o != SCRIPTING_NULL)
			{
				ForcePreload(bundle, i->second);
				output.push_back(ScriptingObjectToObject<Object>(o));
				if (stopAfterOne) return;
			}
		}
	}
#endif
}

Object* LoadNamedObjectFromAssetBundle (AssetBundle& bundle, const std::string& name, ScriptingObjectPtr type)
{
	
	string lowerName = ToLower(name);
	AssetBundle::range found = bundle.GetPathRange(lowerName);
	
	vector<Object*> result;
	ProcessAssetBundleEntries(bundle,found,type,result,true);
	if (result.empty())
		return NULL;

	return result[0];
}

Object* LoadMainObjectFromAssetBundle (AssetBundle& bundle)
{
	PreloadLevelOperation::PreloadBundleSync (bundle, "");
	return bundle.m_MainAsset.asset;
}

void LoadAllFromAssetBundle (AssetBundle& assetBundle, ScriptingObjectPtr type, vector<Object* >& output)
{
	AssetBundle::range found = assetBundle.GetAll();
	ProcessAssetBundleEntries(assetBundle,found,type,output,false);
}

namespace UnityConsoleStream_Static
{

	static bool ReadBigEndian (const UInt8*& cur, const UInt8* dataEnd, UInt32& data)
	{
		if (cur + sizeof(SInt32) > dataEnd)
			return false;
		data = *reinterpret_cast<const SInt32*> (cur);
#if UNITY_LITTLE_ENDIAN
		SwapEndianBytes(data);	
#endif
		cur += sizeof(SInt32);
		return true;
	}

	static bool ReadString (const UInt8*& cur, const UInt8* dataEnd, std::string& data)
	{
		int length = 0;
		while (true)
		{
			if (cur + length >= dataEnd)
				return false;

			if (cur[length] == '\0')
				break;

			length++;
		}

		data.assign (cur, cur + length);
		cur += length + 1;
		return true;
	}

} // namespace UnityWebStream_Static


bool ParseUncompressedFileHeader(AssetBundle::UncompressedFileInfoContainer* files, const UInt8* cur, const UInt8* headerEnd, int offset)
{
	using namespace UnityConsoleStream_Static;
	UInt32 fileCount;
	if (!ReadBigEndian(cur, headerEnd, fileCount))
		return false;

	files->reserve (fileCount);

	for (int i = 0; i < fileCount; i++)
	{
		AssetBundle::UncompressedFileInfo fileInfo;

		if (!ReadString(cur, headerEnd, fileInfo.fileName))	return false;
		if (!ReadBigEndian(cur, headerEnd, fileInfo.offset)) return false;
		else
		{
			fileInfo.offset += offset;
		}
		if (!ReadBigEndian(cur, headerEnd, fileInfo.size)) return false;

		files->push_back(fileInfo);
	}

	return true;
}

#if SUPPORT_SERIALIZATION_FROM_DISK
AssetBundle* ExtractAssetBundle(std::string const& assetBundlePathName)
{
	using namespace UnityConsoleStream_Static;
	File file;
	if (!file.Open (assetBundlePathName, File::kReadPermission, File::kSilentReturnOnOpenFail))
		return NULL;

#define ExitAndCloseFile() {file.Close(); return NULL;}

	const int kBundleHeaderSize = sizeof("UnityRaw") + 
		sizeof(SInt32) + 
		sizeof(UNITY_WEB_BUNDLE_VERSION) +
		sizeof(UNITY_WEB_MINIMUM_REVISION) +
		sizeof(SInt32) * 4;

	UInt8 prefix[kBundleHeaderSize];
	int bytesRead = file.Read(0, prefix, kBundleHeaderSize);
	if (bytesRead != kBundleHeaderSize)
	{
		ErrorString("Error while reading asset bundle header!");
		ExitAndCloseFile();
	}

	// read prefix
	std::string streamId, unityVersion, minimumRevision;
	UInt32 streamVersion;

	UInt8 const* it = prefix, *end = prefix + kBundleHeaderSize;
	if (!ReadString(it, end, streamId))
		ExitAndCloseFile()
	if (streamId != "UnityRaw")
	{
		ErrorStringMsg("This asset bundle was not created with UncompressedAssetBundle flag, expected id 'UnityRaw', got '%s'", streamId.c_str());
		ExitAndCloseFile();
	}
	if (!ReadBigEndian(it, end, streamVersion))
		ExitAndCloseFile();
	if (!ReadString(it, end, unityVersion))
		ExitAndCloseFile();
	if (!ReadString(it, end, minimumRevision))
		ExitAndCloseFile();

	UInt32 byteStart, headerSize, numberOfLevelsToDownloadBeforeStreaming, levelCount, completeFileSize, fileInfoHeaderSize;

	if (!ReadBigEndian(it, end, byteStart))
		ExitAndCloseFile();
	if (!ReadBigEndian(it, end, headerSize))
		ExitAndCloseFile();
	if (!ReadBigEndian(it, end, numberOfLevelsToDownloadBeforeStreaming))
		ExitAndCloseFile();
	if (!ReadBigEndian(it, end, levelCount))
		ExitAndCloseFile();

	UInt8* fullHeader = (UInt8*)alloca (headerSize);
	file.Read(0, fullHeader, headerSize);

	it = fullHeader + kBundleHeaderSize;
	end = fullHeader + headerSize;

	UInt32 dummyCompressed, dummyUncompressed;
	for (UInt32 i = 0; i < levelCount; i++)
	{
		if (!ReadBigEndian(it, end, dummyCompressed))
			ExitAndCloseFile();
		if (!ReadBigEndian(it, end, dummyUncompressed))
			ExitAndCloseFile();
	}
	if (!ReadBigEndian(it, end, completeFileSize))
		ExitAndCloseFile();
	if (!ReadBigEndian(it, end, fileInfoHeaderSize))
		ExitAndCloseFile();

	void* fileContainerRoot = UNITY_NEW_AS_ROOT(int,kMemFile,"Temp","");
	AssetBundle::UncompressedFileInfoContainer* files;
	{
		SET_ALLOC_OWNER(fileContainerRoot);
		files = UNITY_NEW(AssetBundle::UncompressedFileInfoContainer, kMemFile);
	}
	UInt8* fileInfoHeader = (UInt8*)alloca (fileInfoHeaderSize);
	file.Read (headerSize, fileInfoHeader, fileInfoHeaderSize);
	file.Close();

#undef ExitAndCloseFile

	if (ParseUncompressedFileHeader(files, fileInfoHeader, fileInfoHeader + fileInfoHeaderSize, headerSize) == false)
	{
		ErrorString("Failed to parsed asset bundle header");
		UNITY_DELETE(files, kMemFile);
		return NULL;
	}

	if (files->size() == 0 || (files->begin()->fileName.find("BuildPlayer-") != 0 && files->begin()->fileName.find("CustomAssetBundle") != 0 && files->begin()->fileName.find("CAB") != 0))
	{
		ErrorString("The unity3d file is not a valid asset bundle.");
		UNITY_DELETE(files, kMemFile);
		return NULL;
	}

	PersistentManager& pm = GetPersistentManager();
	pm.Lock();

	for (AssetBundle::UncompressedFileInfoContainer::iterator i = files->begin(); i != files->end(); ++i)
	{
		if (pm.IsStreamLoaded(i->fileName))
		{
			pm.Unlock();
			ErrorString(Format("The asset bundle '%s' can't be loaded because another asset bundle with the same files are already loaded.", assetBundlePathName.c_str()));
			UNITY_DELETE(files, kMemFile);
			return NULL;
		}
	}

	// Load all memory streams
	for (AssetBundle::UncompressedFileInfoContainer::iterator i=files->begin(); i != files->end(); ++i)
	{
		// TODO: check flags
		if (!pm.LoadExternalStream(i->fileName, assetBundlePathName, kSerializeGameRelease, i->offset))
		{
			pm.Unlock();
			ErrorString(Format("The asset bundle '%s' can't be loaded because it was not built with the right version or build target.", assetBundlePathName.c_str()));
			UNITY_DELETE(files, kMemFile);
			return NULL;
		}
	}

	pm.Unlock();

	const string &path = files->begin ()->fileName;
	return InitializeAssetBundle (assetBundlePathName, path, files, NULL);
}
#else
AssetBundle* ExtractAssetBundle(std::string const& assetBundlePathName)
{
	ErrorString("Failed to load asset bundle (not supported).");
	return NULL;
}
#endif
