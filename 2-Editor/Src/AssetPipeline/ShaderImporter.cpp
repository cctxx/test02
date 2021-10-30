#include "UnityPrefix.h"
#include "ShaderImporter.h"
#include "AssetDatabase.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "External/shaderlab/Library/ShaderLabErrors.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Shaders/ShaderSupportScripts.h"
#include "Runtime/Shaders/Shader.h"
#include "Editor/Src/EditorHelper.h"
#include "AssetInterface.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Editor/Src/LicenseInfo.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Editor/Src/File/FileScanning.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "External/shaderlab/Library/SLParserData.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Scripting/Scripting.h"

// Increase if you want all shaders to be reimported / upgraded.
// It does not matter what is the exact version, as long as it's just different
// from the version before!
//
// Use current date in YYYYMMDD0 (last digit dummy) format for the version.
// If conflicts merging, just enter current date and increase last digit.
#define UNITY_SHADER_IMPORT_VERSION 201309251


// Shader upgrade actually modifies shader files in the project.
// Define to 0 to disable upgrade and file modification.

#define ENABLE_SHADER_UPGRADE_WHEN_UPGRADING_PROJECT 1
#define ENABLE_SHADER_UPGRADE_ON_IMPORT 1


#if UNITY_WIN
const char* kShaderCompilerPath = "Tools/CgBatch.exe";
const char* kShaderFixorPath = "Tools/ShaderFixor.exe";
#elif UNITY_OSX || UNITY_LINUX
const char* kShaderCompilerPath = "Tools/CgBatch";
const char* kShaderFixorPath = "Tools/ShaderFixor";
#else
#error "Unknown platform"
#endif


static bool ProcessShader (TextAsset& shader, const std::string& inputSource, const std::string& pathName, ShaderErrors& outErrors);
static int CanLoadPathName (const string& pathName, int* queue);


static std::string GetShaderFixorPath()
{
	return AppendPathName (GetApplicationContentsPath(), kShaderFixorPath);
}

static bool UpgradeShader (const std::string& inputPath)
{
	const char* kOutputFile = "Temp/ShaderFixorOutput.shader";
	DeleteFile( kOutputFile );

	std::vector<std::string> args;
	args.push_back (inputPath);
	args.push_back (kOutputFile);

	std::string output;
	const string fixorPath = GetShaderFixorPath();
	LaunchTaskArray (fixorPath, &output, args, true);

	InputString result;
	if( !ReadStringFromFile (&result, kOutputFile) )
		return false; // no upgrade was necessary

	printf_console ("Upgraded shader: %s\n", inputPath.c_str());
	WriteStringToFile (std::string(result.c_str(),result.size()), inputPath, kNotAtomic, 0);

	return true;
}

bool UpgradeShadersIfNeeded (bool willReimportWholeProject)
{
	bool needToUpgradeShaders = false;

	#if ENABLE_SHADER_UPGRADE_ON_IMPORT
	if (willReimportWholeProject)
		needToUpgradeShaders = true;
	#endif

	///@TODO: Do we really need this? This is an ugly hack since it is part of version control!

	#if ENABLE_SHADER_UPGRADE_WHEN_UPGRADING_PROJECT

	if (AssetDatabase::Get().GetShaderImportVersion() != UNITY_SHADER_IMPORT_VERSION)
	{
		printf_console ("Shader import version has changed; will reimport all shaders...\n");
		AssetDatabase::Get().SetShaderImportVersion(UNITY_SHADER_IMPORT_VERSION);
		needToUpgradeShaders = true;
	}
	#endif

	if (!needToUpgradeShaders)
		return false;

	printf_console ("Upgrading shader files ...");

	ABSOLUTE_TIME shaderUpgradeStartTime = START_TIME;

	// Get all files in Assets folder
	dynamic_array<DirectoryEntry> entries;
	string directoryPath = "Assets";
	GetDirectoryContents (directoryPath, entries, kDeepSearch);
	directoryPath.reserve (4096);
	for (int i=0;i<entries.size();i++)
	{
		UpdatePathDirectoryOnly(entries[i], directoryPath);

		int queue;
		if (!CanLoadPathName (entries[i].name, &queue))
			continue;

		string path = AppendPathName(directoryPath, entries[i].name);
		//printf_console ("Verify shader upgrade ... %s\n", path.c_str());

		UpgradeShader (path);
	}

	printf_console("%f seconds.\n", GetElapsedTimeInSeconds(shaderUpgradeStartTime));

	return true;
}

static bool s_NeedToReloadShaders = false;

