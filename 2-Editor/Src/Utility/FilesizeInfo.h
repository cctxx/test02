#pragma once

#include <map>
#include "Configuration/UnityConfigure.h"

typedef std::map<std::pair<std::string, LocalIdentifierInFileType>, std::string> TemporaryAssetLookup;

struct FileSizeInfo
{
	int m_Textures;
	int m_Meshes;
	int m_Animations;
	int m_Sounds;
	int m_Shaders;
	int m_OtherAssets;
	int m_DllDependencies;
	int m_Scripts;
	int m_Levels;
	
	int m_SerializationOverhead;
	int m_PlayerSize;
	int m_TotalSize;
	bool m_Valid;

	std::map<std::string, int> m_IncludedAssetSizes;

	FileSizeInfo () { m_Valid = true; m_Textures = m_Meshes = m_Animations = m_Sounds = m_Shaders = m_OtherAssets = m_DllDependencies = m_Scripts = m_Levels = m_SerializationOverhead = m_PlayerSize = m_TotalSize = 0; }
	
	void AddAssetFileSize (const std::string& path, const TemporaryAssetLookup& assets);
	void AddLevelFileSize (const std::string& path);
	void AddPlayerSize (const std::string& path);
	void AddDependencyDll (const std::string& path);
	void AddScriptDll (const std::string& path);
	
	void Print ();
};