#pragma once

#include "DebugDraw.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Editor/Src/Gizmos/GizmoRenderer.h"

class Transform;

class LightProbeVisualizationSettings
{
public:
	LightProbeVisualizationSettings ();
	~LightProbeVisualizationSettings (){};

	GET_SET(bool, ShowLightProbes, m_ShowLightProbes);

	bool GetShowLightProbeLocations () { return m_ShowLightProbeLocations; }
	void SetShowLightProbeLocations (bool show);

	bool GetShowLightProbeCells () { return m_ShowLightProbeCells; }
	void SetShowLightProbeCells (bool show);

	bool GetDynamicUpdateLightProbes () { return m_DynamicUpdateProbes; }
	void SetDynamicUpdateLightProbes (bool dynamic);

	void DrawTetrahedra (bool recalculateTetrahedra, const Vector3f cameraPosition);

	void DrawPointCloud (	Vector3f* unselectedPositions,
							int numUnselected,
							Vector3f* selectedPositions,
							int numSelected,
							const ColorRGBAf& baseColor,
							const ColorRGBAf& selectedColor, 
							float scale,
							Transform* cloudTransform);

private:
	bool m_ShowLightProbes;
	bool m_ShowLightProbeLocations;
	bool m_ShowLightProbeCells;
	bool m_DynamicUpdateProbes;

	int* m_tetrahedra;
	int m_tetrahedraCount;
	Vector3f* m_positions;
	int m_positionCount;
};

LightProbeVisualizationSettings& GetLightProbeVisualizationSettings ();
void DrawLightProbeGizmoImmediate ();
