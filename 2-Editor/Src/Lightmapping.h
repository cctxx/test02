#ifndef LIGHTMAPPING_H
#define LIGHTMAPPING_H

class Renderer;
enum LightmappingProgress {
	kLightmappingInProgress,	// job is in progress
	kLightmappingDone,			// job has finished
	kLightmappingCancelled		// job has been cancelled
};

template<class T> struct triple;

bool ComputeLightmaps (bool async, bool bakeSelected = false, bool lightProbesOnly = false);
void CancelLightmapping();
void ClearLightmaps (bool clearLightmapIndices = true);
void DeleteLightmapAssets(std::vector<std::string>* excludedPaths = NULL, int stride = 1);
void DeleteGlobalLightmapperBeastShared ();
bool IsRunningLightmapping();
float GetLightmapLODLevelScale (Renderer& renderer);

#endif
