#pragma once

#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/BitUtility.h"

class LODGroup;
struct CullingParameters;

enum
{
	kMaximumLODLevels = 8,
	kInvalidLODGroup = -1,

	// When an LOD group is disabled, we want to make all renderers be disabled.
	// For this purpose we have a single shared LODGroup index, with a mask that has all renderers always disabled.
	kDisabledLODGroup = 0 
};

#if UNITY_EDITOR
// LOD Visualization 
// (NOTE: Keep LODUtilityBindings.txt struct in sync)
struct LODVisualizationInformation
{
	int    triangleCount;
	int    vertexCount;
	int    rendererCount;
	int    submeshCount;
	
	int    activeLODLevel;
	float  activeLODFade;
	float  activeDistance;
	float  activeRelativeScreenSize;
	float  activePixelSize;
	float  activeWorldSpaceSize;
};
#endif

class LODGroupManager
{
	struct LODSelectionData
	{
		// The point we measure the LOD distance against
		Vector3f  worldReferencePoint;
		float     maxDistanceSqr;
		
		// LOD maximum distance values
		float     maxDistances[kMaximumLODLevels];
		int       maxDistancesCount;
		float     fadeDistance;
		
		// The associated lod group
		LODGroup* lodGroup; 
		
		UInt32     forceLODLevelMask;
	};
	
	dynamic_array<LODSelectionData>  m_SelectionData;
	float                            m_LODBias;
	UInt32                           m_MaximumLOD;
	
public:
	
	LODGroupManager (); 
	
	void   AddLODGroup (LODGroup& group, const Vector3f& position, float worldSpaceSize);
	void   RemoveLODGroup (LODGroup& group);
	
	void   UpdateLODGroupParameters (int index, LODGroup& group, const Vector3f& position, float worldSpaceSize);
	void   UpdateLODGroupPosition (int index, const Vector3f& position) { m_SelectionData[index].worldReferencePoint = position; }
	
	void   SetLODBias (float b)                                         { m_LODBias = b; }
	float  GetLODBias () const                                          { return m_LODBias; }
	
	void   SetMaximumLODLevel (UInt32 b)                                { m_MaximumLOD = b; }
	UInt32 GetMaximumLODLevel () const                                  { return m_MaximumLOD; }
	
	void   ResetLODBias ()                                              { SetLODBias(1.0F); }

	int    GetLODGroupCount () const                                    { return m_SelectionData.size(); }
	
	
	// Used by scene culling to determine 
	// /lodGroupIndex/ is the index of LODGroup into m_SelectionData & m_ActiveLOD
	// /activeLODMask/ is the LOD mask of the renderer. 
	// m_ActiveLOD[lodGroupIndex].activeMask is a bitmask specifying which LOD levels should be rendered.
	// When cross-fading between two LOD's multiple LOD's might be active in the same LODGroup
	static bool IsLODVisible (UInt32 lodGroupIndex, UInt32 activeLODMask, const UInt8* activeLOD)
	{
		if (activeLODMask == 0)
			return true;
		
		return activeLOD[lodGroupIndex] & activeLODMask;
	}

	void CalculateLODMasks (const CullingParameters& parameters, UInt8* outMasks, float* outFades);
	static float CalculateLODFade (UInt32 lodGroupIndex, UInt32 rendererActiveLODMask, const UInt8* lodMasks, const float* lodFades);
	
	static void CalculatePerspectiveLODMask (const LODSelectionData& selection, const Vector3f& position, int maximumLOD, int currentMask, const float* fieldOfViewFudge, UInt8* output, float* outputFade);
	static void CalculateOrthoLODMask (const LODSelectionData& selection, int maximumLOD, int currentMask, const float* fudge, UInt8* output, float* outputFade);

#if UNITY_EDITOR
	LODVisualizationInformation CalculateVisualizationData (const CullingParameters& cullingParameters, LODGroup& lodGroup, int lod);
	
#endif

	void   SetForceLODMask (int index, UInt32 forceEditorLODMask)     { m_SelectionData[index].forceLODLevelMask = forceEditorLODMask; }
	UInt32 GetForceLODMask (int index)                               { return m_SelectionData[index].forceLODLevelMask; }
	void   ClearAllForceLODMask ();

};

LODGroupManager* GetLODGroupManagerPtr ();
LODGroupManager& GetLODGroupManager ();
void CleanupLODGroupManager ();
void InitializeLODGroupManager ();
