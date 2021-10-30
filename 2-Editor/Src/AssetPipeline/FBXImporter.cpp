#include "UnityPrefix.h"
#include "FBXImporter.h"
#include "Runtime/Modules/LoadDylib.h"
#include "CImportMesh.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Allocator/MemoryManager.h"
#include "AssetDatabase.h"
#include "Editor/Platform/Interface/NativeMeshFormatConverter.h"
#include "MeshFormatConverter.h"
#include "Editor/Src/Utility/Analytics.h"

#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#endif


#define DEBUG_FBX_IMPORTER 0

typedef CImportScene* DoImportSceneFunc (CImportSettings* settings);
typedef const char* GetImportErrorFunc ();
typedef const char* GetImportWarningsFunc ();
typedef void CleanupImportFunc (CImportScene* import);

typedef void* (*AllocHandler)(size_t size);
typedef void* (*CallocHandler)(size_t num, size_t size);
typedef void* (*ReallocHandler)(void* p, size_t size);
typedef void (*FreeHandler)(void*);
typedef size_t (*SizeHandler)(void*);
typedef void SetMemManagementFunc(AllocHandler allocHandler, CallocHandler callocHandler, ReallocHandler realloacHandler, FreeHandler freeHandler, SizeHandler sizeHandler);

namespace
{
	bool Contains(const std::string* supported, int count, const std::string& assetPathName)
	{
		std::string extension = GetPathNameExtension (assetPathName);

		for ( int i = 0; i < count; i++ )
		{
			if ( StrICmp( supported[ i ], extension ) == 0 )
				return true;
		}

		return false;
	}
}

bool RequiresExternalImporter (const string& pathName)
{
	const string external[] =
	{
		"mb",
		"ma",
		"max",
		"jas",
		"c4d",
		"blend",
		"lxo"
	};
	const int count = sizeof( external ) / sizeof( external[ 0 ] );
	return Contains(external, count, pathName);
}


static int CanLoadPathName (const string& pathName, int* queue)
{
	*queue = ModelImporter::GetQueueNumber(pathName);
	
	const string supported[] =
	{
		"fbx",
		"mb",
		"ma",
		"max",
		"jas",
		"dae",
		"dxf",
		"obj",
		"c4d",
		"blend",
		"lxo"
	};
	const int count = sizeof( supported ) / sizeof( supported[ 0 ] );
	return Contains(supported, count, pathName);
}

FBXImporter::~FBXImporter ()
{}

void FBXImporter::InitializeClass ()
{
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (FBXImporter), kModelImporterVersion);	
}

static const char* kExportedCheetahFBXFile = "Temp/ExportedCheetahFBXFile.fbx";
//static const char* kExportedColladaFile = "Temp/ExportedColladaFile.dae";
static const char* kExportedBlenderFBXFile = "Temp/ExportedBlenderFBXFile.fbx";
//static const char* kExportedModoFBXFile = "Temp/ExportedModoFBXFile.fbx";

bool FBXImporter::IsUseFileUnitsSupported() const
{
	const std::string extension = GetPathNameExtension (GetAssetPathName());
	return ToLower(extension) == "max";
}

bool FBXImporter::IsBakeIKSupported ()
{
	const string supported[] =
	{
		"mb",
		"ma",
		"max",
		"c4d",
	};
	const int count = sizeof( supported ) / sizeof( supported[ 0 ] );	
	return Contains(supported, count, GetAssetPathName());
}

bool FBXImporter::IsTangentImportSupported() const 
{
	const string unsupported[] =
	{
		"c4d",
		"blend",
		"jas",
		"obj",
		"lxo"

		//"jas",
		//"dae",
		//"dxf",		
	};
	const int count = sizeof( unsupported ) / sizeof( unsupported[ 0 ] );
	return !Contains(unsupported, count, GetAssetPathName());
}

