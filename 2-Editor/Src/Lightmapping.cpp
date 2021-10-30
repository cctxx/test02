#include "UnityPrefix.h"
#include "Lightmapping.h"

#if ENABLE_LIGHTMAPPER

#include "LightmapperBeast.h"
#include "Runtime/Camera/LightManager.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/Application.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "AssetPipeline/AssetPathUtilities.h"

// ----------------------------------------------------------------------
//  Global Scope
// ----------------------------------------------------------------------

LightmapperBeastShared* g_Lbs = NULL;

bool ComputeLightmaps (bool async, bool bakeSelected, bool lightProbesOnly)
{
	bool success = false;

	if (!GetApplication().EnsureSceneHasBeenSaved("lightmaps or light probes"))
		return false;

	// don't bake in play mode
	if (IsWorldPlaying())
		return false;
	
	try
	{
		LightmapperBeast lm;
		lm.Prepare(g_Lbs, bakeSelected, lightProbesOnly);

		if (async)
			g_Lbs->ComputeAsync();
		else
			g_Lbs->Compute();

		success = true;
	}
	catch (BeastException& be)
	{
		ClearProgressbar();
		ErrorStringObject(be.What(), be.WhatObject());
		if (g_Lbs)
		{
			delete g_Lbs;
			g_Lbs = NULL;
		}
	}

	return success;
}

void CancelLightmapping()
{
	if (g_Lbs)
		g_Lbs->Cancel();
}

bool IsRunningLightmapping()
{
	return g_Lbs != NULL;
}

void DeleteLightmapAssets(std::vector<string>* excludedPaths, int stride)
{
	string folderPath = GetSceneBakedAssetsPath();
	if (folderPath == "")
		return; //nothing to delete, the scene hasn't even been saved yet
	
	AssetInterface& ai = AssetInterface::Get();
	int i = 0;

	const int potentialPathsSize = LightmapperBeastResults::kLightmapPathsSize;
	
	if (excludedPaths != NULL)
	{
		const int excludedPathsSize = excludedPaths->size();
		const int excludedLightmapDatasCount = excludedPaths->size()/stride;
		for(; i < LightmapSettings::kMaxLightmaps && i < excludedLightmapDatasCount; i++)
		{
			for (int j = 0; j < potentialPathsSize; j++)
			{
				if (LightmapperBeastResults::kLightmapPaths[j] == "")
					continue;

				string potentialPath = Format(LightmapperBeastResults::kLightmapPaths[j], folderPath.c_str(), i);

				// compare the possible path with each excluded path -- if they're the same, don't delete the asset
				int k = 0;
				for (; k < excludedPathsSize; k++)
				{
					if (potentialPath.compare((*excludedPaths)[k]) == 0)
						break;
				}

				// if the possible path wasn't found in excludedPaths and the file exists at that path -- delete it
				if (k == excludedPathsSize && IsFileCreated(potentialPath))
					ai.DeleteAsset (potentialPath);
			}
		}
	}

	for(; i < LightmapSettings::kMaxLightmaps; i++)
	{
		for (int j = 0; j < potentialPathsSize; j++)
		{
			if (LightmapperBeastResults::kLightmapPaths[j] == "")
				continue;

			string path = Format(LightmapperBeastResults::kLightmapPaths[j], folderPath.c_str(), i);
			if (IsFileCreated(path))
				ai.DeleteAsset (path);
		}
	}
}

void ClearLightmaps (bool clearLightmapIndices)
{
	if (clearLightmapIndices)
		BeastUtils::ClearLightmapIndices();

	// clear LightmapSettings array
	if (clearLightmapIndices)
		GetLightmapSettings().ClearLightmaps();

	// mark all lights as not baked yet
	std::vector<Light*> lights;
	Object::FindObjectsOfType(&lights);
	for (size_t i = 0; i < lights.size(); ++i)
	{
		Light* l = lights[i];

		// Only allow scene objects
		if (l->IsPersistent())
			continue;

		l->SetActuallyLightmapped (false);
	}
}

void DeleteGlobalLightmapperBeastShared ()
{
	delete g_Lbs;
	g_Lbs = NULL;
}


#else // #if ENABLE_LIGHTMAPPER


bool ComputeLightmaps (bool async, bool bakeSelected, bool lightProbesOnly) { return false; }
void CancelLightmapping() { }
void ClearLightmaps (bool clearLightmapIndices) { }
void DeleteLightmapAssets(std::vector<std::string>* excludedPaths, int stride) { }
void DeleteGlobalLightmapperBeastShared () { }
bool IsRunningLightmapping() { return false; }
float GetLightmapLODLevelScale (Renderer& renderer) { return 1.0f; }


#endif // #if ENABLE_LIGHTMAPPER
