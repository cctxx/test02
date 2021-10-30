#include "PolygonEditor.h"

#if ENABLE_SPRITES

static PolygonEditor gSingleton;
PolygonEditor& PolygonEditor::Get()
{
	return gSingleton;
}

void PolygonEditor::StartEditing(const Polygon2D& polygon)
{
	m_Polygon = polygon;
}

void PolygonEditor::StopEditing()
{
	m_Polygon.Clear();
}

bool PolygonEditor::GetNearestPoint(const Vector2f& point, int& pathIndex, int& pointIndex, float& distance)
{
	return m_Polygon.GetNearestPoint(point, pathIndex, pointIndex, distance);
}

bool PolygonEditor::GetNearestEdge(const Vector2f& point, int& pathIndex, int& pointIndex0, int& pointIndex1, float& distance, bool loop)
{
	return m_Polygon.GetNearestEdge(point, pathIndex, pointIndex0, pointIndex1, distance, loop);
}

int PolygonEditor::GetPathCount()
{
	return m_Polygon.GetPathCount();
}

int PolygonEditor::GetPointCount(int pathIndex)
{
	return m_Polygon.GetPath(pathIndex).size();
}

bool PolygonEditor::GetPoint(int pathIndex, int pointIndex, Vector2f& point)
{
	point = m_Polygon.GetPath(pathIndex)[pointIndex];
	return true;
}

void PolygonEditor::SetPoint(int pathIndex, int pointIndex, const Vector2f& point)
{
	m_Polygon.GetPath(pathIndex)[pointIndex] = point;
}

void PolygonEditor::InsertPoint(int pathIndex, int pointIndex, const Vector2f& point)
{
	Polygon2D::TPath& path = m_Polygon.GetPath(pathIndex);
	Polygon2D::TPath::iterator it = path.begin();
	for (int i = 0; i < pointIndex; ++i)
		++it;
	path.insert(it, point);
}

void PolygonEditor::RemovePoint(int pathIndex, int pointIndex)
{
	Polygon2D::TPath& path = m_Polygon.GetPath(pathIndex);
	Polygon2D::TPath::iterator it = path.begin();
	for (int i = 0; i < pointIndex; ++i)
		++it;
	path.erase(it);
}

static bool LineSegmentIntersection(const Vector2f& p1, const Vector2f& p2, const Vector2f& p3, const Vector2f& p4, Vector2f& result)
{
	float bx = p2.x - p1.x;
	float by = p2.y - p1.y;
	float dx = p4.x - p3.x;
	float dy = p4.y - p3.y;
	float bDotDPerp = bx * dy - by * dx;
	if (bDotDPerp == 0.0f)
	{
		return false;
	}
	float cx = p3.x - p1.x;
	float cy = p3.y - p1.y;
	float t = (cx * dy - cy * dx) / bDotDPerp;
	if (t < 0.0f || t > 1.0f)
	{
		return false;
	}
	float u = (cx * by - cy * bx) / bDotDPerp;
	if (u < 0.0f || u > 1.0f)
	{
		return false;
	}
	result = Vector2f(p1.x + t * bx, p1.y + t * by);
	return true;
}

bool PolygonEditor::EdgeIntersectsOtherEdges(const Vector2f& v0, const Vector2f& v1, int ignorePath, int ignorePoint, bool loop)
{
	const int pathCount = m_Polygon.GetPathCount();
	for (int i = 0; i < pathCount; ++i)
	{
		Polygon2D::TPath& path = m_Polygon.GetPath(i);
		const int pointCount = path.size();
		const int edgeCount = loop ? pointCount : pointCount - 1;
		for (int p0 = 0; p0 < edgeCount; ++p0)
		{
			int p1 = (p0 + 1) % pointCount;

			if (ignorePath == i)
				if (ignorePoint == p0 || ignorePoint == p1)
					continue;

			Vector2f isect;
			if (LineSegmentIntersection(v0, v1, path[p0], path[p1], isect))
			{
				if (CompareApproximately(v0, isect) || CompareApproximately(v1, isect)) // Ignore segment ends
					continue;
				return true;
			}
		}
	}

	return false;
}

void PolygonEditor::TestPointMove(int pathIndex, int pointIndex, const Vector2f& movePosition, bool& leftIntersect, bool& rightIntersect, bool loop)
{
	Polygon2D::TPath& path = m_Polygon.GetPath(pathIndex);
	const int pointCount = path.size();
	bool testLeft = true;
	bool testRight = true;

	int p0 = pointIndex - 1;
	if (p0 < 0)
	{
		p0 = pointCount - 1;
		testLeft = loop;
	}
	int p1 = pointIndex + 1;
	if (p1 == pointCount)
	{
		p1 = 0;
		testRight = loop;
	}

	if (testLeft)
		leftIntersect = EdgeIntersectsOtherEdges(path[pointIndex], path[p0], pathIndex, pointIndex, loop);
	else
		leftIntersect = false;

	if (testRight)
	{
		rightIntersect = EdgeIntersectsOtherEdges(path[pointIndex], path[p1], pathIndex, pointIndex, loop);
	}
	else
		rightIntersect = false;
}

#endif // ENABLE_SPRITES
