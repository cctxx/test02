#include "UnityPrefix.h"
#include "LightProbeVisualization.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/GfxDevice/GfxDevice.h"

#include "Editor/Src/PrefKeys.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/Gizmos/GizmoManager.h"
#include "Editor/Src/Gizmos/GizmoRenderer.h"

#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Shaders/Material.h"
#include "Editor/Src/AnnotationManager.h"
#include "Editor/Src/Utility/CustomLighting.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Graphics/DrawUtil.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Camera/LightManager.h"


#include "Runtime/Shaders/Shader.h"

#include "Runtime/Graphics/LightProbeGroup.h"
#include "Editor/Src/LightmapperLightProbes.h"

LightProbeVisualizationSettings::LightProbeVisualizationSettings ()
:	m_tetrahedra (NULL),
	m_tetrahedraCount (0),
	m_positions (NULL),
	m_positionCount (0)
{
	m_ShowLightProbes = false;

	m_ShowLightProbeLocations = true;
	m_ShowLightProbeLocations = EditorPrefs::GetBool ("LightmappingShowLightProbeLocations", true);
	
	m_ShowLightProbeCells = true;
	m_ShowLightProbeCells = EditorPrefs::GetBool ("LightmappingShowLightProbeCells", true);
	
	m_DynamicUpdateProbes = true;
	m_DynamicUpdateProbes = EditorPrefs::GetBool ("LightmappingDynamicUpdateLightProbes", true);
}

void LightProbeVisualizationSettings::SetShowLightProbeLocations (bool show)
{
	if (show != m_ShowLightProbeLocations)
	{
		EditorPrefs::SetBool ("LightmappingShowLightProbeLocations", show);
		m_ShowLightProbeLocations = show;
	}
}

void LightProbeVisualizationSettings::SetShowLightProbeCells (bool show)
{
	if (show != m_ShowLightProbeCells)
	{
		EditorPrefs::SetBool ("LightmappingShowLightProbeCells", show);
		m_ShowLightProbeCells = show;
	}
}

void LightProbeVisualizationSettings::SetDynamicUpdateLightProbes (bool dynamic)
{
	if (dynamic != m_DynamicUpdateProbes)
	{
		EditorPrefs::SetBool ("LightmappingDynamicUpdateLightProbes", dynamic);
		m_DynamicUpdateProbes = dynamic;
	}
}

LightProbeVisualizationSettings* gLightProbeVisualizationSettings = NULL;
LightProbeVisualizationSettings& GetLightProbeVisualizationSettings ()
{
	if (gLightProbeVisualizationSettings == NULL)
		gLightProbeVisualizationSettings = new LightProbeVisualizationSettings ();
	return *gLightProbeVisualizationSettings;
}

static inline void DrawLine(const Vector3f& v0, const Vector3f& v1, GfxDevice& device)
{
	device.ImmediateVertex(v0.x, v0.y, v0.z);
	device.ImmediateVertex(v1.x, v1.y, v1.z);
}

static inline void AddBatchLine (const Vector3f& v0, const ColorRGBAf& color0, const Vector3f& v1, const ColorRGBAf& color1, dynamic_array<gizmos::ColorVertex>& lines, GfxDevice& device)
{
	ColorRGBA32 color0_32 (color0);
	lines.push_back();
	lines.back().vertex = v0;
	lines.back().color = device.ConvertToDeviceVertexColor(color0_32).GetUInt32();

	ColorRGBA32 color1_32 (color1);
	lines.push_back();
	lines.back().vertex = v1;
	lines.back().color = device.ConvertToDeviceVertexColor(color1_32).GetUInt32();
}

static inline void DrawLineBatch (dynamic_array<gizmos::ColorVertex>& lines, GfxDevice& device)
{
	for (int i = 0; i < lines.size(); i += 1000)
	{
		int count = lines.size()-i;
		if (count > 1000)
			count = 1000;
		
		device.DrawUserPrimitives(kPrimitiveLines, count, 1<<kShaderChannelVertex | 1<<kShaderChannelColor, &lines[i], sizeof(gizmos::ColorVertex) );
	}
}