void UpdateShaderAsset (Shader* shader, const std::string& source)
{
	Assert(shader);
	bool success = ProcessShader (*shader, source, "", shader->GetErrors());
	Assert(success);
	GetScriptMapper().AddShader (*shader);
	
	AssetDatabase::Get().SetAssetsChangedGraphically ();
}

Shader* CreateShaderAsset (const std::string& source)
{
	Shader* shader = CreateObjectFromCode<Shader>(kInstantiateOrCreateFromCodeAwakeFromLoad);
	DebugAssert(shader);
	if (!shader)
		return NULL;
	
	UpdateShaderAsset (shader, source);

	return shader;
}

IMPLEMENT_CLASS_HAS_INIT (ShaderImporter);
IMPLEMENT_OBJECT_SERIALIZE (ShaderImporter);

ShaderImporter::ShaderImporter(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

ShaderImporter::~ShaderImporter()
{
}


void ShaderImporter::ReloadAllShadersAfterImport ()
{
	if (!s_NeedToReloadShaders)
		return;

	Shader::ReloadAllShaders();

	std::vector<SInt32> allShaders;
	Object::FindAllDerivedObjects (ClassID (Shader), &allShaders);
	for (std::vector<SInt32>::iterator i = allShaders.begin(); i != allShaders.end(); ++i)
	{
		Shader *s = PPtr<Shader> (*i);
		if (!s)
			continue;
		s->GetErrors().LogErrors (s->GetName(), s->GetNamedObjectName(), s, s->GetInstanceID());
	}

	s_NeedToReloadShaders = false;
}


static int CanLoadPathName (const string& pathName, int* queue)
{
	const char* ext = GetPathNameExtension (pathName.c_str(), pathName.size());
	if (StrICmp(ext, "cginc") == 0)
	{
		*queue = -1002; // Import include files first, since they will import shaders that are dependent on them
		return true;
	}
	else if (StrICmp(ext, "shader") == 0)
	{
		*queue = -1001; // Import shaders before native assets in order to make material previews look correct
		return true;
	}
	else
		return false;
}


void ShaderImporter::InitializeClass ()
{
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (ShaderImporter), UNITY_SHADER_IMPORT_VERSION);
}


static void ReimportShadersInFolder (const std::string& folder)
{
	set<string> paths;
	GetFolderContentsAtPath(folder, paths);

	for (set<string>::iterator i=paths.begin();i != paths.end();i++)
	{
		const string& path = *i;
		if (StrICmp(GetPathNameExtension(path), "shader") == 0)
		{
			AssetInterface::Get().ImportAtPath(path);
		}
	}
}


void ShaderImporter::GenerateAssetData ()
{
	const string pathName = GetAssetPathName ();
	const string absolutePath = PathToAbsolutePath(pathName);
	const string ext = ToLower (GetPathNameExtension (pathName));

	// Remove console messages from old shader
	TextAsset* oldScript = GetFirstDerivedObjectNamed<TextAsset> (GetLastPathNameComponent (pathName));
	if (oldScript)
		RemoveErrorWithIdentifierFromConsole (oldScript->GetInstanceID ());

	#if ENABLE_SHADER_UPGRADE_ON_IMPORT
	// Just always do shader upgrades. Fixes cases of:
	// * User drags 2.x shader into 3.0 project
	// * User creates a new shader in 3.0, and pastes some existing 2.x shader code in
	UpgradeShader (pathName);
	#endif


	// read script file contents
	InputString contents;
	if (!ReadStringFromFile (&contents, pathName))
	{
		LogImportError ("Couldn't read shader script file!");
		return;
	}

	// create and produce
	TextAsset  *script = NULL;
	bool success = false;
	if (ext == "shader")
	{
		s_NeedToReloadShaders = true;
		Shader *shader = &RecycleExistingAssetObject<Shader> ();

		// invalidate any cached custom property drawers for this shader
		void* params[] = { Scripting::ScriptingWrapperFor(shader) };
		CallStaticMonoMethod("MaterialPropertyDrawer","InvalidatePropertyCache", params);

		script = shader;
		success = ProcessShader( *script, std::string(contents.c_str(), contents.size()), pathName, shader->GetErrors() );
		GetScriptMapper().AddShader (*shader);

		// Assign default textures to shader.. only do this for ones that exist
		map<UnityStr, PPtr<Texture> > defaultTextures;
		for (int j =0; j < shader->GetPropertyCount(); j++)
		{
			const ShaderLab::ParserProperty* shaderProperty = shader->GetPropertyInfo(j);
			if (shaderProperty->m_Type != ShaderLab::ParserProperty::kTexture)
				continue;

			// find the default texture with this name...
			std::vector<std::pair<UnityStr, PPtr<Texture> > >::iterator defaultTexIterator = m_DefaultTextures.begin();

			while ( defaultTexIterator != m_DefaultTextures.end() && defaultTexIterator->first != shaderProperty->m_Name)
				++defaultTexIterator;

			if (defaultTexIterator == m_DefaultTextures.end())
				continue;

			defaultTextures[defaultTexIterator->first] = defaultTexIterator->second;
		}
		shader->SetDefaultTextures (defaultTextures);
	}
	else if (ext == "cginc")
	{
		CGProgram *cgp = &RecycleExistingAssetObject<CGProgram> ();
		cgp->SetCGProgram (absolutePath);
		script = cgp;
		success = true;

		// For the time being, import all shaders in the same folder as the include file.
		// This is not really correct (We should have proper dependency tracking for include files instead)
		// But this covers the most common use case
		ReimportShadersInFolder (DeleteLastPathNameComponent(pathName));
	}

	if (WarnInconsistentLineEndings(contents.c_str()))
		AddImportError (Format("There are inconsistent line endings in the '%s' script. Some are Mac OS X (UNIX) and some are Windows.\nThis might lead to incorrect line numbers in stacktraces and compiler errors. Many text editors can fix this using Convert Line Endings menu commands.", pathName.c_str()), pathName.c_str(), -1, kLog | kAssetImportWarning, script);

	if (script)
	{
		script->AwakeFromLoad(kDefaultAwakeFromLoad);
		AssetDatabase::Get().SetAssetsChangedGraphically ();
	}
}

