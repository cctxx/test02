#include "UnityPrefix.h"
#include "Polygon2D.h"

#if ENABLE_SPRITES
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Graphics/SpriteFrame.h"

Polygon2D::Polygon2D()
{
	Reset();
}

void Polygon2D::SetPoints (const Vector2f* points, size_t count)
{
	m_Paths.resize(1, TPath(kMemPhysics));
	m_Paths[0].clear();
	m_Paths[0].assign(points, points + count);
}

void Polygon2D::SetPathCount (int pathCount)
{
	m_Paths.resize(pathCount);
}

void Polygon2D::SetPath (int index, const TPath& path)
{
	if (index == 0 && GetPathCount() == 0)
	{
		m_Paths.resize(1);
	}
	else if (index >= GetPathCount())
	{
		ErrorString("Failed setting path. Index is out of bounds.");
		return;
	}

	m_Paths[index] = path;
}

void Polygon2D::CopyFrom (const Polygon2D& paths)
{
	const int pathCount = paths.GetPathCount ();
	if (pathCount == 0)
	{
		m_Paths.clear ();
		return;
	}

	// Transfer paths.
	m_Paths.resize (pathCount);
	for (int index = 0; index < pathCount; ++index)
		m_Paths[index] = paths.GetPath (index);
}

void Polygon2D::GenerateFrom(Sprite* sprite, const Vector2f& offset, float detail, unsigned char alphaTolerance, bool holeDetection)
{
	m_Paths.clear();
	sprite->GenerateOutline(detail, alphaTolerance, holeDetection, m_Paths, 0);
	
	if (offset.x != 0.0f || offset.y != 0.0f)
	{
		for (TPaths::iterator pit = m_Paths.begin(); pit != m_Paths.end(); ++pit)
		{
			TPath& path = *pit;
			for (TPath::iterator it = path.begin(); it != path.end(); ++it)
			{
				Vector2f& point = *it;
				point += offset;
			}
		}
	}
}

void Polygon2D::Reset()
{
	m_Paths.resize(1);
	m_Paths[0].clear();
	m_Paths[0].reserve(4);
	m_Paths[0].push_back(Vector2f(-1, -1));
	m_Paths[0].push_back(Vector2f(-1,  1));
	m_Paths[0].push_back(Vector2f( 1,  1));
	m_Paths[0].push_back(Vector2f( 1, -1));
}

bool Polygon2D::GetNearestPoint(const Vector2f& point, int& pathIndex, int& pointIndex, float& distance) const
{
	bool ret = false;
	float sqrDist = std::numeric_limits<float>::max();

	const int pathCount = m_Paths.size();
	for (int i = 0; i < pathCount; ++i)
	{
		const Polygon2D::TPath& path = m_Paths[i];
		const int pointCount = path.size();
		for (int j = 0; j < pointCount; ++j)
		{
			const Vector2f& testPoint = path[j];
			float d = SqrMagnitude(testPoint - point);
			if (d < sqrDist)
			{
				sqrDist = d;
				ret = true;

				pathIndex = i;
				pointIndex = j;
				distance = SqrtImpl(d);
			}
		}
	}

	return ret;
}

bool Polygon2D::GetNearestEdge(const Vector2f& point, int& pathIndex, int& pointIndex0, int& pointIndex1, float& distance, bool loop) const
{
	bool ret = false;
	float dist = std::numeric_limits<float>::max();

	const int pathCount = m_Paths.size();
	for (int i = 0; i < pathCount; ++i)
	{
		const Polygon2D::TPath& path = m_Paths[i];
		const int pointCount = path.size();
		const int edgeCount = loop ? pointCount : pointCount - 1;
		for (int p0 = 0; p0 < edgeCount; ++p0)
		{
			int p1 = (p0 + 1) % pointCount;
			float d = DistancePointLine<Vector2f>(point, path[p0], path[p1]);
			if (d < dist)
			{
				dist = d;
				ret = true;

				pathIndex = i;
				pointIndex0 = p0;
				pointIndex1 = p1;
				distance = d;
			}
		}
	}

	return ret;
}

#endif //ENABLE_SPRITES