static inline void DrawTetrahedron(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, const Vector3f& v3, const ColorRGBAf& color, GfxDevice& device)
{
	device.ImmediateBegin(kPrimitiveTriangleStripDeprecated);
	device.ImmediateColor(color.r, color.g, color.b, color.a);
	device.ImmediateVertex(v0.x, v0.y, v0.z);
	device.ImmediateVertex(v1.x, v1.y, v1.z);
	device.ImmediateVertex(v2.x, v2.y, v2.z);
	device.ImmediateVertex(v3.x, v3.y, v3.z);
	device.ImmediateVertex(v0.x, v0.y, v0.z);
	device.ImmediateVertex(v1.x, v1.y, v1.z);
	device.ImmediateEnd();
}

static inline void DrawTriangle(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, const ColorRGBAf& color, GfxDevice& device)
{
	device.ImmediateBegin(kPrimitiveTriangles);
	device.ImmediateColor(color.r, color.g, color.b, color.a);
	device.ImmediateVertex(v0.x, v0.y, v0.z);
	device.ImmediateVertex(v1.x, v1.y, v1.z);
	device.ImmediateVertex(v2.x, v2.y, v2.z);
	device.ImmediateEnd();
}

inline float GetLineAlpha (const Vector3f& probePosition, const Vector3f& cameraPosition, float maxDistance, float minAlpha)
{
	return clamp(Lerp(1.0f, 0.0f, Magnitude(probePosition - cameraPosition) / maxDistance), minAlpha, 1.0f);
}

void LightProbeVisualizationSettings::DrawPointCloud (	Vector3f* unselectedPositions,
														int numUnselected,
														Vector3f* selectedPositions,
														int numSelected,
														const ColorRGBAf& baseColor,
														const ColorRGBAf& selectedColor, 
														float scale,
														Transform* cloudTransform)
{
	GfxDevice& device = GetGfxDevice();
	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);

	ColorRGBAf zFailBaseColor = baseColor;
	zFailBaseColor.a *= 0.1f;
	ColorRGBAf zFailSelectColor = selectedColor;
	zFailSelectColor.a *= 0.1f;

	static PPtr<Material> s_MaterialSourceProbes = 0;
	if( !s_MaterialSourceProbes )
	{
		s_MaterialSourceProbes = dynamic_pptr_cast<Material*>(GetMainAsset ("Assets/Editor Default Resources/SceneView/LightProbeHandles.mat"));

		if( !s_MaterialSourceProbes )
			s_MaterialSourceProbes = GetEditorAssetBundle ()->Get<Material> ("SceneView/LightProbeHandles.mat");

		if( !s_MaterialSourceProbes )
			return;
	}

	s_MaterialSourceProbes->SetFloat(ShaderLab::Property("_HandleSize"), 10.0f * GetAnnotationManager ().GetIconSize ());
	
	if (unselectedPositions)
	{
		dynamic_array<Vector3f> probePositions (numUnselected, kMemTempAlloc);
		for (int j = 0; j < numUnselected; j++)
		{
			probePositions [j] = cloudTransform->TransformPoint (unselectedPositions[j]);
		}

		//draw probes
		Mesh* sphereMesh = GetBuiltinResource<Mesh> ("New-Sphere.fbx");
		Quaternionf rot = Quaternionf::identity();
		
		//Pass 1, unselected, foreground
		s_MaterialSourceProbes->SetColor(ShaderLab::Property("_HandleColor"), baseColor);
		const ChannelAssigns* foreground = s_MaterialSourceProbes->SetPass(0);
		for (int i = 0; i < numUnselected; i++)
		{
			DrawUtil::DrawMesh(*foreground, *sphereMesh, probePositions[i], rot);
		}

		//Pass 2, unselected, background
		s_MaterialSourceProbes->SetColor(ShaderLab::Property("_HandleColor"), zFailBaseColor);
		const ChannelAssigns* background = s_MaterialSourceProbes->SetPass(1);
		for (int i = 0; i < numUnselected; i++)
		{
			DrawUtil::DrawMesh(*background, *sphereMesh, probePositions[i], rot);
		}
	}
	
	if (selectedPositions)
	{
		dynamic_array<Vector3f> probePositions (numSelected, kMemTempAlloc);
		for (int j = 0; j < numSelected; j++)
		{
			probePositions [j] = cloudTransform->TransformPoint (selectedPositions[j]);
		}

		Mesh* sphereMesh = GetBuiltinResource<Mesh> ("New-Sphere.fbx");
		Quaternionf rot = Quaternionf::identity();

		//Pass 3, selected, foreground
		s_MaterialSourceProbes->SetColor(ShaderLab::Property("_HandleColor"), selectedColor);
		const ChannelAssigns* foreground = s_MaterialSourceProbes->SetPass(0);
		for (int i = 0; i < numSelected; i++)
		{
			DrawUtil::DrawMesh(*foreground, *sphereMesh, probePositions[i], rot);
		}

		//Pass 4, selected, background
		s_MaterialSourceProbes->SetColor(ShaderLab::Property("_HandleColor"), zFailSelectColor);
		const ChannelAssigns* background = s_MaterialSourceProbes->SetPass(1);
		for (int i = 0; i < numSelected; i++)
		{
			DrawUtil::DrawMesh(*background, *sphereMesh, probePositions[i], rot);
		}
	}
	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

