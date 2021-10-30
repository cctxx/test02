#include "UnityPrefix.h"
#include "NavMeshVisualization.h"
#include "Runtime/NavMesh/NavMesh.h"
#include "Runtime/NavMesh/NavMeshAgent.h"
#include "Runtime/NavMesh/NavMeshSettings.h"
#include "Runtime/NavMesh/NavMeshManager.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"


#include "Editor/Src/PrefKeys.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/Gizmos/GizmoManager.h"
#include "Editor/Src/Gizmos/GizmoRenderer.h"

#include "DebugDraw.h"
#include "DetourDebugDraw.h"
#include "DetourNavMesh.h"
#include "DetourCrowd.h"
#include "DetourCrowdTypes.h"

PREFCOLOR (kGizmoNavMesh,		0x91F48BC0);
PREFCOLOR (kGizmoHeightMesh,	0xFF7ACAFF);
PREFCOLOR (kGizmoBadMesh,		0xFF000040);
PREFCOLOR (kGizmoDetailMesh,	0x00FF00FF);

NavMeshVisualizationSettings::NavMeshVisualizationSettings ()
{
	m_ShowNavigation = false;
	m_ShowNavMesh = EditorPrefs::GetBool ("NavigationShowNavMesh", true);
	m_ShowHeightMesh = EditorPrefs::GetBool ("NavigationShowHeightMesh", false);
}

void NavMeshVisualizationSettings::SetShowNavMesh (bool show)
{
	if (show != m_ShowNavMesh)
	{
		m_ShowNavMesh = show;
		EditorPrefs::SetBool ("NavigationShowNavMesh", show);
	}
}

void NavMeshVisualizationSettings::SetShowHeightMesh (bool show)
{
	if (show != m_ShowHeightMesh)
	{
		EditorPrefs::SetBool ("NavigationShowHeightMesh", show);
		m_ShowHeightMesh = show;
	}
}

bool NavMeshVisualizationSettings::HasHeightMesh () const
{
	const NavMesh* mesh = GetNavMeshSettings ().GetNavMesh ();
	if (mesh == NULL)
		return false;

	const dtNavMesh* internalMesh = mesh->GetInternalNavMesh ();

	if (internalMesh == NULL || internalMesh->tileCount () == 0)
		return false;

	const dtMeshHeader* header = NULL;
	for (int i=0;i<internalMesh->getMaxTiles () && !header;++i)
		header = internalMesh->getTile (i)->header;

	return header && (header->flags & DT_MESH_HEADER_USE_HEIGHT_MESH);
}

NavMeshVisualizationSettings* NavMeshVisualizationSettings::s_Instance = NULL;

NavMeshVisualizationSettings& GetNavMeshVisualizationSettings ()
{
	if (!NavMeshVisualizationSettings::s_Instance)
		NavMeshVisualizationSettings::s_Instance = UNITY_NEW (NavMeshVisualizationSettings, kMemManager);
	return *NavMeshVisualizationSettings::s_Instance;
}

void NavMeshVisualizationSettings::StaticDestroy ()
{
	UNITY_DELETE (s_Instance, kMemManager);
}

static RegisterRuntimeInitializeAndCleanup s_NavMeshVisualizationSettingsCallbacks (NULL, NavMeshVisualizationSettings::StaticDestroy);

