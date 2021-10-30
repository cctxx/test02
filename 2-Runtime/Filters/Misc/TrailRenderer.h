#ifndef TRAILRENDERER_H
#define TRAILRENDERER_H

#include <list>
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Math/Gradient.h"
#include "Runtime/Math/Vector3.h"
#include "LineBuilder.h"
#include "Runtime/Geometry/AABB.h"



// Renders a trail after an object.
// This is good for smoke trails after missiles or a visual FX after strong lights.
// @todo make it work so we track movement over a Cameras screen space.
class TrailRenderer : public Renderer {
public:
	REGISTER_DERIVED_CLASS (TrailRenderer, Renderer)
	DECLARE_OBJECT_SERIALIZE (TrailRenderer)

	TrailRenderer (MemLabelId label, ObjectCreationMode mode);

	virtual void Reset ();

	virtual void Render (int materialIndex, const ChannelAssigns& channels);

	// Hook up to TransformChanged
	static void InitializeClass ();
	
	// TransformChanged message handler
	void TransformChanged (int changeMask);
	
	virtual void UpdateTransformInfo();
	
	GET_SET_DIRTY(float, Time, m_Time)
	GET_SET_DIRTY(float, MinVertexDistance, m_MinVertexDistance)
	GET_SET_DIRTY(float, StartWidth, m_LineParameters.startWidth)
	GET_SET_DIRTY(float, EndWidth, m_LineParameters.endWidth)
	GET_SET_DIRTY(bool, Autodestruct, m_Autodestruct)

protected:
	// from Renderer
	virtual void UpdateRenderer();
	
private:
	float GetHalfMaxLineWidth () const;

	bool				m_TransformChanged;				// Has the transform changed since last render?
	bool				m_WasRendered;					// Trail was rendered so enable autodestruct
	std::list<Vector3f>	m_Positions;					// The positions for each of the centers
	std::list<float>	m_TimeStamps;					// The timestamp for each position
	int					m_CurrentLength;				// The current length in vertices
	MinMaxAABB			m_AABB;							// The size in world coords

	GradientDeprecated<k3DLineGradientSize> m_Colors;
	LineParameters		m_LineParameters;
	float				m_Time;							///< How long the tail should be (seconds). { 0, infinity}
	float				m_MinVertexDistance;			///< The minimum distance to spawn a new point on the trail range { 0, infinity}
	bool				m_Autodestruct;					///< Destroy GameObject when there is no trail?
};

#endif