#define DO_LINE(p1, p2) \
		edgeColor1.a = GetLineAlpha (p1, cameraPosition, tetrahedraLineFarDistance, kMinAlpha); \
		edgeColor2.a = GetLineAlpha (p2, cameraPosition, tetrahedraLineFarDistance, kMinAlpha); \
		AddBatchLine (p1, edgeColor1, p2, edgeColor2, lines, device);

void LightProbeVisualizationSettings::DrawTetrahedra (bool recalculateTetrahedra, const Vector3f cameraPosition)
{
	static PPtr<Material> s_LightProbeLinesMaterial = 0;
	if( !s_LightProbeLinesMaterial )
	{
		s_LightProbeLinesMaterial = dynamic_pptr_cast<Material*>(GetMainAsset ("Assets/Editor Default Resources/SceneView/LightProbeLines.mat"));

		if( !s_LightProbeLinesMaterial )
			s_LightProbeLinesMaterial = GetEditorAssetBundle ()->Get<Material> ("SceneView/LightProbeLines.mat");

		if( !s_LightProbeLinesMaterial )
			return;
	}

	if (recalculateTetrahedra)
	{
		dynamic_array<Vector3f> probePositions;
		LightProbeGroupList& probeGroups = GetLightProbeGroups();
		for (LightProbeGroupList::iterator i = probeGroups.begin(); i != probeGroups.end(); i++)
		{
			Vector3f* localSpacePositions = (*i)->GetPositions();
			
			Transform* transform = (*i)->QueryComponent (Transform);

			for (int j = 0; j < (*i)->GetPositionsSize (); j++)
			{
				probePositions.push_back (transform->TransformPoint (localSpacePositions[j]));
			}
		}

		delete [] m_tetrahedra;
		m_tetrahedra = NULL;
		m_tetrahedraCount = 0;

		delete [] m_positions;
		m_positions = NULL;
		m_positionCount =0;

		LightProbeUtils::Tetrahedralize(probePositions.data (), probePositions.size (), &m_tetrahedra, &m_tetrahedraCount, &m_positions, &m_positionCount);
		m_tetrahedraCount /= 4;

		if (!m_tetrahedra 
			|| m_tetrahedraCount == 0
			|| !m_positions
			|| m_positionCount == 0)
		{
			return;
		}
	}

	const float kMinAlpha = 0.1f;
	float tetrahedraLineFarDistance = 200.0f * GetAnnotationManager ().GetIconSize ();
	ColorRGBAf edgeColor1(1.0f, 0.f, 1.f, 1.0f);
	ColorRGBAf edgeColor2(1.0f, 0.f, 1.f, 1.0f);

	s_LightProbeLinesMaterial->SetPass(0);
	dynamic_array<gizmos::ColorVertex> lines;
	lines.resize_uninitialized(0);
	
	GfxDevice& device = GetGfxDevice();
	for (int t =0; t < m_tetrahedraCount; t++)
	{
		Vector3f v0 = m_positions[m_tetrahedra[t * 4 + 0]];
		Vector3f v1 = m_positions[m_tetrahedra[t * 4 + 1]];
		Vector3f v2 = m_positions[m_tetrahedra[t * 4 + 2]];
		Vector3f v3 = m_positions[m_tetrahedra[t * 4 + 3]];

		DO_LINE (v0, v1);
		DO_LINE (v1, v2);
		DO_LINE (v2, v0);
		DO_LINE (v0, v3);
		DO_LINE (v1, v3);
		DO_LINE (v2, v3);
	}

	DrawLineBatch (lines, GetGfxDevice());
}