void DrawNavMeshAgent (const NavMeshAgent& agent)
{
	const dtCrowdHandle handle = agent.m_AgentHandle;
	if (!handle.IsValid ())
		return;

	const dtCrowdAgent* ag = agent.GetInternalAgent ();
	if (ag == NULL)
		return;

	dtCrowdAgentDebugInfo* debugInfo = GetNavMeshManager ().GetInternalDebugInfo ();
	if (debugInfo == NULL)
		return;

	RecastDebugDraw dd;

	if (ag->active)
	{
		const float radius = ag->params.radius;
		const float* pos = ag->npos;
		const float height = ag->params.height;

		// draw paths
		if (ag->ncorners)
		{
			dd.begin (DU_DRAW_LINES, 2.0f);
			for (int j = 0; j < ag->ncorners; ++j)
			{
				const float* va = j == 0 ? pos : &ag->cornerVerts[(j-1)*3];
				const float* vb = &ag->cornerVerts[j*3];
				dd.vertex (va[0],va[1]+radius,va[2], duRGBA (128,0,0,192));
				dd.vertex (vb[0],vb[1]+radius,vb[2], duRGBA (128,0,0,192));
			}
			if (ag->ncorners && ag->cornerFlags[ag->ncorners-1] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
			{
				const float* v = &ag->cornerVerts[(ag->ncorners-1)*3];
				dd.vertex (v[0],v[1],v[2], duRGBA (192,0,0,192));
				dd.vertex (v[0],v[1]+radius*2,v[2], duRGBA (192,0,0,192));
			}

			dd.end ();
		}

		// Show velocities
		const float* vel = ag->vel;
		const float* dvel = ag->dvel;
		const float* nvel = ag->nvel;

		duDebugDrawArrow (&dd, pos[0],pos[1]+height,pos[2],
			pos[0]+nvel[0],pos[1]+height+nvel[1],pos[2]+nvel[2],
			0.0f, 0.4f, duRGBA (0,255,0,255), 2.0f);

		duDebugDrawArrow (&dd, pos[0],pos[1]+height,pos[2],
			pos[0]+dvel[0],pos[1]+height+dvel[1],pos[2]+dvel[2],
			0.0f, 0.4f, duRGBA (0,192,255,192), 1.0f);

		duDebugDrawArrow (&dd, pos[0],pos[1]+height,pos[2],
			pos[0]+vel[0],pos[1]+height+vel[1],pos[2]+vel[2],
			0.0f, 0.4f, duRGBA (0,0,0,160), 2.0f);

#if 0
		// Draw sampled neighbor agents
		for (int j = 0; j < ag->nneis; ++j)
		{
			const dtCrowdAgent* nei = GetNavMeshManager ().GetCrowdSystem ()->getAgent (ag->neis[j].idx);
			if (!nei) continue; // obstacle ids  > agent ids
			const float* va = ag->npos;
			const float* vb = nei->npos;
			dd.begin (DU_DRAW_LINES, 2.0f);
			dd.vertex (va[0],va[1]+radius,va[2], duRGBA (128,121,0,192));
			dd.vertex (vb[0],vb[1]+radius,vb[2], duRGBA (128,121,0,192));
			dd.end ();
		}

		// Draw sampled wall segments
		for (int j = 0; j < ag->boundary.getSegmentCount (); ++j)
		{
			const float* va = ag->boundary.getSegment (j);
			const float* vb = va+3;
			dd.begin (DU_DRAW_LINES, 2.0f);
			dd.vertex (va[0],va[1]+radius,va[2], duRGBA (128,0,121,192));
			dd.vertex (vb[0],vb[1]+radius,vb[2], duRGBA (128,0,121,192));
			dd.end ();
		}
#endif
	}

	// Set the debug index.
	debugInfo->idx = handle.GetIndex ();
}

void DrawNavMeshGizmoImmediate ()
{
	const NavMeshVisualizationSettings& settings = GetNavMeshVisualizationSettings ();

	if (!settings.GetShowNavigation ())
		return;

	NavMesh* mesh = GetNavMeshSettings ().GetNavMesh ();
	if (mesh == NULL)
		return;

	const dtNavMesh* internalMesh = mesh->GetInternalNavMesh ();

	if (internalMesh == NULL || internalMesh->tileCount () == 0)
		return;

	// The height mesh replaces detail mesh - so don't draw detail mesh if we have a height mesh.
	const bool hasHeightMesh = settings.HasHeightMesh ();
	if (settings.GetShowNavMesh ())
	{
		RecastDebugDraw dd;
		unsigned char flags = DU_DRAWNAVMESH_OFFMESHCONS;
		if (!hasHeightMesh)
			flags |= DU_DRAWNAVMESH_DETAIL_MESHES;

		duDebugDrawNavMesh (&dd, *internalMesh, flags);
	}

	if (hasHeightMesh && settings.GetShowHeightMesh ())
	{
		RecastDebugDraw dd;
		ColorRGBAf color = kGizmoHeightMesh.GetColor ();
		unsigned int col = duRGBAf (color.r, color.g, color.b, color.a);

		duDebugDrawDetailMesh (&dd, *internalMesh, col);
	}
}

void RecastDebugDraw::begin (duDebugDrawPrimitives prim, float size /*= 1.0f*/)
{
	switch (prim)
	{
	case DU_DRAW_POINTS:
		m_RenderMode = kPrimitiveTypeCount;
		break;
	case DU_DRAW_LINES:
		m_RenderMode = kPrimitiveLines;
		break;
	case DU_DRAW_TRIS:
		m_RenderMode = kPrimitiveTriangles;
		break;
	case DU_DRAW_QUADS:
		m_RenderMode = kPrimitiveQuads;
		break;
	default:
		AssertString ("Unsupported");
	};

	m_Vertices.resize_uninitialized (0);
}

void RecastDebugDraw::vertex (const float x, const float y, const float z, unsigned int color, const float u, const float v)
{
	ColorRGBA32 adjustedColor (color);
	adjustedColor.a = (adjustedColor.a * m_AlphaScale) / 255;
	m_Vertices.push_back (gizmos::ColorVertex (Vector3f (x,y,z), adjustedColor));
	
	// GfxDeviceD3D9/D3D11::DrawUserPrimitives() asserts if passing more than 60k vertices,
	// we'll split the rendering into 60k chunks.
	if (m_Vertices.size () >= kVertexCountLimit)
		flush ();
}

void RecastDebugDraw::vertex (const float* pos, unsigned int color)
{
	vertex (pos[0], pos[1], pos[2], color, 0.0F, 0.0F);
}

void RecastDebugDraw::vertex (const float* pos, unsigned int color, const float* uv)
{
	vertex (pos[0], pos[1], pos[2], color, uv[0], uv[1]);
}

void RecastDebugDraw::vertex (const float x, const float y, const float z, unsigned int color)
{
	vertex (x, y, z, color, 0.0F, 0.0F);
}

void RecastDebugDraw::end ()
{
	flush ();
}

void RecastDebugDraw::flush ()
{
	/////@TODO: HANDLE
	if (m_RenderMode == kPrimitiveTypeCount)
		return;

	if (m_gizmoMode)
	{
		gizmos::AddColorOcclusionPrimitives (m_RenderMode, m_Vertices.size (), m_Vertices.begin ());
	}
	else
	{
		GetGfxDevice ().DrawUserPrimitives (m_RenderMode, m_Vertices.size (), (1<<kShaderChannelVertex) | (1<<kShaderChannelColor), m_Vertices.begin (), sizeof (gizmos::ColorVertex));
	}

	m_Vertices.resize_uninitialized (0);
}