/*
static string ConvertBlenderToCollada (const string& sourceFile, string* file)
{
	// should this function be implemented on windows at all? its not used anymore?
	#if UNITY_WIN

	return "Not implemented yet";

	#else
	CreateDirectory ("Temp");
	DeleteFile (kExportedColladaFile);

	string blenderPath = GetDefaultApplicationForFile (sourceFile);
	if (blenderPath.empty ())
		return "Blender could not be found.\nMake sure that Blender is installed and the .blend file has Blender as its 'Open with' application!";

	blenderPath = AppendPathName (blenderPath, "Contents/MacOS/blender");
	string output;
	string toolsFolder = AppendPathName(GetApplicationContentsPath(), "Tools");

	setenv("UNITY_BLENDER_EXPORTER_OUTPUT_FILE", PathToAbsolutePath(kExportedColladaFile).c_str(), 1);
	setenv("UNITY_BLENDER_EXPORT_TOOLS_FOLDER", PathToAbsolutePath(toolsFolder).c_str(), 1);
	
	LaunchTask (blenderPath, &output, "-b", sourceFile.c_str (), "-P", AppendPathName(toolsFolder, "Unity-BlenderToCollada.py").c_str(), NULL);

	if (!IsFileCreated (kExportedColladaFile))
	{
		return "Blender could not convert the .blend file to Collada file.\n"
	       "Try exporting the blender file to Collada file manually.\n";
           "You need to use Blender 2.40 or higher for direct blender import to work.\n";
	}

	*file = kExportedColladaFile;
	return "";
	#endif
}
*/

static string ConvertBlenderToFBX (const string& sourceFile, string* file)
{
	CreateDirectory ("Temp");
	DeleteFile (kExportedBlenderFBXFile);

#if UNITY_LINUX
	 // No reliable way to get default app
	string blenderPath = "blender";
#else
	string blenderPath = GetDefaultApplicationForFile (sourceFile);
	if (blenderPath.empty ())
		return "Blender could not be found.\nMake sure that Blender is installed and the .blend file has Blender as its 'Open with' application!";
#endif

	string output;
	string toolsFolder = AppendPathName(GetApplicationContentsPath(), "Tools");

	#if UNITY_OSX
	blenderPath = AppendPathName (blenderPath, "Contents/MacOS/blender");

	setenv("UNITY_BLENDER_EXPORTER_OUTPUT_FILE", PathToAbsolutePath(kExportedBlenderFBXFile).c_str(), 1);
	setenv("UNITY_BLENDER_EXPORT_TOOLS_FOLDER", PathToAbsolutePath(toolsFolder).c_str(), 1);
	
	LaunchTask (blenderPath, &output, "-b", sourceFile.c_str (), "-P", AppendPathName(toolsFolder, "Unity-BlenderToFBX.py").c_str(), NULL);

	#elif UNITY_WIN

	string str = "UNITY_BLENDER_EXPORTER_OUTPUT_FILE=" + PathToAbsolutePath(kExportedBlenderFBXFile);
	ConvertSeparatorsToWindows(str);
	_putenv(str.c_str());

	vector<string> args;
	args.push_back("-b");
	args.push_back(sourceFile);
	args.push_back("-P");
	args.push_back(AppendPathName(toolsFolder, "Unity-BlenderToFBX.py").c_str());

	LaunchTaskArray(blenderPath, &output, args, true, blenderPath.substr(0, blenderPath.find_last_of('/')));

	#elif UNITY_LINUX
	setenv("UNITY_BLENDER_EXPORTER_OUTPUT_FILE", PathToAbsolutePath(kExportedBlenderFBXFile).c_str(), 1);
	setenv("UNITY_BLENDER_EXPORT_TOOLS_FOLDER", PathToAbsolutePath(toolsFolder).c_str(), 1);

	if (!LaunchTask (blenderPath, &output, "-b", sourceFile.c_str (), "-P", AppendPathName(toolsFolder, "Unity-BlenderToFBX.py").c_str(), NULL))
		return "Blender could not be found.\nMake sure that Blender is installed and in your PATH.";
	#else
	#error "Unknown platform"
	#endif

	if (!IsFileCreated (kExportedBlenderFBXFile))
	{
		return "Blender could not convert the .blend file to FBX file.\n"
           "You need to use Blender 2.45-2.49 or 2.58 and later versions for direct Blender import to work.";
	}

	*file = kExportedBlenderFBXFile;
	return "";
}

