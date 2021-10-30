#include "UnityPrefix.h"
#include "PackageUtility.h"
#include "Editor/Src/BuildPipeline/BuildSerialization.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Editor/Src/AssetPipeline/MdFourGenerator.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetServer/ASController.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Src/Utility/ExceptionUtilities.h"
#include "Editor/Src/Utility/Base64.h"
#include "EditorHelper.h"
#include "Utility/AssetPreviewPostFixFile.h"
#include <queue>

using namespace std;

static const char* kImportPackageFolder = "Temp/Export Package/";
static const char* kAssetInformationFile = "pathname";
static const char* kExportFolder = "Temp/Export Package";
static const char* kUnityPackageExtension = "unitypackage";


static bool ExtractAssetInformation (const string& fileName, string* path, MdFour* digest)
{
	// Read File
	InputString data;
	if (!ReadStringFromFile(&data, fileName))
		return false;

	// We support the old format which contained only the pathname
	// and the new format which has the format:
	// pathName\n 
	// digest in hexadecimal text (32 characters)
	InputString::size_type pos = data.rfind ('\n');
	if (pos != InputString::npos)
	{
		if (pos + 32 + 1 != data.size ())
			return false;
			
		InputString hexedDigest (data.begin () + pos + 1, data.end ());
		*digest = StringToMdFour (hexedDigest.c_str());
		data.erase (pos, data.size ());
		*path = data.c_str();
		return true;
	}
	else
	{
		*digest = MdFour ();
		*path = data.c_str();
		return true;
	}
}

