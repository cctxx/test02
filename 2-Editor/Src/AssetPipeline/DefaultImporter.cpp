#include "UnityPrefix.h"
#include "DefaultImporter.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Graphics/Image.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/GUID.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"


static const char* kBuildPrefsIconName = "GameManager Icon";
static Image* gBuildPrefsImage = NULL;
using namespace std;

// Increasing the version number causes an automatic reimport when the last imported version is a different.
// Only increase this number if an older version of the importer generated bad data or the importer applies data optimizations that can only be done in the importer.
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
enum { kDefaultImporterVersion = 5 };



static int DefaultImporterCanLoadPathName (const string& pathName, int* queue)
{
	// Default assets are imported first in order to make folders import first.
	// Folders need to be imported in the right order. (Root folders first (lower queue priority))
	// We use the queue index for that.
	int depth = count (pathName.begin (), pathName.end (), kPathNameSeparator);
	*queue = -10000000 + depth;
	if (IsDirectoryCreated(pathName))
		return kFolderAsset;
	else
		return kCopyAsset;
}

static int DefaultImporterCanLoadPathNameFolder (const string& pathName, int* queue)
{
	// Default assets are imported first in order to make folders import first.
	// Folders need to be imported in the right order. (Root folders first (lower queue priority))
	// We use the queue index for that.
	int depth = count (pathName.begin (), pathName.end (), kPathNameSeparator);
	*queue = -10000000 + depth;
	if (IsDirectoryCreated(pathName))
		return kFolderAsset;
	else
		return 0;
}

//IgnoreSpecializedFoldersForImport("Assets/Plugins/Test");

static int IgnoreSpecializedFoldersForImport (const string& pathName, int* queue)
{
	// webplayer template contain source .js files, not to be compiled!
	if (ToLower(pathName).find("assets/webplayertemplates") == 0)
		return DefaultImporterCanLoadPathName(pathName, queue);

	vector<std::string> path = FindSeparatedPathComponents(pathName, '/');
	if (path.size() < 5 /* "Assets/Plugins/<platform>/*../*../" */ ||
		StrICmp(path[0].c_str(), "Assets") != 0 ||
		StrICmp(path[1].c_str(), "Plugins")!= 0)
		return 0;
	
	for (BuildTargetPlatform target = kBuildNoTargetPlatform; target < kBuildPlayerTypeCount; target = (BuildTargetPlatform)(target + 1))
	{
		string platform = GetBuildTargetShortName(target);
		if (platform.empty())
			continue;
		if (StrICmp(path[2].c_str(), platform.c_str()) == 0)
			return DefaultImporterCanLoadPathName(pathName, queue);
	}
	
	return 0;
}

void DefaultImporter::InitializeClass ()
{
	// Folders must be verified first
	AssetImporter::RegisterCanLoadPathNameCallback (DefaultImporterCanLoadPathNameFolder, Object::StringToClassID ("DefaultImporter"), kDefaultImporterVersion, -1000);

	// Other default assets must be handled last
	AssetImporter::RegisterCanLoadPathNameCallback (IgnoreSpecializedFoldersForImport, Object::StringToClassID ("DefaultImporter"), kDefaultImporterVersion, -999);

	// Other default assets must be handled last
	AssetImporter::RegisterCanLoadPathNameCallback (DefaultImporterCanLoadPathName, Object::StringToClassID ("DefaultImporter"), kDefaultImporterVersion, 1000);
}

DefaultImporter::DefaultImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

DefaultImporter::~DefaultImporter ()
{}


void GenerateDefaultImporterAssets (AssetImporter& assetImporter, bool generateWarningIcon)
{
	string assetPathName = assetImporter.GetAssetPathName();

	if (assetImporter.GetImportFlags() & kAssetWasModifiedOnDisk && GetApplication().GetCurrentScene() == assetPathName)
	{
		GetApplication().OpenSceneChangedOnDisk();
	}

	bool isScene = StrICmp(GetPathNameExtension(assetPathName), "unity") == 0;
	if (isScene)
	{
		// For backwards compatibility with 4.0 we want to generate a fileID for DefaultAsset but create a SceneAsset
		// It used to be:
		// SceneAsset& sceneAsset = assetImporter.ProduceAssetObject<SceneAsset> ();
		SceneAsset& sceneAsset = static_cast<SceneAsset&> (assetImporter.ProduceAssetDeprecated(ClassID(SceneAsset), ClassID(DefaultAsset)));

		sceneAsset.AwakeFromLoad(kDefaultAwakeFromLoad);
	}
	else
	{
		DefaultAsset& defaultAsset = assetImporter.ProduceAssetObject<DefaultAsset> ();
		defaultAsset.AwakeFromLoad(kDefaultAwakeFromLoad);
		
		if (IsDirectoryCreated (assetPathName) && StrICmp(GetPathNameExtension(assetPathName), "bundle") != 0)
			;
		else if (ToLower(assetPathName) == "library/buildplayer.prefs") 
		{
			if (gBuildPrefsImage == NULL)
				gBuildPrefsImage = new Image (GetImageNamed(kBuildPrefsIconName));
			assetImporter.SetThumbnail (*gBuildPrefsImage, defaultAsset.GetInstanceID ());
		}
		else
		{
			string extension = GetPathNameExtension (assetPathName);
			if (!extension.empty())
			{
				const int iconSize = 32;
				Image image = ImageForIconAtPath (assetPathName);
				image.ReformatImage (iconSize, iconSize, kTexFormatRGBA32, ImageReference::BLIT_BILINEAR_SCALE);		
				assetImporter.SetThumbnail (image, defaultAsset.GetInstanceID ());
			}
			else
			{
				// Use our own icon for files without extension by not calling SetThumbnail on the importer
			}
		}	
	}
}


void DefaultImporter::GenerateAssetData ()
{
	GenerateDefaultImporterAssets(*this, false);
}


template<class TransferFunction>
void DefaultImporter::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	PostTransfer (transfer);
}


IMPLEMENT_CLASS_HAS_INIT (DefaultImporter)
IMPLEMENT_OBJECT_SERIALIZE (DefaultImporter)


IMPLEMENT_CLASS (DefaultAsset);

DefaultAsset::DefaultAsset (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

DefaultAsset::~DefaultAsset ()
{}


IMPLEMENT_CLASS (SceneAsset);

SceneAsset::SceneAsset (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

SceneAsset::~SceneAsset ()
{}


