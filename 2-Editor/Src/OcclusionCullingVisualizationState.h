#pragma once

class OcclusionCullingVisualization;
class Camera;

OcclusionCullingVisualization* GetOcclusionCullingVisualization ();

class OcclusionCullingVisualization
{
public:
	
	OcclusionCullingVisualization ();
	~OcclusionCullingVisualization () {};
	
	
	void SetShowOcclusionCulling (bool state) { m_ShowOcclusionCulling = state; }
	bool GetShowOcclusionCulling () { return m_ShowOcclusionCulling; }

	void SetShowPreVis (bool state);
	bool GetShowPreVis ();
	
	void SetShowGeometryCulling (bool state);
	bool GetShowGeometryCulling ();

	void SetShowViewVolumes (bool state) { m_ShowViewVolumes = state; }
	bool GetShowViewVolumes () { return m_ShowViewVolumes; }

	void SetShowPortals(bool state) { m_ShowPortals = state; }
	bool GetShowPortals () { return m_ShowPortals; }

	void SetShowDynamicObjectBounds(bool state) { m_ShowDynamicObjectBounds = state; }
	bool GetShowDynamicObjectBounds() { return m_ShowDynamicObjectBounds; }

	void SetShowVisibilityLines(bool state) { m_ShowVisLines = state; }
	bool GetShowVisibilityLines() { return m_ShowVisLines; }

	float GetSmallestHole () { return m_SmallestHole; }
	void SetSmallestHole (float q) { m_SmallestHole = q; }

private:
	
	bool m_ShowOcclusionCulling;
	bool m_ShowPreVis;
	bool m_ShowViewVolumes;
	bool m_ShowGeometryCulling;
    bool m_ShowVFCulling;
	int  m_CullingMode;
	bool m_ShowPortals;
	bool m_ShowDynamicObjectBounds;
	bool m_ShowVisLines;
	float m_SmallestHole;
};


Camera* FindPreviewOcclusionCamera ();
