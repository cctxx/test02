#ifndef DEBUG_PRIMITIVES_H
#define DEBUG_PRIMITIVES_H

#include "UnityPrefix.h"
#if ENABLE_MULTITHREADED_CODE
#include "Runtime/Threads/SimpleLock.h"
#endif
#include "Runtime/Math/Color.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Camera/Camera.h"

// container for storing/drawing debug primitives to the editor scene view (like the grid)
struct DebugAABB
{
	DebugAABB(const AABB& aabb, const ColorRGBAf& color,int frame, int tag):m_AABB(aabb),m_Color(color),m_Frame(frame),m_Tag(tag) {};
	int m_Frame;
	int m_Tag;
	AABB m_AABB;
	ColorRGBAf m_Color;
};
struct DebugLine
{
	DebugLine(const Vector3f& start, const Vector3f& end, const ColorRGBAf& color, int frame, int tag):m_Start(start),m_End(end),m_Color(color),m_Frame(frame),m_Tag(tag) {};
	int m_Frame;
	int m_Tag;
	Vector3f m_Start;
	Vector3f m_End;
	ColorRGBAf m_Color;
};
struct DebugPoint
{
	DebugPoint(const Vector3f& point, const ColorRGBAf& color, int frame, int tag):m_Point(point),m_Color(color),m_Frame(frame),m_Tag(tag) {};
	int m_Frame;
	int m_Tag;
	Vector3f m_Point;
	ColorRGBAf m_Color;
};

class DebugPrimitives : public NonCopyable
{
public:
	/// Singleton access
	static DebugPrimitives* Get();

	void Draw(Camera* camera) const;
	void Clear();
	void AddAABB(const AABB& aabb, const ColorRGBAf& color, int tag = -1);
	void AddLine(const Vector3f& start, const Vector3f& end, const ColorRGBAf& color, int tag = -1);
	void AddPoint(const Vector3f& point, const ColorRGBAf& color, int tag = -1);
	void LogAABBs() const;
	void LogLines() const;
	void LogPoints() const;
	void Log() const;
private:
#if ENABLE_MULTITHREADED_CODE
	mutable SimpleLock m_Lock;
#endif
	dynamic_array<DebugAABB> m_AABBs;
	dynamic_array<DebugLine> m_Lines;
	dynamic_array<DebugPoint> m_Points;
};

#endif