static string ConvertJASToFBX (const string& jasFile, string* file)
{
	#if UNITY_WIN

	return "Cheetah 3D '.jas' file importing is not supported on Windows.";

	#else

	CreateDirectory ("Temp");
	DeleteFile (kExportedCheetahFBXFile);

	string cheetahPath = GetDefaultApplicationForFile (jasFile);
	if (cheetahPath.empty ())
		return "Cheetah 3D could not be found.\nMake sure that Cheetah 3D is installed and the .jas file has Cheetah 3D as its 'Open with' application!";

	cheetahPath = AppendPathName (cheetahPath, "Contents/MacOS/Cheetah3D");
	string output;
	LaunchTask (cheetahPath, &output, "-b", jasFile.c_str (), PathToAbsolutePath (kExportedCheetahFBXFile).c_str (), NULL);
	if (!IsFileCreated (kExportedCheetahFBXFile))
	{
		return "Cheetah3D couldn't convert the jas file to an fbx file.\n"
	       "Try exporting the jas file to an fbx file manually.\n"
           "You need to use Cheetah3D 2.4.1 or higher for direct jas import to work.\n";
	}

	*file = kExportedCheetahFBXFile;
	return "";
	#endif
}



string FBXImporter::ConvertToFBXFile ()
{
	string extension = ToLower (GetPathNameExtension (GetAssetPathName ()));
	string absoluteAssetPath = PathToAbsolutePath (GetAssetPathName ());
	string file;

	// Convert maya binary file to fbx file!
	if (extension == "mb" || extension == "ma")
	{
		// Launch maya and convert to fbx. Sometimes maya is unrobust and crashes.
		// In that case we try one more time then give up.
		string error = ConvertMayaToFBX (*this, absoluteAssetPath, &file, 0);
		if (file.empty ())
			error = ConvertMayaToFBX (*this, absoluteAssetPath, &file, 1);
		
		if (file.empty ())
			LogImportError (error);
		else
			LogImportWarning (error);
			
		return file;
	}
	else if ( extension == "max" )
	{
		#if UNITY_WIN
		const bool useFileUnits = GetUseFileUnits();
		// Launch 3dsMax and convert to fbx. Sometimes 3dsMax is unrobust and crashes.
		// In that case we try one more time then give up.
		string error = ConvertMaxToFBX (*this, absoluteAssetPath, &file, 0, useFileUnits);
		//if (file.empty ())
		//	error = ConvertMaxToFBX (*this, absoluteAssetPath, &file, 1, useFileUnits);
		
		if (file.empty ())
			LogImportError (error);
		else
			LogImportWarning (error);
			
		return file;
		#else
		LogImportError ("3ds '.max' file importing is not supported on OS X.\nPlease export to FBX in 3ds Max instead.");
		return "";
		#endif
	}
	else if (extension == "c4d")
	{
		// Launch Cinema4D and convert to fbx. 
		string error = ConvertC4DToFBX (*this, absoluteAssetPath, &file);
		
		if (file.empty ())
			LogImportError (error);
		else
			LogImportWarning (error);
			
		return file;
	}
	else if (extension == "jas")
	{
		string error = ConvertJASToFBX (absoluteAssetPath, &file);

		if (file.empty ())
			LogImportError (error);
		else
			LogImportWarning (error);
			
		return file;
	}
	else if (extension == "blend")
	{
		string error;
//#if !DEBUG_FBX_IMPORTER
//		if (m_FirstImportVersion == 1)
//		{
//			error = ConvertBlenderToCollada (absoluteAssetPath, &file);
//		}
//		else
//#endif
		{
			error = ConvertBlenderToFBX (absoluteAssetPath, &file);
		}
		if (file.empty ())
			LogImportError (error);
		else
			LogImportWarning (error);
			
		return file;
	}
	else if (extension == "lxo")
	{
		string error = ConvertModoToFBXSDKReadableFormat(absoluteAssetPath, &file);
		
		if (file.empty ())
			LogImportError (error);
		else
			LogImportWarning (error);
		
		return file;
	}
	else
		return GetAssetPathName ();
}

