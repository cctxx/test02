#include "UnityPrefix.h"
#include "Editor/Src/Utility/DebugPrimitives.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Input/TimeManager.h"

void DrawDebugAABBs( const Camera* camera, Material* s_GridMaterial, const DebugAABB* aabbs, size_t numAABBs )
{
	AssertIf(camera==0);
	
	// render the lines
	for( int passI = 0 ; passI < s_GridMaterial->GetPassCount () ; ++passI )
	{
		s_GridMaterial->SetPass(passI);
		GetGfxDevice().ImmediateBegin(kPrimitiveLines);
		for ( size_t i = 0; i < numAABBs; ++i)
		{	
			const ColorRGBAf color = aabbs[i].m_Color;
			const Vector3f min = aabbs[i].m_AABB.GetMin();
			const Vector3f max = aabbs[i].m_AABB.GetMax();

			GetGfxDevice().ImmediateColor(color.r, color.g, color.b, color.a);

			// XYminZ quad
			GetGfxDevice().ImmediateVertex(min.x, min.y, min.z);
			GetGfxDevice().ImmediateVertex(max.x, min.y, min.z);

			GetGfxDevice().ImmediateVertex(max.x, min.y, min.z);
			GetGfxDevice().ImmediateVertex(max.x, max.y, min.z);

			GetGfxDevice().ImmediateVertex(max.x, max.y, min.z);
			GetGfxDevice().ImmediateVertex(min.x, max.y, min.z);

			GetGfxDevice().ImmediateVertex(min.x, max.y, min.z);
			GetGfxDevice().ImmediateVertex(min.x, min.y, min.z);

			// XYmaxZ quad
			GetGfxDevice().ImmediateVertex(min.x, min.y, max.z);
			GetGfxDevice().ImmediateVertex(max.x, min.y, max.z);

			GetGfxDevice().ImmediateVertex(max.x, min.y, max.z);
			GetGfxDevice().ImmediateVertex(max.x, max.y, max.z);

			GetGfxDevice().ImmediateVertex(max.x, max.y, max.z);
			GetGfxDevice().ImmediateVertex(min.x, max.y, max.z);

			GetGfxDevice().ImmediateVertex(min.x, max.y, max.z);
			GetGfxDevice().ImmediateVertex(min.x, min.y, max.z);

			// Z lines
			GetGfxDevice().ImmediateVertex(min.x, min.y, min.z);
			GetGfxDevice().ImmediateVertex(min.x, min.y, max.z);

			GetGfxDevice().ImmediateVertex(max.x, min.y, min.z);
			GetGfxDevice().ImmediateVertex(max.x, min.y, max.z);

			GetGfxDevice().ImmediateVertex(max.x, max.y, min.z);
			GetGfxDevice().ImmediateVertex(max.x, max.y, max.z);

			GetGfxDevice().ImmediateVertex(min.x, max.y, min.z);
			GetGfxDevice().ImmediateVertex(min.x, max.y, max.z);
		}
		GetGfxDevice().ImmediateEnd();
	}
}

void DrawDebugLines( const Camera* camera, Material* s_GridMaterial, const DebugLine* lines, size_t numLines )
{
	AssertIf(camera==0);
	
	// render the lines
	for( int passI = 0 ; passI < s_GridMaterial->GetPassCount () ; ++passI )
	{
		s_GridMaterial->SetPass(passI);
		GetGfxDevice().ImmediateBegin(kPrimitiveLines);
		for ( size_t i = 0; i < numLines; ++i)
		{	
			const ColorRGBAf color = lines[i].m_Color;
			const Vector3f start = lines[i].m_Start;
			const Vector3f end = lines[i].m_End;
			GetGfxDevice().ImmediateColor(color.r, color.g, color.b, color.a);
			GetGfxDevice().ImmediateVertex(start.x, start.y, start.z);
			GetGfxDevice().ImmediateVertex(end.x, end.y, end.z);
		}
		GetGfxDevice().ImmediateEnd();
	}
}

void DrawDebugPoints( const Camera* camera, Material* s_GridMaterial, const DebugPoint* points, size_t numPoints )
{
	AssertIf(camera==0);
	
	// render the lines
	for( int passI = 0 ; passI < s_GridMaterial->GetPassCount () ; ++passI )
	{
		s_GridMaterial->SetPass(passI);
		GetGfxDevice().ImmediateBegin(kPrimitivePoints);
		for ( size_t i = 0; i < numPoints; ++i)
		{	
			const ColorRGBAf color = points[i].m_Color;
			const Vector3f point = points[i].m_Point;
			GetGfxDevice().ImmediateColor(color.r, color.g, color.b, color.a);
			GetGfxDevice().ImmediateVertex(point.x, point.y, point.z);
		}
		GetGfxDevice().ImmediateEnd();
	}
}

