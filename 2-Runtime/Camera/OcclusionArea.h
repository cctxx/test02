#ifndef OCCLUSION_AREA_H
#define OCCLUSION_AREA_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Vector3.h"

class OcclusionArea : public Unity::Component {
public:
	OcclusionArea ();
	
	REGISTER_DERIVED_CLASS (OcclusionArea, Component)
	DECLARE_OBJECT_SERIALIZE (OcclusionArea)    

	OcclusionArea (MemLabelId label, ObjectCreationMode mode);

	virtual void Reset();

	void SetCenter (const Vector3f& center) { m_Center = center; SetDirty(); }
	const Vector3f& GetCenter () const { return m_Center; }
	
	void SetSize (const Vector3f& size) { m_Size = size; SetDirty(); }
	const Vector3f& GetSize () const { return m_Size; }

	void SetViewVolume (bool isViewVolume) { m_IsViewVolume = isViewVolume; SetDirty(); }
	bool GetViewVolume	() const { return m_IsViewVolume; }

	Vector3f GetGlobalExtents () const;
	Vector3f GetGlobalCenter () const;
	
	static void InitializeClass ();
	static void CleanupClass () { }

private:
	
	Vector3f	        m_Size;
	Vector3f	        m_Center;
	
	bool                m_IsViewVolume;
	
//	bool                m_OverrideResolution;
//	float               m_SmallestVoxelSize;
//	float               m_SmallestHoleSize;
//	float               m_BackfaceCulling;
};

#endif