void ShaderImporter::SetDefaultTextures (const std::vector<std::string>& name, const std::vector<Texture* >&  target)
{
	m_DefaultTextures.resize(name.size());
	for (int i=0;i<m_DefaultTextures.size();i++)
	{
		m_DefaultTextures[i].first = name[i];
		m_DefaultTextures[i].second = target[i];
	}
	SetDirty();
}

PPtr<Texture> ShaderImporter::GetDefaultTexture (const std::string& name)
{
	for (int i=0;i<m_DefaultTextures.size();i++)
	{
		if (m_DefaultTextures[i].first  == name)
			return m_DefaultTextures[i].second;
	}

	return NULL;
}

void ShaderImporter::UnloadObjectsAfterImport (UnityGUID guid)
{
	// Intentionally left blank.
	// Shaders are not unloaded after import, to be able to report their errors
	// to the console (that needs to happen after all reimports reparse all shaders)
}


template<class TransferFunction>
void ShaderImporter::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TRANSFER(m_DefaultTextures);
	PostTransfer (transfer);
}

static bool CheckShaderCompilerResult (const string& output, bool& hadErrors, ShaderErrors& outErrors)
{
	// the output is lines; at least one line when successful
	hadErrors = true;
	bool ok = true;

	size_t outputSize = output.size();
	int startPos = 0;
	while (true)
	{
		// read the line
		size_t lineEnd = startPos;
		while (lineEnd < outputSize && !(output[lineEnd]=='\r' || output[lineEnd]=='\n') )
			++lineEnd;
		if (lineEnd == startPos)
			break;
		string line = output.substr( startPos, lineEnd - startPos );
		startPos += line.size();

		// skip white space
		while (startPos < outputSize && IsSpace(output[startPos]))
			++startPos;

		// parse the line
		if( line.size() < 6 || (line[0] != 'w' && line[0] != 'E' && line[0] != 'R') || line[1] != ' ' ) // minimal line is like 'E 0: s'
			continue;

		int colonPos = line.find( ':', 0 );
		if( colonPos == string::npos )
			continue;
		int numberPos = colonPos-1;
		while( numberPos >= 0 && (isdigit(line[numberPos]) || line[numberPos]=='-') )
			--numberPos;
		int lineNumber = StringToInt( line.substr( numberPos, colonPos-numberPos ) );
		string messageFull = line.substr( colonPos+2, line.size()-(colonPos+2) );

		string message, messageDetails;

		size_t newLinePos = messageFull.find("\\n");
		if (newLinePos != string::npos)
		{
			message = messageFull.substr(0, newLinePos);
			messageDetails = messageFull.substr(newLinePos + 2);
		}
		else
		{
			message = messageFull;
		}

		if( line[0] == 'w' )
		{
			outErrors.AddShaderError (message, messageDetails, lineNumber, true, true);
			ok = false;
		}
		else if( line[0] == 'E' )
		{
			outErrors.AddShaderError (message, messageDetails, lineNumber, false, true);
			ok = false;
		}
		else if( line[0] == 'R' )
		{
			hadErrors = false;
		}
	}

	return ok && !hadErrors;
}

