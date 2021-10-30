#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_SPRITES

#include "Runtime/Math/Vector2.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Serialize/SerializeUtility.h"

class Sprite;

class Polygon2D
{
public:
	DECLARE_SERIALIZE_NO_PPTR (Polygon2D)
	
	typedef dynamic_array<Vector2f> TPath;
	typedef std::vector<TPath> TPaths;

	Polygon2D();

	bool IsEmpty() const { return (m_Paths.size() == 0 || m_Paths[0].size() < 3); }
	void Reset();
	void Clear() { m_Paths.clear(); }

	// Simple API
	void SetPoints (const Vector2f* points, size_t count);
	const Vector2f* GetPoints() const { return (m_Paths.size() > 0) ? m_Paths[0].data() : NULL; }
	size_t GetPointCount() const { return (m_Paths.size() > 0) ? m_Paths[0].size() : 0; }

	// Advanced API
	void SetPathCount(int pathCount);
	int GetPathCount() const { return m_Paths.size(); }
	
	void SetPath(int index, const TPath& path);
	const TPath& GetPath(int index) const { return m_Paths[index]; }
	TPath& GetPath(int index) { return m_Paths[index]; }

	void CopyFrom (const Polygon2D& paths);

	size_t GetTotalPointCount() const
	{
		size_t count = 0;
		for (size_t i = 0; i < m_Paths.size(); ++i)
			count += m_Paths[i].size();
		return count;
	}

	// Generation
	void GenerateFrom(Sprite* sprite, const Vector2f& offset, float detail, unsigned char alphaTolerance, bool holeDetection);

	// Math
	bool GetNearestPoint(const Vector2f& point, int& pathIndex, int& pointIndex, float& distance) const;
	bool GetNearestEdge(const Vector2f& point, int& pathIndex, int& pointIndex0, int& pointIndex1, float& distance, bool loop) const;

private:
	TPaths m_Paths;
};

template<class TransferFunction>
void Polygon2D::Transfer(TransferFunction& transfer)
{
	TRANSFER(m_Paths);
}

#endif //ENABLE_SPRITES
