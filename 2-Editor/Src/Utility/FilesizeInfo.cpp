#include "UnityPrefix.h"
#include "FilesizeInfo.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Utilities/FileUtilities.h"
using namespace std;

inline void SumBytesForClass (const string& className, map<int, int>& bytesPerClass, int& bytes, int& bytes2)
{
	int classID = Object::StringToClassID (className);
	AssertIf (classID == -1);
	vector<SInt32> classes;
	Object::FindAllDerivedClasses (classID, &classes);
	for (int i=0;i<classes.size ();i++)
	{
		bytes += bytesPerClass[classes[i]];
		bytes2 += bytesPerClass[classes[i]];
	}
}

inline bool ExtractFileSize (const string& path, map<int, int> & bytesUsedPerClass, int& outByteSize, int& outOverhead, int& outTotalSize, const TemporaryAssetLookup& assetLookup, map<string, int>& assetSize)
{
	vector<LocalIdentifierInFileType> fileIDs;
	GetPersistentManager().Lock();
	
	std::string absoluteFilePath = GetPersistentManager().RemapToAbsolutePath (path);
	
	SerializedFile* file = UNITY_NEW(SerializedFile,kMemTempAlloc);
	ResourceImageGroup group;
	if (!file->InitializeRead (absoluteFilePath, group, 4096, 2, kSerializeGameRelease))
	{
		UNITY_DELETE(file,kMemTempAlloc);
		GetPersistentManager().Unlock();
		return NULL;
	}
	
	file->GetAllFileIDs (&fileIDs);
	int fileSize = GetFileLength (absoluteFilePath);
	int totalUsedBytes = 0;

	for (int i=0;i<fileIDs.size ();i++)
	{
		int classID = file->GetClassID (fileIDs[i]);
		int byteSize = file->GetByteSize (fileIDs[i]);
		bytesUsedPerClass[classID] += byteSize;
		totalUsedBytes += byteSize;

		TemporaryAssetLookup::const_iterator found = assetLookup.find(make_pair(path, fileIDs[i]));
		if (found != assetLookup.end())
		{
			assetSize[found->second] += byteSize;
		}
	}
	
	outOverhead += fileSize - totalUsedBytes;
	outByteSize += totalUsedBytes;
	outTotalSize += fileSize;
	UNITY_DELETE(file,kMemTempAlloc);
	GetPersistentManager().Unlock();
	return true;
}


void FileSizeInfo::AddAssetFileSize (const string& path, const TemporaryAssetLookup& assetLookup)
{
	map<int, int> bytesPerClass;
	int usedAssetFileSize = 0;
	ExtractFileSize (path, bytesPerClass, usedAssetFileSize, m_SerializationOverhead, m_TotalSize, assetLookup, m_IncludedAssetSizes);
	
	int total = 0;
	
	SumBytesForClass ("Texture", bytesPerClass, m_Textures, total);
	SumBytesForClass ("Mesh", bytesPerClass, m_Meshes, total);
	SumBytesForClass ("AudioClip", bytesPerClass, m_Sounds, total);

	SumBytesForClass ("AnimationClip", bytesPerClass, m_Animations, total);

	SumBytesForClass ("MonoScript", bytesPerClass, m_Scripts, total);

	SumBytesForClass ("Shader", bytesPerClass, m_Shaders, total);
	SumBytesForClass ("CGProgram", bytesPerClass, m_Shaders, total);
	
	m_OtherAssets += usedAssetFileSize - total;
}

void FileSizeInfo::AddLevelFileSize (const string& path)
{
	map<string, int> temp;
	map<int, int> bytesPerClass;
	if (!ExtractFileSize (path, bytesPerClass, m_Levels, m_SerializationOverhead, m_TotalSize, TemporaryAssetLookup(), temp))
		m_Valid = false;
}

void FileSizeInfo::AddDependencyDll (const string& path)
{
	m_DllDependencies += GetFileLength(path);
	m_TotalSize += GetFileLength(path);
}

void FileSizeInfo::AddScriptDll (const string& path)
{
	m_Scripts += GetFileLength(path);
	m_TotalSize += GetFileLength(path);
}

inline void PrintSizeLine (const char* name, const char* post, int size, int totalSize)
{
	float percent = (float)size / (float)totalSize * 100.0F;
	float kilobytes = size / 1024.0F;
	if (kilobytes < 1024.0F)
		printf_console ("%s %.1f kb\t %.1f%% %s\n", name, kilobytes, percent, post);
	else
	{
		float megabytes = kilobytes / 1024.0F;
		printf_console ("%s %.1f mb\t %.1f%% %s\n", name, megabytes, percent, post);
	}
}

void FileSizeInfo::Print ()
{
	int dataSize = m_Textures + m_Sounds + m_Meshes + m_Animations + m_OtherAssets + m_Levels + m_Shaders + m_SerializationOverhead + m_Scripts + m_DllDependencies;
	AssertIf (dataSize != m_TotalSize);

	if (m_Valid)
	{
		printf_console ("\n");

		PrintSizeLine ("Textures     ", "", m_Textures, m_TotalSize);
		PrintSizeLine ("Meshes       ", "", m_Meshes, m_TotalSize);
		PrintSizeLine ("Animations   ", "", m_Animations, m_TotalSize);
		PrintSizeLine ("Sounds       ", "", m_Sounds, m_TotalSize);
		PrintSizeLine ("Shaders      ", "", m_Shaders, m_TotalSize);
		PrintSizeLine ("Other Assets ", "", m_OtherAssets, m_TotalSize);
		PrintSizeLine ("Levels       ", "", m_Levels, m_TotalSize);
		PrintSizeLine ("Scripts      ", "", m_Scripts, m_TotalSize);
		PrintSizeLine ("Included DLLs", "", m_DllDependencies, m_TotalSize);
		PrintSizeLine ("File headers ", "", m_SerializationOverhead, m_TotalSize);
		PrintSizeLine ("Complete size", "", m_TotalSize, m_TotalSize);

		printf_console ("\n");
		
		// Sort by assets size
		multimap<int, string> sortedIncludedAssets;
		for (std::map<std::string, int>::iterator i=m_IncludedAssetSizes.begin ();i != m_IncludedAssetSizes.end ();i++)
		{
			sortedIncludedAssets.insert(make_pair(i->second, i->first));
		}

		printf_console ("Used Assets, sorted by uncompressed size:\n");
		for (multimap<int, string>::reverse_iterator i=sortedIncludedAssets.rbegin ();i != sortedIncludedAssets.rend ();i++)
		{
			PrintSizeLine("", i->second.c_str(), i->first, m_TotalSize);
		}
	}
	else
	{
		printf_console ("Player size statistics calculation failed because a file couldn't be read!\n");
	}
}