bool ImportPackageStep1 (const std::string& packagePath, vector<ImportPackageAsset>& assets, string& packageIconPath)
{
	AssetDatabase* database = &AssetDatabase::Get();
	AssetServer::Controller& maint = AssetServer::Controller::Get();
	map<string,UnityGUID> exportedPathToGuid;

	if (IsWorldPlaying ())
	{	
		UnityBeep ();
		return false;
	}
	
	// Give us a unique location inside kImportPackageFolder to
	// unpack the contents of the current package.  We need to be able
	// to unpack multiple packages to kImportPackageFolder and then,
	// after importing, delete the entire toplevel folder.
	string unpackPath = GetUniqueTempPath (kImportPackageFolder);

	try
	{
		// Uncompress package into 
		DisplayProgressbar("Decompressing", "Decompressing package", 0);

		// Create export folder
		if (! CreateDirectoryRecursive (unpackPath))
			ThrowString ("Couldn't create temporary directory");
		
		if (!DecompressPackageTarGZ (packagePath, unpackPath))
			ThrowString ("Couldn't decompress package");
			
		// Correct permissions. OS X often makes permissions become read only when copying over the network.
		// (Maybe it's because DecompressPackageTarGZ passes -p (preserve permissions) to the tar command)
		if (! SetPermissionsReadWrite (PathToAbsolutePath (unpackPath)))
			ThrowString ("Couldn't set permissions");

		ClearProgressbar();
	}
	catch (const UnityStr& error)
	{
		ClearProgressbar();
		ErrorString ("Error while importing package: " + error);
		UnityBeep ();
		return false;
	}

	// Place all the assets and metadata into their place using the same interface the asset server is using
	maint.LockAssets ();
	
	string errorMessage;
	try
	{
		set<string> subpaths;
		packageIconPath = AppendPathName(unpackPath, ".icon.png");
		if ( ! IsFileCreated(packageIconPath) )
			packageIconPath = "";

		
		GetFolderContentsAtPath(unpackPath, subpaths);

		//// Generate message that contains which assets will be replaced/created
		//// and do some error checking
		string message;
		
		for (set<string>::iterator i=subpaths.begin();i != subpaths.end();i++)
		{
			// Get guid of the asset
			string guidFolder = *i;
			string folderName = GetLastPathNameComponent (guidFolder);
			
			// Packages created on OS X with some unix file system flags may store that
			// information in "PaxHeader" directories, which tar on windows cannot read.
			// Ignore them.
			if (folderName == "PaxHeader")
				continue;
			
			UnityGUID guid = StringToGUID (folderName);
			if (guid == UnityGUID ())
				ThrowString ("Package has unknown format");
			
			// Calculate asset pathname using pathNameFile
			string exportedAssetPath;
			MdFour exportedDigest;
			if (!ExtractAssetInformation (AppendPathName (guidFolder, kAssetInformationFile), &exportedAssetPath, &exportedDigest))
				ThrowString ("Package has unknown format");
				
			exportedPathToGuid[GetGoodPathName(exportedAssetPath)] = guid;
			
			//// Replace the asset if the guid is in the assetdatabase
			if (database->IsAssetAvailable (guid))
			{
				// Don't update folders
				if( IsFileCreated(AppendPathName (guidFolder, AssetServer::kAssetStream)) ) {
					// Only replace the asset if the file is different
					MdFour digest = AssetServerCache::Get().FindCachedDigest(guid);
					if (exportedDigest != digest)
					{
						// Setup asset info
						string currentAssetPath = GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
						
						ImportPackageAsset ass;
						ass.exportedAssetPath = exportedAssetPath;
						ass.guid = guid;
						ass.guidFolder = guidFolder;
						ass.message = DeleteFirstPathNameComponent (currentAssetPath);
						ass.exists = true;
						ass.isFolder = !IsFileCreated(AppendPathName (ass.guidFolder, AssetServer::kAssetStream));
						ass.previewPath = AppendPathName (ass.guidFolder, AssetServer::kAssetPreviewStream);
						if ( ! IsFileCreated(ass.previewPath) )
							ass.previewPath="";
						
						assets.push_back(ass);
					}
				}
			}
			//// Create the asset with the guid and pathname from the package
			else
			{
				// Skip assets that contain the name _Deprecated
				vector<string> components = FindSeparatedPathComponents(exportedAssetPath, kPathNameSeparator);
				if (find (components.begin(), components.end(), string("_Deprecated")) == components.end())
				{
					ImportPackageAsset asset;
					asset.guid = guid;
					asset.guidFolder = guidFolder;
					asset.exportedAssetPath = exportedAssetPath;
					asset.message = DeleteFirstPathNameComponent (exportedAssetPath);
					asset.exists = false;
					asset.isFolder = !IsFileCreated(AppendPathName (asset.guidFolder, AssetServer::kAssetStream));
					asset.previewPath = AppendPathName (asset.guidFolder, AssetServer::kAssetPreviewStream);
					if ( ! IsFileCreated(asset.previewPath) )
						asset.previewPath="";
					
					
					assets.push_back(asset);
				}
			}
		}
		
		if (! errorMessage.empty ())
			ThrowString (errorMessage.c_str());
	}
	catch (const UnityStr& error)
	{
		ErrorString ("Error while importing package: " + error);
		UnityBeep ();
		DeleteFileOrDirectory(kImportPackageFolder);
		maint.UnlockAssets (kForceSynchronousImport);	
		return false;
	}

	maint.UnlockAssets (kForceSynchronousImport);
	
	std::sort(assets.begin(), assets.end());
	
	// Fill in parent guids, so the import dialog works correctly
	for (vector<ImportPackageAsset>::iterator i=assets.begin(); i != assets.end(); i++) {
		string parentPath = GetGoodPathName(DeleteLastPathNameComponent (i->exportedAssetPath));
		i->parentGuid = UnityGUID();
		map<string,UnityGUID>::iterator parentIter = exportedPathToGuid.find(parentPath);
		if(parentIter != exportedPathToGuid.end() )
			i->parentGuid = parentIter->second;

	}
		
	return true;
}

static vector<ImportPackageAsset> gDelayedImportPackageStep2;

struct ExportPackageInfo
{
	string m_PackagePathName;
	set<UnityGUID> m_Guids;
};

static vector<ExportPackageInfo> gDelayedExportPackages;

void DelayedExportPackage (const set<UnityGUID>& guids, const string& packagePathName)
{
	ExportPackageInfo package;
	package.m_PackagePathName = packagePathName;
	package.m_Guids = guids;

	gDelayedExportPackages.push_back (package);
}

