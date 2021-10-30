#include "UnityPrefix.h"
#include "OcclusionCullingVisualization.h"
#include "OcclusionCullingVisualizationState.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "OcclusionCulling.h"
#include "Gizmos/GizmoVertexFormats.h"
#include "Editor/Src/Gizmos/GizmoRenderer.h"

#include "External/Umbra/builds/interface/runtime/umbraTome.hpp"
#include "External/Umbra/builds/interface/runtime/umbraQuery.hpp"

#include "Runtime/Camera/UnityScene.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/CameraCullingParameters.h"
#include "Runtime/Camera/CullResults.h"

static void DrawUmbraLinesGizmoImmediate_Draw (const dynamic_array<gizmos::ColorVertex>& lines)
{
    for (int i = 0; i < lines.size(); i += 1000)
    {
        int count = lines.size()-i;
        if (count > 1000)
            count = 1000;
		
		gizmos::AddColorOcclusionPrimitives(kPrimitiveLines, count, &lines[i]);
    }
}

static void DrawUmbraQuadsGizmoImmediate_Draw (const dynamic_array<gizmos::ColorVertex>& quads)
{
    for (int i = 0; i < quads.size(); i += 1000)
    {
        int count = quads.size()-i;
        if (count > 1000)
            count = 1000;
		
		gizmos::AddColorOcclusionPrimitives(kPrimitiveQuads, count, &quads[i]);
    }
}

class UmbraDebugRenderer : public Umbra::DebugRenderer
{
public:
    UmbraDebugRenderer(dynamic_array<gizmos::ColorVertex>& lines)
	: m_Lines(lines)
    {
		m_Lines.resize_uninitialized(0);
    }
	
    virtual ~UmbraDebugRenderer(void)
    {
    }
	
    virtual void addLine	(const Umbra::Vector3& start, const Umbra::Vector3& end, const Umbra::Vector4& color)
    {
		ColorRGBA32 c = (ColorRGBAf&)color;
		
        m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)start;
		m_Lines.back().color = c.GetUInt32();
		
		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)end;
		m_Lines.back().color = c.GetUInt32();
    }
	
    virtual void addPoint	(const Umbra::Vector3& point, const Umbra::Vector4& color)
    {
		AssertString("TODO IMPLEMENT");
		//        m_Lines.push_back((Vector3f&)point);
		//        m_Lines.push_back((Vector3f&)point+Vector3f(0.1f,0.1f,0.1f));
    }

	virtual void addQuad    (const Umbra::Vector3& p0, const Umbra::Vector3& p1, const Umbra::Vector3& p2, const Umbra::Vector3& p3, const Umbra::Vector4& color)
	{
		ColorRGBA32 c = (ColorRGBAf&)color;
		
        m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p0;
		m_Lines.back().color = c.GetUInt32();
		
		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p1;
		m_Lines.back().color = c.GetUInt32();

		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p2;
		m_Lines.back().color = c.GetUInt32();
		
		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p3;
		m_Lines.back().color = c.GetUInt32();

        m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p0;
		m_Lines.back().color = c.GetUInt32();
		
		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p3;
		m_Lines.back().color = c.GetUInt32();

		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p2;
		m_Lines.back().color = c.GetUInt32();
		
		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p1;
		m_Lines.back().color = c.GetUInt32();
	}

    virtual void addBuffer	(Umbra::UINT8* , int , int )
    {
    }
	
private:
	
    dynamic_array<gizmos::ColorVertex>& m_Lines;
};

static void pushBackLine(dynamic_array<gizmos::ColorVertex>& outLines, Vector3f* vertices, int i0, int i1)
{
	outLines.push_back();
	outLines.back().vertex = vertices[i0];
	outLines.back().color = ColorRGBA32(0xffffffff);
	
	outLines.push_back();
	outLines.back().vertex = vertices[i1];
	outLines.back().color = ColorRGBA32(0xffffffff);
}

inline int GetStaticObjectCount (const Umbra::Tome* tome)
{
	return tome == NULL ? 0 : tome->getObjectCount();
}