namespace 
{
	struct ConversionApplicationInfo 
	{
		ConversionApplicationInfo(const std::string& applicationName_, const std::string& applicationDetailedName_)
		:	applicationName(applicationName_), applicationDetailedName(applicationDetailedName_)
		{
		}

		std::string applicationName;
		std::string applicationDetailedName;
	};
	
	typedef std::map<std::string, ConversionApplicationInfo> ExtensionToConversionApplicationMap;

	const ExtensionToConversionApplicationMap GetExtensionToConversionApplicationMap()
	{
		ExtensionToConversionApplicationMap m;

		m.insert(std::make_pair("ma",		ConversionApplicationInfo("Maya", "Maya-ma")));
		m.insert(std::make_pair("mb",		ConversionApplicationInfo("Maya", "Maya-mb")));
		m.insert(std::make_pair("blend",	ConversionApplicationInfo("Blender", "Blender")));
		m.insert(std::make_pair("jas",		ConversionApplicationInfo("Cheetah3D", "Cheetah3D")));
		m.insert(std::make_pair("max",		ConversionApplicationInfo("3ds Max", "3ds Max")));		
		m.insert(std::make_pair("c4d",		ConversionApplicationInfo("Cinema4D", "Cinema4D")));
		m.insert(std::make_pair("lxo",		ConversionApplicationInfo("Modo", "Modo")));

		m.insert(std::make_pair("fbx",		ConversionApplicationInfo("FBXSDK", "FBXSDK-fbx")));
		m.insert(std::make_pair("obj",		ConversionApplicationInfo("FBXSDK", "FBXSDK-obj")));
		m.insert(std::make_pair("dae",		ConversionApplicationInfo("FBXSDK", "FBXSDK-dae")));
		m.insert(std::make_pair("dxf",		ConversionApplicationInfo("FBXSDK", "FBXSDK-dxf")));

		// This should never be used, but we have it here for consistency
		m.insert(std::make_pair("3ds", ConversionApplicationInfo("3dsImporter", "3dsImporter")));

		return m;
	}

	const ExtensionToConversionApplicationMap kExtensionToConversionApplicationMap = GetExtensionToConversionApplicationMap();

	void SendAnalytics(const ImportSceneInfo& originalInfo, const std::string& extension)
	{
		ImportSceneInfo info = originalInfo;

		std::string cathegory = "FBXImporter";
		bool sendStats = true;

		if (extension != "fbx")
		{
			// TODO : rename to FBXImporter-Conversion?
			cathegory = "FBXImporterConversion";
			if (!info.hasApplicationName)
			{
				ExtensionToConversionApplicationMap::const_iterator it = kExtensionToConversionApplicationMap.find(extension);
				if (it == kExtensionToConversionApplicationMap.end())
				{
					ErrorStringMsg("Failed to find conversion application for extension %s", extension.c_str());
					sendStats = false;
				}
				else
				{
					info.applicationName = it->second.applicationName;
					info.applicationDetailedName = it->second.applicationDetailedName;
				}
			}
		}

		if (sendStats)
			AnalyticsTrackEvent(cathegory, info.applicationName, info.applicationDetailedName + " " + info.exporterInfo, 1);
	}
}

void* FBXAllocate(size_t size)
{
	return UNITY_MALLOC(kMemFBXImporter, size);
}

void* FBXCalloc(size_t num, size_t size)
{
	return UNITY_CALLOC(kMemFBXImporter, num, size);
}

void* FBXRealloc(void* p, size_t size)
{
	return UNITY_REALLOC_(kMemFBXImporter, p, size);
}

