#pragma once
#ifndef RUNTIME_NAVMESH_PATH
#define RUNTIME_NAVMESH_PATH

#include "Runtime/Math/Vector3.h"
#include "NavMeshTypes.h"

struct OffMeshLinkData;

class NavMeshPath
{
public:
	enum
	{
		kMaxPathPolygons = 256
	};

	NavMeshPath ();
	~NavMeshPath ();

	inline Vector3f GetSourcePosition () const;
	inline void SetSourcePosition (const Vector3f& sourcePosition);
	inline Vector3f GetTargetPosition () const;
	inline void SetTargetPosition (const Vector3f& targetPosition);
	inline int GetPolygonCount () const;
	inline void SetPolygonCount (int polygonCount);

	inline unsigned int* GetPolygonPath ();
	inline const unsigned int* GetPolygonPath () const;

	inline NavMeshPathStatus GetStatus () const;
	inline void SetStatus (NavMeshPathStatus status);
	inline void SetTimeStamp (unsigned int timeStamp);

private:

	unsigned int m_timeStamp;
	NavMeshPathStatus m_status;
	unsigned int m_polygons[kMaxPathPolygons];
	int m_polygonCount;
	Vector3f m_sourcePosition;
	Vector3f m_targetPosition;
};

inline Vector3f NavMeshPath::GetSourcePosition () const
{
	return m_sourcePosition;
}
inline void NavMeshPath::SetSourcePosition (const Vector3f& sourcePosition)
{
	m_sourcePosition = sourcePosition;
}
inline Vector3f NavMeshPath::GetTargetPosition () const
{
	return m_targetPosition;
}
inline void NavMeshPath::SetTargetPosition (const Vector3f& targetPosition)
{
	m_targetPosition = targetPosition;
}
inline int NavMeshPath::GetPolygonCount () const
{
	return m_polygonCount;
}
inline void NavMeshPath::SetPolygonCount (int polygonCount)
{
	m_polygonCount = polygonCount;
}
inline unsigned int* NavMeshPath::GetPolygonPath ()
{
	return m_polygons;
}
inline const unsigned int* NavMeshPath::GetPolygonPath () const
{
	return m_polygons;
}
inline NavMeshPathStatus NavMeshPath::GetStatus () const
{
	return m_status;
}
inline void NavMeshPath::SetStatus (NavMeshPathStatus status)
{
	m_status = status;
}
inline void NavMeshPath::SetTimeStamp (unsigned int timeStamp)
{
	m_timeStamp = timeStamp;
}

#endif
