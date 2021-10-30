#include "UnityPrefix.h"
#include "ProceduralMaterial.h"
#include "SubstanceSystem.h"
#include "Runtime/Misc/CachingManager.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Graphics/Image.h"

#if ENABLE_SUBSTANCE && ENABLE_CACHING && !UNITY_EDITOR
bool ProceduralMaterial::PreProcess(std::set<unsigned int>& cachedTextureIDs)
{
	if (m_LoadingBehavior!=ProceduralLoadingBehavior_Cache)
		return false;

	std::string cacheFolder = GetCacheFolder();
	std::vector<string> fileNames;
	CachingManager::ReadInfoFile(cacheFolder, NULL, &fileNames);
	if (fileNames.size()==0)
		return false;

	bool success = true;
	std::map<ProceduralTexture*, SubstanceTexture> cachedTextures;
	for (PingedTextures::iterator it=m_PingedTextures.begin();it!=m_PingedTextures.end() ; ++it)
	{
		if (*it!=NULL)
		{
			std::string fileName;
			if (ReadCachedTexture(fileName, cachedTextures, cacheFolder, **it))
			{
				cachedTextureIDs.insert((*it)->GetSubstanceBaseTextureUID());
			}
			else
			{
				success = false;
				DeleteFileOrDirectory(fileName);
				std::vector<string>::iterator it = std::find(fileNames.begin(), fileNames.end(), fileName);
				if (it!=fileNames.end())
					fileNames.erase(it);
			}
		}
	}
	GetSubstanceSystem().ForceSubstanceResults(cachedTextures);

	if (fileNames.size()==0)
	{
		DeleteFileOrDirectory(cacheFolder);
		return false;
	}

	time_t timestamp = CachingManager::GenerateTimeStamp();
	CachingManager::WriteInfoFile(cacheFolder, fileNames, timestamp);
	GetCachingManager().GetCurrentCache().UpdateTimestamp(cacheFolder, timestamp);

	if (success)
		m_LoadingBehavior = ProceduralLoadingBehavior_Generate;
	return success;
}

void ProceduralMaterial::PostProcess(const std::map<ProceduralTexture*, SubstanceTexture>& textures, const std::set<unsigned int>& cachedTextureIDs)
{
	if (m_LoadingBehavior!=ProceduralLoadingBehavior_Cache)
		return;
	m_LoadingBehavior = ProceduralLoadingBehavior_Generate;

	std::string cacheFolder = GetCacheFolder();
	std::vector<string> fileNames;
	CachingManager::ReadInfoFile(cacheFolder, NULL, &fileNames);
	for (std::map<ProceduralTexture*, SubstanceTexture>::const_iterator it=textures.begin() ; it!=textures.end() ; ++it)
	{
		if (it->first!=NULL && cachedTextureIDs.find(it->first->GetSubstanceBaseTextureUID())==cachedTextureIDs.end())
		{
			std::string fileName;
			bool result = WriteCachedTexture(fileName, cacheFolder, *it->first, it->second);
			std::vector<string>::iterator it = std::find(fileNames.begin(), fileNames.end(), fileName);
			if (result)
			{
				if (it==fileNames.end())
					fileNames.push_back(fileName);
			}
			else
			{
				DeleteFileOrDirectory(fileName);
				if (it!=fileNames.end())
					fileNames.erase(it);
			}
		}
	}
	if (fileNames.size()==0)
	{
		DeleteFileOrDirectory(cacheFolder);
	}
	else
	{
		GetCachingManager().WriteInfoFile(cacheFolder, fileNames);
	}
}

std::string ProceduralMaterial::GetCacheFolder() const
{
	std::string hash;
	char hashValue[6];
	for (int i=0 ; i<16 ; ++i)
	{
		snprintf(hashValue, 6, "%d", (int)m_Hash.hashData.bytes[i]);
		hash += hashValue;
	}
	return GetCachingManager().GetCurrentCache().GetFolder(hash.c_str(), true);
}

std::string ProceduralMaterial::GetCacheFilename(const ProceduralTexture& texture) const
{
	char filename[24];
	snprintf(filename, 24, "%u.cache", texture.GetSubstanceBaseTextureUID());
	return filename;
}

bool ProceduralMaterial::ReadCachedTexture(string& fileName, std::map<ProceduralTexture*, SubstanceTexture>& cachedTextures, const std::string& folder, const ProceduralTexture& texture)
{
	fileName = folder+"/"+GetCacheFilename(texture);
	File file;
	if (!file.Open(fileName, File::kReadPermission, File::kSilentReturnOnOpenFail))
		return false;
	unsigned int infoSize = sizeof(SubstanceTexture);
	unsigned int textureSize;
	SubstanceTexture& data = cachedTextures[(ProceduralTexture*)&texture];
	bool result = file.Read(&data, infoSize)
		&& file.Read(&textureSize, 4)
		&& (data.buffer=SubstanceSystem::OnMalloc(textureSize))!=NULL
		&& file.Read(data.buffer, textureSize);
	file.Close();
	return result;
}

bool ProceduralMaterial::WriteCachedTexture(string& fileName, const std::string& folder, const ProceduralTexture& texture, const SubstanceTexture& data)
{
	fileName = folder+"/"+GetCacheFilename(texture);
	if (IsFileCreated(fileName) && texture.HasBeenUploaded())
		return true;
	unsigned int infoSize = sizeof(SubstanceTexture);
	int mipLevels = data.mipmapCount;
	if (mipLevels==0)
		mipLevels = CalculateMipMapCount3D(data.level0Width, data.level0Height, 1);
	unsigned int textureSize = CalculateMipMapOffset(data.level0Width, data.level0Height, texture.GetSubstanceFormat(), mipLevels+1);
	if (!GetCachingManager().GetCurrentCache().FreeSpace(infoSize+4+textureSize))
		return false;
	File file;
	if (!file.Open(fileName, File::kWritePermission, File::kSilentReturnOnOpenFail))
		return false;
	bool result = file.Write(&data, infoSize)
		&& file.Write(&textureSize, 4)
		&& file.Write(data.buffer, textureSize);
	file.Close();
	return result;
}
#endif