void TickPackageImport ()
{
	if (!gDelayedImportPackageStep2.empty())
	{
		vector<ImportPackageAsset> step = gDelayedImportPackageStep2;
		gDelayedImportPackageStep2.clear();
		ImportPackageStep2(step);
	}

	if (!gDelayedExportPackages.empty())
	{
		for (int i = 0; i < gDelayedExportPackages.size (); ++i)
			ExportPackage (gDelayedExportPackages[i].m_Guids, gDelayedExportPackages[i].m_PackagePathName);

		gDelayedExportPackages.clear ();
	}
}

void DelayedImportPackageStep2 (vector<ImportPackageAsset>& assets)
{
	gDelayedImportPackageStep2.insert (gDelayedImportPackageStep2.end (), assets.begin(), assets.end());
}


bool ImportPackageStep2 (vector<ImportPackageAsset>& assets)
{
	AssetDatabase* database = &AssetDatabase::Get();
	AssetServer::Controller& maint = AssetServer::Controller::Get();
	AssetServer::Configuration& conf = AssetServer::Configuration::Get();
	maint.LockAssets ();	

	bool success = false;
	try
	{
		//// Go through all folders. Lookup if an asset with this guid exists. If it does replace the asset.
		//// If it doesn't create a new asset
		while (!assets.empty ())
		{
			ImportPackageAsset importAsset = assets.back ();
			assets.pop_back ();
			UnityGUID guid = importAsset.guid;
		
			if (!importAsset.enabled)
				continue;

			// If the guid can't be found in the database
			// We look up the exported asset path name
			// and replace that asset instead of trying to create a new 
			// asset which will fail because one is already there
			if (! database->IsAssetAvailable (guid) )
			{
				UnityGUID replaceGUID;
				if (GetGUIDPersistentManager ().PathNameToGUID (importAsset.exportedAssetPath, &replaceGUID))
				{
					if (database->IsAssetAvailable (replaceGUID))
					{
						guid = replaceGUID;
					}
				}
			}
			// Setup streams
			AssetServer::StreamMap streams;
			if(!importAsset.isFolder) {
				streams[AssetServer::kAssetStream] = AppendPathName (importAsset.guidFolder, AssetServer::kAssetStream);
			
				if (IsFileCreated(AppendPathName (importAsset.guidFolder, AssetServer::kTxtMetaStream)))
					streams[AssetServer::kTxtMetaStream] = AppendPathName (importAsset.guidFolder, AssetServer::kTxtMetaStream);
				if (IsFileCreated(AppendPathName (importAsset.guidFolder, AssetServer::kBinMetaStream)))
					streams[AssetServer::kBinMetaStream] = AppendPathName (importAsset.guidFolder, AssetServer::kBinMetaStream);
				if (IsFileCreated(AppendPathName (importAsset.guidFolder, AssetServer::kAssetPreviewStream)))
					streams[AssetServer::kAssetPreviewStream] = AppendPathName (importAsset.guidFolder, AssetServer::kAssetPreviewStream);

			}
		
			//// Replace the asset if the guid is in the assetdatabase
			if (database->IsAssetAvailable (guid) )
			{
				if(importAsset.isFolder)
					continue; // No need to replace already existing folders
			
				// Only replace the asset if the file is different
				const Asset& asset = database->AssetFromGUID (guid);
				// Setup asset info
				UnityStr currentAssetPath = GetGUIDPersistentManager ().AssetPathNameFromGUID (guid);
			
				AssetServer::Item info;
				info.guid = guid;
				info.parent = asset.parent;
				info.name = GetLastPathNameComponent (currentAssetPath);
				info.changeset = conf.GetWorkingItem(guid).changeset; // Keep original version number, otherwise we get a faux conflict

				// Replace the asset contents
				if (! maint.ReplaceAsset (info, streams))
					ThrowString ("Couldn't replace asset " + currentAssetPath + "\n" + maint.GetLastError());
			
			}
			//// Create the asset with the guid and pathname from the package
			else
			{
				string parentAssetPath = DeleteLastPathNameComponent (importAsset.exportedAssetPath);
				// Create parent folder -- this is required when importing pre 2.0 packages, as they do not contain directories
				if (! CreateDirectoryRecursive (parentAssetPath))
					ThrowString ("Could not create directory for asset");

				UnityGUID parent = GetGUIDPersistentManager ().CreateAsset (parentAssetPath);
			
				// Setup asset info
				AssetServer::Item info;
				info.guid = guid;
				info.parent = parent;
				info.name = GetLastPathNameComponent (importAsset.exportedAssetPath);
				info.changeset = -1;
				info.type = importAsset.isFolder?AssetServer::kTDir:AssetServer::kTText;

				// Create the asset
				if (! maint.CreateAsset (info, streams))
					ThrowString ("Couldn't create asset " + (UnityStr)importAsset.exportedAssetPath + "\n" + maint.GetLastError());
			}
		}

		success = true;
	}
	catch (const UnityStr& error)
	{
		ErrorString ("Error while importing package: " + error);
		UnityBeep ();
	}
	
	// Kill off the unpack folder.
	DeleteFileOrDirectory(kImportPackageFolder);

	maint.UnlockAssets (kForceSynchronousImport);

	return success;
}