// container for storing/drawing debug primitives to the editor scene view
void DebugPrimitives::Draw(Camera* camera) const
{
	GetGfxDevice().SetAntiAliasFlag(true);

	// get the material 
	static PPtr<Material> s_GridMaterial = 0;
	if( !s_GridMaterial )
	{
		s_GridMaterial = dynamic_pptr_cast<Material*>(GetMainAsset ("Assets/Editor Default Resources/SceneView/Grid.mat"));

		if( !s_GridMaterial )
			s_GridMaterial = GetEditorAssetBundle ()->Get<Material> ("SceneView/Grid.mat");

		if( !s_GridMaterial )
			return;
	}

	SimpleLock::AutoLock lock(m_Lock);
	DrawDebugAABBs(camera, s_GridMaterial, m_AABBs.data(), m_AABBs.size());
	DrawDebugLines(camera, s_GridMaterial, m_Lines.data(), m_Lines.size());
	DrawDebugPoints(camera, s_GridMaterial, m_Points.data(), m_Points.size());

	GetGfxDevice().SetAntiAliasFlag(false);
}
void DebugPrimitives::Clear()
{
#if ENABLE_MULTITHREADED_CODE
	SimpleLock::AutoLock lock(m_Lock);
#endif
	m_AABBs.clear();
	m_Lines.clear();
}
void DebugPrimitives::AddAABB(const AABB& aabb,const ColorRGBAf& color, int tag)
{
#if ENABLE_MULTITHREADED_CODE
	SimpleLock::AutoLock lock(m_Lock);
#endif
	const int frame = GetTimeManager().GetRenderFrameCount();
	m_AABBs.push_back(DebugAABB(aabb,color,frame,tag));
}
void DebugPrimitives::AddLine(const Vector3f& start, const Vector3f& end, const ColorRGBAf& color, int tag)
{
#if ENABLE_MULTITHREADED_CODE
	SimpleLock::AutoLock lock(m_Lock);
#endif
	const int frame = GetTimeManager().GetRenderFrameCount();
	m_Lines.push_back(DebugLine(start,end,color,frame,tag));
}

void DebugPrimitives::AddPoint(const Vector3f& point, const ColorRGBAf& color, int tag)
{
#if ENABLE_MULTITHREADED_CODE
	SimpleLock::AutoLock lock(m_Lock);
#endif
	const int frame = GetTimeManager().GetRenderFrameCount();
	m_Points.push_back(DebugPoint(point,color,frame,tag));
}

void DebugPrimitives::LogAABBs() const
{
#if ENABLE_MULTITHREADED_CODE
	SimpleLock::AutoLock lock(m_Lock);
#endif
	for ( size_t i = 0; i < m_AABBs.size(); ++i)
	{	
		const Vector3f start = m_AABBs[i].m_AABB.GetMin();
		const Vector3f end = m_AABBs[i].m_AABB.GetMin();
		const int frame = m_AABBs[i].m_Frame;
		const int tag = m_AABBs[i].m_Tag;
		printf_console("Line %d [frame: %d, tag: %d] -> min: [ %f ; %f ; %f ] -> max: [ %f ; %f ; %f ].\n", i, frame, tag, start.x, start.y, start.z, end.x, end.y, end.z );
	}
}

void DebugPrimitives::LogLines() const
{
#if ENABLE_MULTITHREADED_CODE
	SimpleLock::AutoLock lock(m_Lock);
#endif
	for ( size_t i = 0; i < m_Lines.size(); ++i)
	{	
		const Vector3f start = m_Lines[i].m_Start;
		const Vector3f end = m_Lines[i].m_End;
		const int frame = m_Lines[i].m_Frame;
		const int tag = m_Lines[i].m_Tag;
		printf_console("Line %d [frame: %d, tag: %d] -> from: [ %f ; %f ; %f ] -> to: [ %f ; %f ; %f ].\n", i, frame, tag, start.x, start.y, start.z, end.x, end.y, end.z );
	}
}

void DebugPrimitives::LogPoints() const
{
#if ENABLE_MULTITHREADED_CODE
	SimpleLock::AutoLock lock(m_Lock);
#endif
	for ( size_t i = 0; i < m_Points.size(); ++i)
	{	
		const Vector3f point = m_Points[i].m_Point;
		const int frame = m_Points[i].m_Frame;
		const int tag = m_Points[i].m_Tag;
		printf_console("Point %d [frame: %d, tag: %d] -> point: [ %f ; %f ; %f ].\n", i, frame, tag, point.x, point.y, point.z );
	}
}

void DebugPrimitives::Log() const
{
	LogAABBs();
	LogLines();
	LogPoints();
}

DebugPrimitives gDebugPrimitivesInterface;

DebugPrimitives* DebugPrimitives::Get ()
{
	return &gDebugPrimitivesInterface;
}
