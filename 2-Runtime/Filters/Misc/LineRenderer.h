#ifndef LINERENDERER_H
#define LINERENDERER_H

#include <vector>
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Geometry/AABB.h"
#include "LineBuilder.h"



// Renders a freeform texture/colored line in 3D space.
// (heavily based on TrailRenderer code, so most comments apply to both)
class LineRenderer : public Renderer {
public:
	REGISTER_DERIVED_CLASS (LineRenderer, Renderer)
	DECLARE_OBJECT_SERIALIZE (LineRenderer)

	LineRenderer (MemLabelId label, ObjectCreationMode mode);

	virtual void Reset ();

	virtual void Render (int materialIndex, const ChannelAssigns& channels);

	// Can operate in either local or world space, so we need to fill whole transform info ourselves
	virtual void UpdateTransformInfo();

	void SetPosition (int index, const Vector3f& position);
	
	void SetVertexCount(int count);

	void SetColors(const ColorRGBAf& c0, const ColorRGBAf& c1) { m_Parameters.color1 = c0; m_Parameters.color2 = c1; SetDirty(); }
	
	void SetWidth(float startWidth,float endWidth)
	{
		m_Parameters.startWidth = startWidth;
		m_Parameters.endWidth = endWidth;
		BoundsChanged();
		SetDirty();
	}
	
	bool GetUseWorldSpace () { return m_UseWorldSpace; }
	void SetUseWorldSpace (bool space);

	void AwakeFromLoad(AwakeFromLoadMode mode);
	static void InitializeClass ();

protected:
	// from Renderer
	virtual void UpdateRenderer();

private:
//	bool             m_BoundsDirty;
	bool             m_UseWorldSpace;							///< Draw lines in worldspace (or localspace)
	LineParameters   m_Parameters;
	typedef UNITY_VECTOR(kMemRenderer,Vector3f) PositionVector;
	PositionVector m_Positions;
};

#endif
