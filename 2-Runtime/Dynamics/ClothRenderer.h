#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_CLOTH

#include "Runtime/Filters/Renderer.h"

class VBO;

namespace Unity { class Cloth; }


class ClothRenderer : public Renderer
{
public:	
	REGISTER_DERIVED_CLASS (ClothRenderer, Renderer)
	DECLARE_OBJECT_SERIALIZE (ClothRenderer)

	ClothRenderer (MemLabelId label, ObjectCreationMode mode);
	
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	virtual void Reset ();	
	virtual void Render (int/* subsetIndex*/, const ChannelAssigns& channels);
	
	virtual void UpdateTransformInfo();
	
	virtual void RendererBecameVisible();
	virtual void RendererBecameInvisible();
	
	bool GetPauseWhenNotVisible() const { return m_PauseWhenNotVisible; }
	void SetPauseWhenNotVisible(bool pause) { SetDirty(); m_PauseWhenNotVisible = pause; }

	void UnloadVBOFromGfxDevice();
	void ReloadVBOToGfxDevice();
	
private:
	
	void UpdateClothVBOImmediate (int requiredChannels, UInt32& unavailableChannels);
	void UpdateClothVerticesFromPhysics ();
	void UpdateAABB ();
		
	VBO* m_VBO;
	UInt32 m_ChannelsInVBO;
	UInt32 m_UnavailableInVBO;
	AABB m_AABB;
	bool m_PauseWhenNotVisible;
	bool m_IsPaused;
	bool m_AABBDirty;
	
	friend class DeformableMesh;
};

#endif // ENABLE_CLOTH