void BuildExportPackageAssetList (const set<UnityGUID>* guids, vector<ImportPackageAsset>* assets, bool dependencies, bool includeUnmodified, bool includeNameConflicts)
{
	set<UnityGUID> allGUIDs;
	AssetServer::Controller& maint = AssetServer::Controller::Get();
	AssetServer::Configuration& conf = AssetServer::Configuration::Get();

	if(includeUnmodified)
	{
		allGUIDs = *guids;
	}
	else
	{
		for (set<UnityGUID>::const_iterator i=guids->begin();i != guids->end();i++)
		{
			if(AssetServer::ShouldIgnoreAsset(*i)) // Early out ignore assets
				continue;
			switch( conf.GetWorkingItem(*i).GetStatus() )
			{
				case AssetServer::kClientOnly:
				case AssetServer::kNewLocalVersion: 
				case AssetServer::kRestoredFromTrash: 
				case AssetServer::kConflict: 
					allGUIDs.insert(*i);
					break;
				default: 
					break;
			}
		}
	}

	// Include dependencies
	if (dependencies)
	{
		// Save Assets so dependencies can be read from serialized files.
		AssetInterface::Get().SaveAssets ();
		
		set<UnityGUID> newGUIDs = CollectAllDependencies(allGUIDs);

		if(includeUnmodified)
		{
			allGUIDs.insert(newGUIDs.begin(), newGUIDs.end());
		}
		else
		{
			for (set<UnityGUID>::iterator i=newGUIDs.begin();i != newGUIDs.end();i++)
			{
				// Avoid calling GetWorkingItem and GetStatus if the guid has already been processed
				if(guids->find(*i) != guids->end()) 
					continue;
				if (AssetServer::ShouldIgnoreAsset(*i)) // Early out ignore assets
					continue;
	
				switch( conf.GetWorkingItem(*i).GetStatus() )
				{
					case AssetServer::kClientOnly:
					case AssetServer::kNewLocalVersion: 
					case AssetServer::kRestoredFromTrash: 
					case AssetServer::kConflict: 
						allGUIDs.insert(*i);
						break;
					default: 
						break;
				}
			}
		}
	}
	
	// Include assets that exist locally under a different name but have the same name as a renamed asset on server
	// This is required in order to move the asset away on the server before uploading the conflicting one
	if (includeNameConflicts)
	{
		set<UnityGUID> moreGUIDs;
		for(set<UnityGUID>::iterator i = allGUIDs.begin(); i != allGUIDs.end(); i++)
		{
			UnityGUID conflict = conf.GetPathNameConflict(*i);
			if ( conflict != UnityGUID()  )
			{
				if(includeUnmodified)
				{
					moreGUIDs.insert(conflict);
				}
				else if ( allGUIDs.find(conflict) == allGUIDs.end() && guids->find(conflict) == guids->end() )
				{ // Avoid calling GetWorkingItem and GetStatus if the guid has already been processed
					switch( conf.GetWorkingItem(*i).GetStatus() ) {
						case AssetServer::kClientOnly:
						case AssetServer::kNewLocalVersion: 
						case AssetServer::kRestoredFromTrash: 
						case AssetServer::kConflict: 
							moreGUIDs.insert(*i);
							break;
						default: 
							break;
					}
				
				}
			}
		}
		allGUIDs.insert(moreGUIDs.begin(), moreGUIDs.end());

	}
	
	// In maint client, ensure that parent folders are always included in candidate list if they don't exist on server
	if(!includeUnmodified)
	{
		// Add parent folders so they will get added if non-existing or modified;
		set<UnityGUID> parents;
		for(set<UnityGUID>::iterator i = allGUIDs.begin(); i != allGUIDs.end(); i++)
		{
			for (UnityGUID parent = maint.GetAssetParent(*i); parent != UnityGUID(); parent=maint.GetAssetParent(parent) )
			{
				if(parent == UnityGUID (0,0,1,0) || allGUIDs.find(parent) != allGUIDs.end() || parents.find(parent) != parents.end())
					break;
				
				AssetServer::Item working = conf.GetWorkingItem(parent);
				if(!dependencies && working.changeset > 0 ) break;
				
				switch( conf.GetWorkingItem(parent).GetStatus() )
				{
					case AssetServer::kClientOnly:
					case AssetServer::kNewLocalVersion: 
					case AssetServer::kRestoredFromTrash: 
					case AssetServer::kConflict: 
						parents.insert(parent);
						break;
					default: 
						break;
				}
					
			}
		}
		allGUIDs.insert(parents.begin(), parents.end());
	}

	// Build assets list
	assets->clear();
	for (set<UnityGUID>::iterator i=allGUIDs.begin();i != allGUIDs.end();i++)
	{

		ImportPackageAsset asset;
		asset.guid = *i;
		asset.enabled = true;
		
		string assetPath = GetAssetPathFromGUID(*i);
		string parentAssetPath = DeleteLastPathNameComponent (assetPath);
		
		asset.parentGuid = UnityGUID();
		if(parentAssetPath != "")
			GetGUIDPersistentManager ().PathNameToGUID(parentAssetPath, &asset.parentGuid);
			
		asset.message = DeleteFirstPathNameComponent(assetPath);
		assets->push_back(asset);
	}
	
	std::sort(assets->begin(), assets->end());
}