static std::string GetShaderCompilerPath()
{
	return AppendPathName (GetApplicationContentsPath(), kShaderCompilerPath);
}

static bool ProcessShaderPrograms (TextAsset& shader, std::string& source, ShaderErrors& outErrors)
{
	// optimization: if we don't have CGPROGRAM or GLSLPROGRAM anywhere, don't do anything
	int beginCg = source.find ("CGPROGRAM");
	int beginGLSL = source.find ("GLSLPROGRAM");
	if( beginCg == string::npos && beginGLSL == string::npos )
		return true;

	string assetPath = GetPersistentManager().GetPathName(shader.GetInstanceID());
	assetPath = GetGUIDPersistentManager().AssetPathNameFromAnySerializedPath( assetPath );
	const string includePath = ResolveSymlinks( AppendPathName( GetApplicationContentsPath (), "CGIncludes" ) );
	const string compilerPath = GetShaderCompilerPath();

	const char* kInputFile = "Temp/CgBatchInput.shader";
	const char* kOutputFile = "Temp/CgBatchOutput.shader";

	if (!WriteStringToFile( source, kInputFile, kProjectTempFolder, kFileFlagDontIndex | kFileFlagTemporary ))
	{
		ErrorString( "Failed writing temporary file during shader compilation!" );
		return false;
	}
	DeleteFile( kOutputFile );

	std::vector<std::string> args;
	args.push_back( kInputFile );
	args.push_back( DeleteLastPathNameComponent(assetPath) );
	args.push_back( includePath );
	args.push_back( kOutputFile );

	bool developerBuild = IsDeveloperBuild (false);

	if (developerBuild || IsBuildTargetSupported(kBuildMetroPlayerX86)
		#if INCLUDE_WP8SUPPORT
		|| IsBuildTargetSupported(kBuildWP8Player)
		#endif
		)
		args.push_back( "-d3d11_9x" );

	std::string output;
	LaunchTaskArray (compilerPath, &output, args, true);
	bool hadErrors = false;
	bool ok = CheckShaderCompilerResult (output, hadErrors, outErrors);

	// Even if shader had errors, still use processed text
	InputString result;
	if (!ReadStringFromFile (&result, kOutputFile))
	{
		outErrors.AddShaderError ("Shader compilation produced no results (compiler crashed?)", -2, false, true);
		return false;
	}
	source = std::string(result.c_str(), result.size());

	return ok;
}



static bool ProcessShader (TextAsset& shader, const std::string& inputSource, const std::string& pathName, ShaderErrors& outErrors)
{
	outErrors.Clear();

	string source = inputSource;
	bool ok = ProcessShaderPrograms (shader, source, outErrors);

	return shader.SetScript(source) && ok;
}


#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (ShaderImporterTests)
{
	TEST (ShaderCompilerAvailable)
	{
		string path = GetShaderCompilerPath ();
		CHECK (IsFileCreated (path));
	}
	TEST (ShaderFixorAvailable)
	{
		string path = GetShaderFixorPath ();
		CHECK (IsFileCreated (path));
	}

	// We had a bug introduced when shader compiler's output was not printing to stdout anymore
	// (it was printing to Player Log!). Successfully compiled shaders were all fine, but shader
	// errors were just not being reported.
	TEST (ShaderCompilerPrintsToStdout)
	{
		string path = GetShaderCompilerPath ();
		string src = "CGPROGRAM\n and no end tag should result in error\n";
		string inFile = "testshaderin.shader";
		string outFile = "testshaderout.shader";
		CHECK(WriteStringToFile(src, inFile, kNotAtomic, kFileFlagDontIndex | kFileFlagTemporary));

		std::vector<string> args;
		args.push_back(inFile);
		args.push_back(".");
		args.push_back(".");
		args.push_back(outFile);

		std::string output;
		LaunchTaskArray (GetShaderCompilerPath(), &output, args, true);

		ShaderErrors errors;
		bool hadErrors = false;
		CheckShaderCompilerResult (output, hadErrors, errors);
		DeleteFile(inFile);
		DeleteFile(outFile);

		// We should have errors!
		CHECK(hadErrors);
		CHECK(errors.HasErrors());
	}
}

#endif // ENABLE_UNIT_TESTS
