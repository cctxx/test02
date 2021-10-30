#ifndef ASSETDATABASE_STRUCTS_H
#define ASSETDATABASE_STRUCTS_H

#include "Runtime/BaseClasses/EditorExtension.h"
#include "AssetLabels.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/GUID.h"
#include "MdFourGenerator.h"
#include "Runtime/Utilities/DateTime.h"
#include "Runtime/Utilities/vector_set.h"

enum
{
	// An importer imports the data from the asset path (eg. texture) and converts it into a serialized representation.
	// Nothing can be modified in the inspector and it will only be saved after a reimport.
	// Imported assets, e.g fbx file.
	kCopyAsset = 1,
	// A serialized asset, that is modified in the editor and stored in the assets path.
	// Automatically saved with the project when saving.
	// Unity native assets, e.g material.
	kSerializedAsset = 2,
	// This has folder semantics (No extra data, may contain children)
	kFolderAsset = 4
};

enum UpdateAssetOptions
{
	/// The update was forced by the user. He clicked on Reimport.
	/// vs. the update might have been caused by an import because the modification date changed.
	kForceUpdate = 1 << 0,
	
	/// All asset importing must be done synchronously.
	/// Eg. scripts need to be compiled and only then will other importing continue.
	/// This is important so that when downloading packages and then importing a prefab,
	/// the prefab will be serialized with the newly compiled assembly. Otherwise we might lose data.
	kForceSynchronousImport = 1 << 3,
	/// The next import will have the same priority.
	/// This is used by the MonoImporter to detect that we can now start compiling assets, because now no more scripts are going to come in during this reimport
	kNextImportHasSamePriority = 1 << 4,
	/// Import may be cancelled.
	/// You may cancel import settings only
	/// if the user brought up the import dialog via menu item for an existing asset
	kMayCancelImport = 1 << 7,
	/// Should reimport be applied recursively
	kImportRecursive = 1 << 8,
	/// Set if the asset (or its .meta file) was modified on disk. Used as a hint to NativeAssetImporter to unload any assets stored in memory before reimporting them.
	kAssetWasModifiedOnDisk = 1 << 9,
	/// Set if the asset is up-to-date, but we only need to either create or remove the text based .meta file.
	kRefreshTextMetaFile = 1 << 10,
	// Importing asset based on automatic Refresh reimport
	kImportAssetThroughRefresh = 1 << 11,
	// Forces rewriting of the text meta file. Use to write GUID changes to disk.
	kForceRewriteTextMetaFile = 1 << 12,
	// Forces rewriting of the text meta file. Use to write GUID changes to disk.
	kDontImportWithCacheServer = 1 << 13,
	// Forces asset import as uncompressed for edition facilities.
	kForceUncompressedImport = 1 << 14,
	// Set this flag to all AssetInterface::Refresh to switch to syncronous import if both script and other asset type needs refreshing
	kAllowForceSynchronousImport = 1 << 15,
};




struct LibraryRepresentation
{
	enum
	{
		kHasFullPreviewImage = (1<<0),
		kAssetHasErrors = (1<<1),
	};

	// Note: A library representation of all assets in the project
	// is always kept in memory -> try to keep this class small!

	UnityStr              name;
	Image                 thumbnail;
	PPtr<Object>          object; //@TODO: This should be stored as LocalIdentifierInFile. InstanceID can be reconstructed from it. This way there is no need for it to have all instanceID's mapped during import time.
	SInt16                classID;
	UInt16				  flags;
	UnityStr              scriptClassName;
	
	friend bool operator == (const LibraryRepresentation& lhs, const LibraryRepresentation& rhs);
	
	DECLARE_SERIALIZE (LibraryRepresentation)
	
	LibraryRepresentation ()
	{
		classID = 0;
		flags = 0;
	}
};

struct AssetTimeStamp
{
	AssetTimeStamp();
	AssetTimeStamp( const DateTime& iModDate, const DateTime& iMetaModDate ) ;
	
	DateTime   modificationDate;
	DateTime   metaModificationDate;
	UInt8      refreshFlags;

	enum
	{
		kRefreshFoundAssetFile = 1 << 0,
		kRefreshFoundMetaFile = 1 << 1, 
		kRefreshFoundHiddenMetaFile = 1 << 2
	};
	
	friend bool operator == (const AssetTimeStamp& lhs, const AssetTimeStamp& rhs);
	
	DECLARE_SERIALIZE (AssetTimeStamp)
};

// MD4 Hash of 1 - first 4 bytes
enum { kDefaultImporterVersionHash = 1804873070 };

struct Asset
{
	Asset () 
		: type(-1), 
		labels(), 
		importerClassId(-1), 
		importerVersionHash(kDefaultImporterVersionHash), 
		hash()
	{}

	UnityGUID                          parent;

	// The main representation of the asset shown in the project window. eg name, icon, classID
	LibraryRepresentation              mainRepresentation;
	// One layer of sub-objects in the project window. (eg. Mesh in a model importer)
	std::vector<LibraryRepresentation> representations;
	// If the asset is a folder it can have children
	std::vector<UnityGUID>             children;

	SInt32                             type;
	
	AssetLabels                        labels;
	
	SInt32                             importerClassId;
	UInt32                             importerVersionHash;

	MdFour                             hash;
	
	friend bool operator == (const Asset& lhs, const Asset& rhs);
	
	friend bool CompareLibraryRepChanged (const Asset& lhs, const Asset& rhs);
	
	DECLARE_SERIALIZE (Asset)
};

typedef std::map<UnityGUID, Asset>                             Assets;
typedef std::map<int, UInt32>                             AssetImporterVersionHash;
typedef std::map<UnityStr, MdFour>                             AssetHashes;

template<class TransferFunction>
inline void Asset::Transfer (TransferFunction& transfer)
{
	TRANSFER (mainRepresentation);
	TRANSFER (representations);
	
	TRANSFER (children);
	TRANSFER (parent);

	TRANSFER (type);
	TRANSFER (labels);
	TRANSFER (importerClassId);
	TRANSFER (importerVersionHash);
	TRANSFER (hash);
}

template<class TransferFunction>
inline void AssetTimeStamp::Transfer (TransferFunction& transfer)
{
	UInt32* modDate = reinterpret_cast<UInt32*> (&modificationDate);
	UInt32* metaModDate = reinterpret_cast<UInt32*> (&metaModificationDate);
	
	transfer.Transfer (modDate[0], "modificationDate[0]");
	transfer.Transfer (modDate[1], "modificationDate[1]");
	transfer.Transfer (metaModDate[0], "metaModificationDate[0]");
	transfer.Transfer (metaModDate[1], "metaModificationDate[1]");
}

inline AssetTimeStamp::AssetTimeStamp() : 
modificationDate(), 
metaModificationDate(),
refreshFlags(0)
{}

inline AssetTimeStamp::AssetTimeStamp( const DateTime& iModDate, const DateTime& iMetaModDate) : 
modificationDate(iModDate),
metaModificationDate(iMetaModDate),
refreshFlags(0)
{}

template<class TransferFunction>
inline void LibraryRepresentation::Transfer (TransferFunction& transfer)
{
	TRANSFER (name);
	TRANSFER (thumbnail);
	TRANSFER (object);
	transfer.Transfer (classID, "thumbnailClassID");
	transfer.Transfer (flags, "flags");
	transfer.Transfer (scriptClassName, "scriptClassName");
}


#endif