void FBXFree(void* p)
{
	UNITY_FREE(kMemFBXImporter, p);
}

size_t FBXMemSize(void* p)
{
	return GetMemoryManager().GetAllocator(kMemFBXImporter)->GetPtrSize(p);
}


/// The fbx libraries don't come as codewarrior / mach-o so we need to build it with xcode
/// In xcode we create a dylib which generates the import scene. and converts it into a c style import scene.
bool FBXImporter::DoMeshImport (ImportScene& outputScene)
{
	DoImportSceneFunc* DoImportScene;
	GetImportErrorFunc* GetImportError;
	GetImportWarningsFunc* GetImportWarnings;
	CleanupImportFunc* CleanupImport;
	SetMemManagementFunc* SetMemManagement;

	// FBX always has inverted winding
	m_MeshSettings.invertWinding = true;
	
	// Cinema 4d fbx doesnt export framerate correctly. We get it from the convert plugin instead!
	m_OverrideSampleRate = -1;

	string fbxFile = ConvertToFBXFile ();
	if (fbxFile.empty ())
		return false;
	
#if UNITY_OSX
	string dylibPath = AppendPathName (GetApplicationContentsPath (), "Tools/ImportFBX.dylib");
#elif UNITY_WIN
	string dylibPath = AppendPathName (GetApplicationContentsPath (), "Tools/ImportFBX.dll");
#elif UNITY_LINUX
	string dylibPath = AppendPathName (GetApplicationContentsPath (), "Tools/ImportFBX.so");
#else
	#error "Unknown platform"
#endif
		
	if (!LoadAndLookupSymbols( dylibPath.c_str (), 
									"DoImportScene", &DoImportScene,
									"GetImportError", &GetImportError,
									"GetImportWarnings", &GetImportWarnings,
									"CleanupImport", &CleanupImport,
									"SetMemManagement", &SetMemManagement,
									NULL ))
	{
		LogImportError ("Failed loading fbx importer");
		return false;
	}
	
	CImportSettings settings;
	string absolutePath = PathToAbsolutePath (fbxFile);
	settings.absolutePath = absolutePath.c_str ();
	string originalExt = ToLower( GetPathNameExtension( GetAssetPathName() ) );
	settings.originalExtension = originalExt.c_str();
	settings.importBlendShapes = m_ImportBlendShapes;

	settings.importAnimations = ShouldImportAnimations();
	settings.importSkinMesh = ShouldImportSkinnedMesh(); 
	// TODO@MECANIM validate new conditions to adjust clips by time rnage
	settings.adjustClipsByTimeRange = ShouldAdjustClipsByDeprecatedTimeRange();
	settings.importNormals = m_MeshSettings.normalImportMode == kTangentSpaceOptionsImport;
	settings.importTangents = m_MeshSettings.tangentImportMode == kTangentSpaceOptionsImport;

	SetMemManagement(FBXAllocate, FBXCalloc, FBXRealloc, FBXFree, FBXMemSize);

	// Run the dynamic library
	CImportScene* importScene = DoImportScene (&settings);
	
	// Log errors
	string error = GetImportError ();
	if (!error.empty ())
	{
		if (importScene == NULL)
			LogImportError (error);
		else
			LogImportError (error);	
	}
	
	string warnings = GetImportWarnings();
	if ( !warnings.empty() )
		LogImportWarning( warnings );
	
	if (importScene == NULL)
		return false;

	CImportSceneToImportScene (*importScene, outputScene);
	CleanupImport (importScene);
	
	if (m_OverrideSampleRate != -1)
		outputScene.sampleRate = m_OverrideSampleRate;

	SendAnalytics(outputScene.sceneInfo, ToLower(GetPathNameExtension(GetAssetPathName())));
		
	#if DEBUG_FBX_IMPORTER
	UnloadDynamicLibrary (dylibPath);
	#endif
	
	return true;	
}

IMPLEMENT_CLASS_HAS_INIT (FBXImporter)
