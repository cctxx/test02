#include "UnityPrefix.h"
#include "Runtime/Serialize/TransferFunctions/YAMLRead.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Utilities/File.h"

UnityGUID ReadGUIDFromTextMetaData( const string& pathName )
{
	string textMetaData = GetTextMetaDataPathFromAssetPath (pathName);
	UnityGUID result = UnityGUID();
	
 	File file;
	if (file.Open (textMetaData, File::kReadPermission, File::kSilentReturnOnOpenFail))
	{
		// Fast path for reading the guid
		const int kReadBuffer = 256;
		char header[kReadBuffer+1];
		const char* kGUIDYamlTag = "\nguid: ";
		
		int read = file.Read(header, kReadBuffer);
		if (read > 0)
		{
			header[kReadBuffer] = '\0';
			const char* guid = strstr(header, kGUIDYamlTag);
			if (guid != NULL)
			{
				guid += strlen(kGUIDYamlTag);
				if (const char* end = strstr(guid, "\n"))
				{
					// We may also be reading CRLF file, unfortunately
					if (end - guid >= 1 && end[-1] == '\r')
						--end;
					if (end - guid == 32)
						result = StringToGUID(string(guid, 32));
				}
			}
		}
		file.Close();
		
		// Fallback to using the YAML parser to find the guid
		if (result == UnityGUID())
		{
			printf_console("Non-Standard formatted meta data file, slow loading path '%s'\n", pathName.c_str());
			
			TEMP_STRING metaDataString;
			ReadStringFromFile (&metaDataString, textMetaData);
			
			YAMLRead read (metaDataString.c_str(), metaDataString.size(), 0);
			read.Transfer(result, "guid");
		}
	}
	
	return result;
}

bool IsTextMetaDataForFolder( const std::string& pathName )
{
	InputString str;
	string fullMetaPath = GetTextMetaDataPathFromAssetPath(PathToAbsolutePath(pathName));
	ReadStringFromFile(&str, fullMetaPath);
	return str.find("folderAsset: yes") != string::npos;
}