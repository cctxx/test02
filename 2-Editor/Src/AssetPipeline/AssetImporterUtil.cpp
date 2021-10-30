#include "UnityPrefix.h"
#include "AssetImporterUtil.h"
#include "AudioImporter.h"
#include "TextureImporter.h"
#include "SubstanceImporter.h"
#include "TrueTypeFontImporter.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "AssetDatabase.h"
#include "AssetInterface.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "AssetImportState.h"
#include "Runtime/Misc/PlayerSettings.h"

double GetTimeSinceStartup ();

bool ShouldCheckAllTexturesForRecompression (bool requireCompressedAssets)
{
	// When compress textures is disabled, and we are not about to make a build
	// Allow fast switching without recompressing assets for the target platform
	if (!requireCompressedAssets && !GetAssetImportState().GetCompressAssetsPreference ())
		return false;
	
	BuildTargetSelection selection = GetEditorUserBuildSettings().GetActiveBuildTargetSelection();
	
	// Are all assets already imported for this targetPlatform?
	if (selection == GetAssetImportState().GetImportedForTarget())
		return false;
	
	return true;
}

int GetRemappedSRGB ()
{
	if (GetEditorUserBuildSettings().GetActiveBuildTarget () == kBuildXBOX360 && GetPlayerSettings().GetDesiredColorSpace() == kLinearColorSpace)
		return kBuildXBOX360;
	else
		return kNoRemappedSRGB;
}

bool ShouldCheckTexturesForLinearRendering ()
{
	return GetRemappedSRGB() != GetAssetImportState().GetDidImportRemappedSRGB();
}

/**
 * Check if assets needs to reimport (call this upon startup and when targetplatform changes)
 **/
void VerifyAssetsForBuildTarget(bool requireCompressedAssets, AssetInterface::CancelBehaviour cancelBehaviour)
{
	bool shouldCheckAllTexturesForRecompression = ShouldCheckAllTexturesForRecompression (requireCompressedAssets);
	bool shouldCheckLinearRenderingMode = ShouldCheckTexturesForLinearRendering ();
	
	// Early out if we have nothing to check on a per texture basis.
	if (!shouldCheckAllTexturesForRecompression && !shouldCheckLinearRenderingMode)
		return;

	BuildTargetSelection selection = GetEditorUserBuildSettings().GetActiveBuildTargetSelection();
	BuildTargetPlatform targetPlatform = selection.platform;

	bool wasCompressAssetsEnabled = GetAssetImportState().GetCompressAssetsPreference ();
	GetAssetImportState().SetCompressAssetsPreference (true);
	
	AssetInterface::Get ().Refresh ();
	
	ABSOLUTE_TIME startTime = START_TIME;
	printf_console("Determining assets that need to be reimported for target platform ... ");
	
	set<UnityGUID> all;
	vector<string> assetsNeedReimport;
	
	UnityGUID rootAsset = kAssetFolderGUID;
	if (AssetDatabase::Get ().IsAssetAvailable (rootAsset))
	{
		AssetDatabase::Get ().CollectAllChildren (rootAsset, &all);
		AssetInterface::Get ().StartAssetEditing ();
		
		const string progressTitle("Hold on");
		const string progressMsg("Determining assets that need to be reimported for target platform");
		
		set<UnityGUID>::iterator i;
		int count=0;
		
		double start = GetTimeSinceStartup();
		for (i=all.begin ();i != all.end ();i++)
		{
			count++;
			
			if (GetTimeSinceStartup() - start > 1)
				DisplayProgressbar(progressTitle, progressMsg, float(count) / all.size());
				
			string path = GetGUIDPersistentManager ().AssetPathNameFromGUID (*i);
			
			// If you add a new platform specific reimport here.
			// Then you have to ensure that GetAssetImportState().SetDidImportAssetForTarget () during import as well!
			
			if (TextureImporter::DoesAssetNeedReimport(path, selection, true))
				assetsNeedReimport.push_back(path);
		
			if (SubstanceImporter::DoesAssetNeedReimport(path, targetPlatform, true, requireCompressedAssets))
				assetsNeedReimport.push_back(path);

			if (AudioImporter::DoesAssetNeedReimport(path, targetPlatform, true))
				assetsNeedReimport.push_back(path);			

			if (TrueTypeFontImporter::DoesAssetNeedReimport(path, targetPlatform, true))
				assetsNeedReimport.push_back(path);			
		}
		ClearProgressbar();
		AssetInterface::Get ().SetMaxProgressImmediateImportAssets (assetsNeedReimport.size());
		
		printf_console("%f seconds.\n", GetElapsedTimeInSeconds(startTime));
		
		for (vector<string>::iterator s=assetsNeedReimport.begin ();s != assetsNeedReimport.end ();s++)
		{
			AssetInterface::Get ().ImportAtPath(*s);
		}
		
		AssetInterface::OperationStatus opStatus = AssetInterface::Get ().StopAssetEditing (cancelBehaviour);
		if (opStatus == AssetInterface::kUserCancelled)
			throw ProgressBarCancelled ();
	}
	
	GetAssetImportState().SetImportedForTarget (selection);
	GetAssetImportState().SetCompressAssetsPreference(wasCompressAssetsEnabled);
	GetAssetImportState().SetDidImportRemappedSRGB(GetRemappedSRGB());
}

void SetApplicationSettingCompressAssetsOnImport(bool value)
{
	GetAssetImportState().SetCompressAssetsPreference(value);
	
	if (value)
		VerifyAssetsForBuildTarget(true, AssetInterface::kNoCancel);
}

bool GetApplicationSettingCompressAssetsOnImport()
{
	return GetAssetImportState().GetCompressAssetsPreference();
}

void CheckTextureImporterLinearRenderingMode()
{
	VerifyAssetsForBuildTarget(false, AssetInterface::kNoCancel);
}