void DrawUmbraLinesGizmoImmediate ()
{
	if (!GetOcclusionCullingVisualization()->GetShowOcclusionCulling())
		return;
	
	// Get visualization camera, if previsualizing -> we actually need the camera, otherwise show nothing
	Camera* camera = FindPreviewOcclusionCamera();
	if (!GetOcclusionCullingVisualization()->GetShowPreVis() && camera == NULL)
		return;
	
	dynamic_array<gizmos::ColorVertex> vertices;
	UmbraDebugRenderer renderer(vertices);
	
	CameraCullingParameters parameters (*camera, kCullFlagForceEvenIfCameraIsNotActive);
	if (GetOcclusionCullingVisualization()->GetShowGeometryCulling() && GetScene().GetUmbraTome().tome)
	{
		parameters.cullFlag |= kCullFlagOcclusionCull;
		parameters.umbraDebugRenderer = &renderer;
	}
		
	if (GetOcclusionCullingVisualization()->GetShowPortals())
	{
		if (!GetOcclusionCullingVisualization()->GetShowPreVis())		
		{
			CullResults results;
			parameters.umbraDebugFlags = Umbra::Query::DEBUGFLAG_PORTALS;
			camera->CustomCull(parameters, results);

			DrawUmbraQuadsGizmoImmediate_Draw(vertices);

			vertices.resize_uninitialized(0);
		}
	}

	if (GetOcclusionCullingVisualization()->GetShowVisibilityLines())
	{
		if (!GetOcclusionCullingVisualization()->GetShowPreVis())		
		{
			CullResults results;
			parameters.umbraDebugFlags = Umbra::Query::DEBUGFLAG_VISIBILITY_LINES;
			camera->CustomCull(parameters, results);

			DrawUmbraLinesGizmoImmediate_Draw(vertices);
			vertices.resize_uninitialized(0);
		}
	}

	if (GetOcclusionCullingVisualization()->GetShowViewVolumes())
	{
		if (GetOcclusionCullingVisualization()->GetShowPreVis())
		{
			OcclusionCullingTask::GetUmbraPreVisualization(vertices);
			DrawUmbraLinesGizmoImmediate_Draw(vertices);
			vertices.resize_uninitialized(0);
		}
		else
		{
			CullResults results;
			parameters.umbraDebugFlags = Umbra::Query::DEBUGFLAG_VIEWCELL;
			camera->CustomCull(parameters, results);
			
			DrawUmbraLinesGizmoImmediate_Draw(vertices);
			
			vertices.resize_uninitialized(0);
		}
	}

	/*
	// Dynamic object bound visualization
	if (GetOcclusionCullingVisualization()->GetShowDynamicObjectBounds() && GetScene().GetUmbraTome())
	{	
		Unity::Scene& scene = GetScene();
		dynamic_array<gizmos::ColorVertex> boundLines;
		int staticObjectCount = GetStaticObjectCount(scene.GetUmbraTome());
		int totalObjectCount = scene.GetNumRenderers();
		for (int objectIndex = staticObjectCount; objectIndex < totalObjectCount; objectIndex++)
		{
			if (scene.GetSavedVisibilityResult(objectIndex))
			{
				const AABB& bounds = GetScene().GetRendererAABB(objectIndex);
				Vector3f outVertices[8];
				bounds.GetVertices(outVertices);
				
				//    6-----7
				//   /     /|
				//  2-----3 |
				//  | 4   | 5
				//  |     |/
				//  0-----1

				pushBackLine(boundLines, outVertices, 0, 1);
				pushBackLine(boundLines, outVertices, 0, 2);
				pushBackLine(boundLines, outVertices, 1, 3);
				pushBackLine(boundLines, outVertices, 2, 3);

				pushBackLine(boundLines, outVertices, 2, 6);
				pushBackLine(boundLines, outVertices, 3, 7);
				pushBackLine(boundLines, outVertices, 0, 4);
				pushBackLine(boundLines, outVertices, 1, 5);

				pushBackLine(boundLines, outVertices, 4, 5);
				pushBackLine(boundLines, outVertices, 4, 6);
				pushBackLine(boundLines, outVertices, 5, 7);
				pushBackLine(boundLines, outVertices, 6, 7);
			}
		}
		DrawUmbraLinesGizmoImmediate_Draw(boundLines);
	}
	*/
}
