#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_SPRITES
#include "Runtime/Graphics/Polygon2D.h"

// Note: current PolygonEditor relies on basic Polygon2D functionality and has O(n) complexity in most cases.
//       To improve performance we might want to generate a spatial database for the geometry.
class PolygonEditor
{
public:
	static PolygonEditor& Get();
	
	void StartEditing(const Polygon2D& polygon);
	void StopEditing();

	bool GetNearestPoint(const Vector2f& point, int& pathIndex, int& pointIndex, float& distance);
	bool GetNearestEdge(const Vector2f& point, int& pathIndex, int& pointIndex0, int& pointIndex1, float& distance, bool loop);
	int  GetPathCount();
	int  GetPointCount(int pathIndex);
	bool GetPoint(int pathIndex, int pointIndex, Vector2f& point);
	void SetPoint(int pathIndex, int pointIndex, const Vector2f& point);
	void InsertPoint(int pathIndex, int pointIndex, const Vector2f& point);
	void RemovePoint(int pathIndex, int pointIndex);
	void TestPointMove(int pathIndex, int pointIndex, const Vector2f& movePosition, bool& leftIntersect, bool& rightIntersect, bool loop);

	Polygon2D& GetPoly() { return m_Polygon; }

private:
	Polygon2D m_Polygon;

	bool EdgeIntersectsOtherEdges(const Vector2f& v0, const Vector2f& v1, int ignorePath, int ignorePoint, bool loop);
};

#endif // ENABLE_SPRITES