const char* kExportPackageProgressTitle = "Exporting Package";

LocalIdentifierInFileType GetLocalIdentifierOfMainAsset (const UnityGUID& guid)
{
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset == NULL)
		return 0;
	
	return GetPersistentManager().GetLocalFileID(asset->mainRepresentation.object.GetInstanceID());
}

void ExportPackage (const set<UnityGUID>& guids, const std::string& packagePathName, bool forceBatchMode)
{
	bool batchMode = forceBatchMode || IsBatchmode();
	AssetServer::Controller& maint = AssetServer::Controller::Get ();

	// Make sure we don't have any leftovers in the
	// our temporary export folder.
	DeleteFileOrDirectory(kExportFolder);
	
	maint.LockAssets ();
	
	try
	{
		if (!CreateDirectoryRecursive (kExportFolder))
			ThrowString ("Couldn't create temporary directory");
		
		if ( !batchMode ) 
			DisplayProgressbar (kExportPackageProgressTitle, "Gathering files...", 0.1F);
		set<UnityGUID>::const_iterator i;
		for (i=guids.begin ();i != guids.end ();i++)
		{
			// Get asset, metadata path
			AssetServer::StreamMap streams = maint.GetAssetStreams (*i, true);
			AssertIf (streams.find (AssetServer::kAssetStream) == streams.end () || streams.find (AssetServer::kBinMetaStream) == streams.end ());
			string assetPath = streams.find (AssetServer::kAssetStream)->second;
			string metaDataPath = streams.find (AssetServer::kBinMetaStream)->second;
			
			bool isFolder = IsDirectoryCreated (assetPath);
				
			// Create the folder we will store the asset in
			string destFolder = AppendPathName (kExportFolder, GUIDToString (*i));
			if (!CreateDirectory (destFolder))
				ThrowString ("Couldn't create asset folder");

			if(!isFolder) {
				// Copy asset into export folder
				if (!CopyFileOrDirectory (assetPath, AppendPathName (destFolder, AssetServer::kAssetStream)))
					ThrowString ("Couldn't copy asset");
			
				// Copy metadata into export folder
				if (!CopyFileOrDirectory (metaDataPath, AppendPathName (destFolder, AssetServer::kBinMetaStream)))
					ThrowString ("Couldn't copy metadata");
				
				// Create preview image file
				dynamic_array<UInt8> imageData;
				if (ExtractAssetPreviewImage (metaDataPath, GetLocalIdentifierOfMainAsset(*i), imageData))
				{
					if (!WriteBytesToFile(imageData.begin(), imageData.size(), AppendPathName (destFolder, AssetServer::kAssetPreviewStream)))
						ThrowString ("Couldn't copy asset preview");
				}
			}
			
			// Create pathname file
			string pathnameFilePath = AppendPathName (destFolder, kAssetInformationFile);
			if (!CreateFile (pathnameFilePath, 'TEXT', 'TEXT'))
				ThrowString ("Couldn't create pathname file");
			
			// Write asset information file
			// Format:
			// pathName\n
			// hexadecimal digest [32 characters]
			int fileSize = assetPath.size () + 1 + 32;
			MdFour digest = AssetServerCache::Get().FindCachedDigest(*i);
			string guidString = '\n'+ MdFourToString(digest);
			File pathNameFile;
			if (!pathNameFile.Open(pathnameFilePath, File::kWritePermission))
				ThrowString ("Couldn't open output file");

			if (!pathNameFile.SetFileLength (fileSize))
				ThrowString ("Couldn't set file length");

			if (!pathNameFile.Write (&assetPath[0], assetPath.size ()) ||
				!pathNameFile.Write (&guidString[0], guidString.size ()))
				ThrowString ("Couldn't write to file");

			if (!pathNameFile.Close())
				ThrowString ("Couldn't close file");
		}
		
		if ( !batchMode ) 
			DisplayProgressbar (kExportPackageProgressTitle, "Compressing package...", 0.5F);
		CompressPackageTarGZAndDeleteFolder (kExportFolder, packagePathName, !batchMode);
	}
	catch (const UnityStr& error)
	{
		ClearProgressbar ();
		ErrorString ("Error while exporting package: " + error);
		UnityBeep ();
	}

	maint.UnlockAssets (kForceSynchronousImport);
}

