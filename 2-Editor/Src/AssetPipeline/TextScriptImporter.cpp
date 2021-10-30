#include "UnityPrefix.h"
#include "TextScriptImporter.h"
#include "AssetDatabase.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "Runtime/Shaders/Shader.h"
#include "Editor/Platform/Interface/EditorUtility.h"

// Increasing the version number causes an automatic reimport when the last imported version is a different.
// Only increase this number if an older version of the importer generated bad data or the importer applies data optimizations that can only be done in the importer.
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
enum { kTextScriptImporterVersion = 2 };

static int CanLoadPathName (const string& pathName, int* queue)
{
	string ext = ToLower(GetPathNameExtension (pathName));
	return
		!ext.compare("txt") ||
		!ext.compare("html") ||
		!ext.compare("htm") ||
		!ext.compare("xml") ||
		!ext.compare("json") ||
		!ext.compare("csv") ||
		!ext.compare("yaml") ||
		!ext.compare("bytes") ||
		!ext.compare("fnt");
}

void TextScriptImporter::InitializeClass ()
{ 
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (TextScriptImporter), kTextScriptImporterVersion, -20);
}

TextScriptImporter::TextScriptImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

TextScriptImporter::~TextScriptImporter ()
{}

void TextScriptImporter::GenerateAssetData ()
{
	string pathName = GetAssetPathName ();
	string ext = GetPathNameExtension (pathName);
	
	// read script file contents
	InputString tempContents;
	if (!ReadStringFromFile (&tempContents, pathName))
	{
		LogImportError ("File couldn't be read");
		return;
	}
	
	bool containsBinaryData = !ext.compare("bytes");
	TextAsset::ScriptString contents;

	if(!containsBinaryData)
	{
		bool encodingDetected;
		contents = ConvertToUTF8(tempContents.c_str(), tempContents.size(), encodingDetected);

		// if (!encodingDetected) ...
	
		//TextAsset::ScriptString contents (tempContents.begin(), tempContents.end());
	}
	else
		contents.assign(tempContents.c_str(), tempContents.size());

	// create and produce
	TextAsset *newScript = &ProduceAssetObject<TextAsset> ();
	newScript->SetScript (contents, containsBinaryData);
	newScript->AwakeFromLoad(kDefaultAwakeFromLoad);
}

template<class TransferFunction>
void TextScriptImporter::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	PostTransfer (transfer);
}


IMPLEMENT_CLASS_HAS_INIT (TextScriptImporter)
IMPLEMENT_OBJECT_SERIALIZE (TextScriptImporter);
