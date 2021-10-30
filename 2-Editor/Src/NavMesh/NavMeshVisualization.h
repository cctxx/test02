#pragma once

#include "DebugDraw.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Editor/Src/Gizmos/GizmoRenderer.h"


class NavMeshVisualizationSettings
{
public:
	NavMeshVisualizationSettings ();
	~NavMeshVisualizationSettings (){};

	GET_SET(bool, ShowNavigation, m_ShowNavigation);

	bool GetShowNavMesh () const { return m_ShowNavMesh; }
	void SetShowNavMesh (bool show);

	bool GetShowHeightMesh () const { return m_ShowHeightMesh; }
	void SetShowHeightMesh (bool show);

	bool HasHeightMesh () const;

	static NavMeshVisualizationSettings* s_Instance;
	static void StaticDestroy ();
private:
	bool m_ShowNavigation;
	bool m_ShowNavMesh;
	bool m_ShowHeightMesh;
};

NavMeshVisualizationSettings& GetNavMeshVisualizationSettings ();

class NavMeshAgent;
void DrawNavMeshAgent (const NavMeshAgent& agent);
void DrawNavMeshGizmoImmediate ();


class RecastDebugDraw : public duDebugDraw
{
public:
	///@TODO: Support FOR UV & textures in gizmos.
	RecastDebugDraw ()
	: m_AlphaScale (255)
	, m_CompareFunc (kFuncLess)
	, m_gizmoMode (true) {}

	virtual ~RecastDebugDraw () {}
	virtual void depthMask (bool inputValue) {}
	virtual void texture (bool state) {}

	// Begin drawing primitives.
	// Params:
	//  prim - (in) primitive type to draw, one of rcDebugDrawPrimitives.
	//  nverts - (in) number of vertices to be submitted.
	//  size - (in) size of a primitive, applies to point size and line width only.
	virtual void begin (duDebugDrawPrimitives prim, float size = 1.0f);

	// Submit a vertex
	// Params:
	//  x,y,z - (in) position of the verts.
	//  color - (in) color of the verts.
	virtual void vertex (const float x, const float y, const float z, unsigned int color, const float u, const float v);
	virtual void vertex (const float* pos, unsigned int color);
	virtual void vertex (const float* pos, unsigned int color, const float* uv);
	virtual void vertex (const float x, const float y, const float z, unsigned int color);

	// End drawing primitives.
	virtual void end ();
private:
	
	void flush ();
	
	// Maximum number of vertices to commit per draw call.
	static const int kVertexCountLimit = 60000;
	
	int              m_AlphaScale;
	CompareFunction  m_CompareFunc;
	bool			 m_gizmoMode;

	dynamic_array<gizmos::ColorVertex> m_Vertices;
	GfxPrimitiveType m_RenderMode;
};