bool ImportPackageNoGUI (const std::string& packagePath)
{
	vector<ImportPackageAsset> assets;
	string dummy;
	if (!ImportPackageStep1 (packagePath, assets, dummy))
		return false;	
	return ImportPackageStep2(assets);
}

bool ImportPackageGUI( const std::string& packagePath )
{
	vector<ImportPackageAsset> assets;
	string packageIconPath;
	if (ImportPackageStep1 (packagePath, assets, packageIconPath))
	{
		ShowImportPackageGUI(assets, packageIconPath);
		return true;
	}
	else
	{
		return false;
	}
}

// See http://www.gzip.org/zlib/rfc-gzip.html#header-trailer for info on how gzip files are supposed to look like
// Note: all multibyte fields are little-endian.
struct GzipHeader 
{
	UInt8 id1,id2; // Magic header: always 0x1F, 0x8B
	UInt8 compressionMethod; // Compression method. 8 is "deflate", 0-7 is reserved
	UInt8 isTextFile : 1; 
	UInt8 hasCrc32 : 1;
	UInt8 hasExtraFields : 1;
	UInt8 hasOrigFileName : 1;
	UInt8 hasCommentField : 1;
	UInt8 reserved : 3;
	UInt8 mtime[4];
	UInt8 compressionLevel;
	UInt8 os;
};

struct GzipHeaderExtraField
{
	UInt8 si1, si2; // ID for the extra flag. The asset store uses 'U','$' (jsonData) and 'U','%' (pngIcon)
	UInt16 length; // The length of the data (note: max 64k - if the data is larger, it will be spilt into multiple extra fields and should be concatenated)
};

#define READ_CHECKED(dst, size) \
	if ( fileLength < filePos +size){ \
		fh.Close(); \
		return; \
	} \
	filePos += fh.Read(filePos, dst, size);

