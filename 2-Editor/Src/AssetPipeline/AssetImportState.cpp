#include "UnityPrefix.h"
#include "AssetImportState.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/PlayerPrefs.h"

static const char* kCompressTexturesOnImport = "kCompressTexturesOnImport";
static const char* kImportStateFile = "Library/AssetImportState";

static std::string ToString(const BuildTargetSelection& selection, int remappedSRGB)
{
	std::string str;
	str += IntToString(selection.platform);
	str += ";";
	str += IntToString(selection.subTarget);
	str += ";";
	str += IntToString(remappedSRGB);
	return str;
}

static void FromString(const std::string& str, BuildTargetSelection& selection, int& remappedSRGB)
{
	selection = BuildTargetSelection::NoTarget();
	remappedSRGB = kRemappedSRGBUndefined;
	
	size_t separatorIndex = str.find(';');
	int value = StringToInt(str.substr(0, separatorIndex));
	if (value >= 0)
		selection.platform = (BuildTargetPlatform)value;
	
	// Subtarget
	if (separatorIndex != std::string::npos)
	{
		int value = StringToInt(str.substr(separatorIndex+1, std::string::npos));
		if (value >= 0)
			selection.subTarget = value;
	}
	
	// linear rendering
	separatorIndex = str.find(';', separatorIndex +1 );
	if (separatorIndex != std::string::npos)
	{
		remappedSRGB = StringToInt(str.substr(separatorIndex+1, std::string::npos));
	}
}

static AssetImportState* gAssetImportState = NULL;

AssetImportState::AssetImportState ()
:	m_SelectionForTarget(kBuildNoTargetPlatform,0)
,   m_RemappedSRGB (kRemappedSRGBUndefined)
{
	InputString data;
	ReadStringFromFile(&data, kImportStateFile);

	FromString(string(data.begin(), data.end()), m_SelectionForTarget, m_RemappedSRGB);
}

void AssetImportState::SetImportedForTarget (BuildTargetSelection selection)
{
	if (m_SelectionForTarget != selection)
	{
		m_SelectionForTarget = selection;
		WriteStateToFile();
	}
}

BuildTargetSelection AssetImportState::GetImportedForTarget ()
{
	return m_SelectionForTarget;
}

void AssetImportState::SetDidImportTextureUncompressed ()
{
	SetImportedForTarget(BuildTargetSelection::NoTarget());
}

void AssetImportState::SetDidImportAssetForTarget (BuildTargetSelection selection)
{
	// When we import an asset for a different build target than the current one 
	// -> we have an inconsistent state, thus we mark the current import state as having been imported for no specific platform.
	//    This will cause rescanning of all assets when making a build.
	if (m_SelectionForTarget != selection)
		SetImportedForTarget(BuildTargetSelection::NoTarget());
}

void AssetImportState::SetDidImportAssetForTarget (BuildTargetPlatform target)
{
	if (m_SelectionForTarget.platform != target)
		SetImportedForTarget(BuildTargetSelection::NoTarget());
}

void AssetImportState::SetDidImportRemappedSRGB (int remappedSRGB)
{
	if (m_RemappedSRGB != remappedSRGB)
	{
		m_RemappedSRGB = remappedSRGB;
		WriteStateToFile();
	}
}

void AssetImportState::WriteStateToFile()
{
	WriteStringToFile(ToString(m_SelectionForTarget, m_RemappedSRGB), kImportStateFile, kProjectTempFolder, kFileFlagDontIndex);
}

void AssetImportState::SetCompressAssetsPreference (bool compress)
{
	EditorPrefs::SetBool(kCompressTexturesOnImport,compress);
}

bool AssetImportState::GetCompressAssetsPreference ()
{
	return EditorPrefs::GetBool(kCompressTexturesOnImport,true);
}

AssetImportState& GetAssetImportState ()
{
	if (gAssetImportState == NULL)
		gAssetImportState = new AssetImportState ();
	return *gAssetImportState;
}