void DrawLightProbeGizmoImmediate ()
{
	if (!GetLightProbeVisualizationSettings ().GetShowLightProbes ())
		return;

	// then render all objects with the 'Show Lightmap Resolution' shader, alpha-blended
	GfxDevice& device = GetGfxDevice();

	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);

	static PPtr<Material> s_LightProbeLinesMaterial = 0;
	if( !s_LightProbeLinesMaterial )
	{
		s_LightProbeLinesMaterial = dynamic_pptr_cast<Material*>(GetMainAsset ("Assets/Editor Default Resources/SceneView/LightProbeLines.mat"));

		if( !s_LightProbeLinesMaterial )
			s_LightProbeLinesMaterial = GetEditorAssetBundle ()->Get<Material> ("SceneView/LightProbeLines.mat");

		if( !s_LightProbeLinesMaterial )
			return;
	}

	static PPtr<Material> s_LightProbeMaterial = 0;
	if( !s_LightProbeMaterial )
	{
		s_LightProbeMaterial = dynamic_pptr_cast<Material*>(GetMainAsset ("Assets/Editor Default Resources/SceneView/SH.mat"));

		if( !s_LightProbeMaterial )
			s_LightProbeMaterial = GetEditorAssetBundle ()->Get<Material> ("SceneView/SH.mat");

		if( !s_LightProbeMaterial )
			return;
	}

	LightProbes* lightProbes = GetLightProbes();
	
	if (lightProbes && GetLightProbeVisualizationSettings ().GetShowLightProbeLocations () )
	{
		ColorRGBAf edgeColor(1.0f, 0.922f, 0.016f, 0.2f);
		ColorRGBAf faceColor(0.8f, 0.8f, 0.0f, 0.1f);
		ColorRGBAf faceEdgeColor(1.0f, 1.0f, 0.0f, 1.0f);

		Vector3f* positions = lightProbes->GetPositions();
		LightProbeCoefficients* lightProbeData = lightProbes->GetCoefficients();
		int positionCount = lightProbes->GetPositionsSize();
		Tetrahedron* tetrahedra = lightProbes->GetTetrahedra();
		Vector3f* hullRays = lightProbes->GetHullRays();
		int tetrahedraCount = lightProbes->GetTetrahedraSize();

		//draw probes
		Mesh* sphereMesh = GetBuiltinResource<Mesh> ("New-Sphere.fbx");
		Quaternionf rot = Quaternionf::identity();
		BuiltinShaderParamValues& params = device.GetBuiltinParamValues();
		s_LightProbeMaterial->SetFloat(ShaderLab::Property("_HandleSize"), 10.0f * GetAnnotationManager ().GetIconSize ());
		CustomLighting::Get().RemoveCustomLighting();
		GetRenderSettings().SetupAmbient();
		for (int i = 0; i < positionCount; i++)
		{
			SetSHConstants ((float (*)[3])lightProbeData[i].sh, params);
			const ChannelAssigns* channels = s_LightProbeMaterial->SetPass(0);
			DrawUtil::DrawMesh(*channels, *sphereMesh, positions[i], rot);
		}

		s_LightProbeLinesMaterial->SetPass(0);
		
		if (GetLightProbeVisualizationSettings ().GetShowLightProbeCells () && positionCount > 0)
		{
			dynamic_array<gizmos::ColorVertex> lines;
			lines.resize_uninitialized(0);
		
			// draw tetrahedra
			for (int i = 0; i < tetrahedraCount; i++)
			{
				Tetrahedron& t = tetrahedra[i];
				if (t.indices[3] >= 0)
				{
					Vector3f v0 = positions[t.indices[0]];
					Vector3f v1 = positions[t.indices[1]];
					Vector3f v2 = positions[t.indices[2]];
					Vector3f v3 = positions[t.indices[3]];
					AddBatchLine (v0, edgeColor, v1, edgeColor, lines, device);
					AddBatchLine (v1, edgeColor, v2, edgeColor, lines, device);
					AddBatchLine (v2, edgeColor, v0, edgeColor, lines, device);
					AddBatchLine (v0, edgeColor, v3, edgeColor, lines, device);
					AddBatchLine (v1, edgeColor, v3, edgeColor, lines, device);
					AddBatchLine (v2, edgeColor, v3, edgeColor, lines, device);
				}
				else
				{
					Vector3f& v0 = positions[t.indices[0]];
					Vector3f& v1 = positions[t.indices[1]];
					Vector3f& v2 = positions[t.indices[2]];

					// draw rays at vertices
					AddBatchLine (v0, edgeColor, v0 + hullRays[t.indices[0]], edgeColor, lines, device);
					AddBatchLine (v1, edgeColor, v1 + hullRays[t.indices[1]], edgeColor, lines, device);
					AddBatchLine (v2, edgeColor, v2 + hullRays[t.indices[2]], edgeColor, lines, device);
				}
			}
			DrawLineBatch (lines, device);
		}

		GameObject* activeGO = GetActiveGO();
		Renderer* active = NULL;
		if (activeGO)
			active = activeGO->QueryComponent(Renderer);
		if(active && active->GetUseLightProbes())
		{
			Vector3f pos = active->GetLightProbeInterpolationPosition();
			float coefficients[9][3];
			int tetIndex = -1;
			Vector4f weights;
			float t;
			lightProbes->GetInterpolatedLightProbe(pos, active, &coefficients[0][0], tetIndex, weights, t);
			if (tetIndex >=0)
			{
				
				// draw the interpolated probe without z test
				SetSHConstants (coefficients, params);
				s_LightProbeMaterial->SetFloat(ShaderLab::Property("_HandleSize"), 11.0f * GetAnnotationManager ().GetIconSize ());
				const ChannelAssigns* channels = s_LightProbeMaterial->SetPass(1);
				DrawUtil::DrawMesh(*channels, *sphereMesh, pos, rot);

				s_LightProbeLinesMaterial->SetPass(0);
				Tetrahedron& tet = tetrahedra[tetIndex];
				Vector3f normal(&tet.matrix[0]);
				if (tet.indices[3] < 0)
				{
					device.ImmediateBegin(kPrimitiveLines);

					//Outline of edge
					device.ImmediateColor(faceEdgeColor.r, faceEdgeColor.g, faceEdgeColor.b, faceEdgeColor.a);
					DrawLine(positions[tet.indices[0]], positions[tet.indices[1]], device);
					DrawLine(positions[tet.indices[0]], positions[tet.indices[2]], device);
					DrawLine(positions[tet.indices[1]], positions[tet.indices[2]], device);

					//Draw a line from the center of the triangle to the object...
					Vector3f midPoint = (positions[tet.indices[0]] * weights.x + positions[tet.indices[1]] * weights.y + positions[tet.indices[2]] * weights.z);
					DrawLine(midPoint, pos, device);

					device.ImmediateEnd();

					DrawTriangle(positions[tet.indices[0]], positions[tet.indices[1]], positions[tet.indices[2]], faceColor, device);
				}
				else
				{
					//Outline of edge
					device.ImmediateBegin(kPrimitiveLines);
					device.ImmediateColor(faceEdgeColor.r, faceEdgeColor.g, faceEdgeColor.b, faceEdgeColor.a);
					DrawLine(positions[tet.indices[0]], positions[tet.indices[1]], device);
					DrawLine(positions[tet.indices[0]], positions[tet.indices[2]], device);
					DrawLine(positions[tet.indices[0]], positions[tet.indices[3]], device);
					DrawLine(positions[tet.indices[1]], positions[tet.indices[2]], device);
					DrawLine(positions[tet.indices[1]], positions[tet.indices[3]], device);
					DrawLine(positions[tet.indices[2]], positions[tet.indices[3]], device);
					device.ImmediateEnd();

					DrawTetrahedron(positions[tet.indices[0]], positions[tet.indices[1]], positions[tet.indices[2]], positions[tet.indices[3]], faceColor, device);
				}

			}
		}
	}

	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}