PackageInfo::PackageInfo(const std::string& path) 
	: jsonInfo("")
	, packagePath(path)
{
	int filePos=0;
	int fileLength=0;
	GzipHeader header;
	File fh;

	if ( ! fh.Open(packagePath, File::kReadPermission) )
		return; // Could not read file
	fileLength = fh.GetFileLength();

	READ_CHECKED( (void *)&header, sizeof(GzipHeader) );
	
	if ( header.id1 != 0x1F || header.id2 != 0x8B) {
		fh.Close();
		return; // File is not a valid gzip file
	}
	
	UInt16 extraSize=0;
	if ( header.hasExtraFields ) {
		READ_CHECKED( (void *)&extraSize, sizeof(UInt16));
		SwapEndianBytesLittleToNative(extraSize);
	}
	
	while ( extraSize >  sizeof(GzipHeaderExtraField) )
	{
		GzipHeaderExtraField extra;
		READ_CHECKED( (void *)&extra, sizeof(GzipHeaderExtraField));
		extraSize -= sizeof(GzipHeaderExtraField);
		SwapEndianBytesLittleToNative(extra.length);
		
		if ( extraSize < extra.length )
			break;
		
		void* buffer = NULL;
		if ( extra.si1 == 'A' )
		{
			if ( extra.si2 == '$' ) // "A$" -- Asset Store info as a block of JSON
			{
				size_t currentLen = jsonInfo.size();
				jsonInfo.resize(currentLen + extra.length);
				buffer = (void*)&(jsonInfo[currentLen]);
			}
			else if ( extra.si2 == '%' ) // "A%" -- Binary PNG image, 128x128. Used as the Asset Store icon 
			{
				size_t currentLen = pngIcon.size();
				pngIcon.resize(currentLen + extra.length);
				buffer = (void*)&(pngIcon[currentLen]);
			}
		}
		if (buffer)
		{
			READ_CHECKED( buffer, extra.length);
		}
		else 
		{
			filePos += extra.length;
		}
		
		extraSize -= extra.length;

	}
	
	fh.Close();
	
	//printf_console("JSON data for %s: %s\n", packagePath.c_str(), jsonInfo.c_str());
	
}

PackageInfo::~PackageInfo() 
{
	
}

void GetPackageLocations(vector<string>& result, bool includeStandard, bool include3rdPlace, bool includeTargetSpecific)
{
	result.clear();

	if (includeStandard)
	{
		std::string packageLocation = IsDeveloperBuild() ? "../../External/Standard_Packages" : "Standard Packages";
		result.push_back (AppendPathName (GetApplicationFolder (), packageLocation));
	}
	if (include3rdPlace)
	{
		string userData = GetUserAppDataFolder ();
		if (!userData.empty())
			result.push_back (AppendPathName (userData, "Asset Store"));
	}
	if (includeTargetSpecific)
	{
		for (int i = 0; i < kBuildPlayerTypeCount; ++i)
		{
			const BuildTargetPlatform plat = (BuildTargetPlatform)i;
			if (IsBuildTargetSupported(plat))
			{
				std::string dir = GetPlaybackEngineDirectory(plat, 0, false);
				result.push_back(AppendPathName(dir, "Packages"));
			}
		}
	}
	return ;
}

void GetPackageList(vector<PackageInfo>& packages, bool includeStandard, bool include3rdPlace, bool includeTargetSpecific)
{
	vector<string> paths;
	GetPackageLocations(paths, includeStandard, include3rdPlace, includeTargetSpecific);
	std::queue<string> q;
	for ( vector<string>::iterator location = paths.begin(); location != paths.end() ; ++location) 
		q.push(*location);
		
	packages.clear();
	for (; !q.empty(); q.pop())
	{
		if ( IsDirectoryCreated(q.front()) ) 
		{
			set<string> subpaths;
			GetFolderContentsAtPath(q.front(), subpaths);

			for (set<string>::iterator i=subpaths.begin();i != subpaths.end();i++)
			{
				if ( IsDirectoryCreated(*i) )
				{
					q.push(*i);
					continue;
				}
				if ( !IsFileCreated(*i) || StrICmp (GetPathNameExtension (*i), kUnityPackageExtension) != 0)
					continue;
				
				packages.push_back(PackageInfo(*i));
			}
			

		}
	}
	return ;
}

string PackageInfo::GetIconDataURL()
{
	if( pngIcon.empty() )
		return string("data:image/png;base64,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");
		
	std::string encoded=string();	
	Base64Encode(&pngIcon[0], pngIcon.size(), encoded, 0, string());

	return (string("data:image/png;base64,")+encoded);
}
