#ifndef SPRITERENDERER_H
#define SPRITERENDERER_H
#include "Configuration/UnityConfigure.h"

#if ENABLE_SPRITES

#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Graphics/SpriteFrame.h"

class SpriteRenderer : public Renderer
{
public:
	REGISTER_DERIVED_CLASS (SpriteRenderer, Renderer)
	DECLARE_OBJECT_SERIALIZE (SpriteRenderer)
	
	SpriteRenderer (MemLabelId label, ObjectCreationMode mode);
	// ~SpriteRenderer ();	 declared-by-macro
	
	static bool IsSealedClass () { return true; }
	static void InitializeClass ();
	static void CleanupClass ();

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	virtual void SmartReset ();

	virtual void UpdateTransformInfo();
	virtual void UpdateLocalAABB ();
	virtual void Render (int materialIndex, const ChannelAssigns& channels);
#if GFX_ENABLE_DRAW_CALL_BATCHING
	static void RenderMultiple (const BatchInstanceData* instances, size_t count, const ChannelAssigns& channels);
#endif
	PPtr<Sprite> GetSprite() const { return m_Sprite; }
	void SetSprite(PPtr<Sprite> sprite);

	ColorRGBAf GetColor() const { return m_Color; }
	void SetColor(const ColorRGBAf& color) { m_Color = color; }

	static void SetupMaterialPropertyBlock(MaterialPropertyBlock& block, const Texture2D* spriteTexture);

	static Material* GetDefaultSpriteMaterial();

private:
	PPtr<Sprite> m_Sprite;
	ColorRGBAf   m_Color;

	void SetupMaterialProperties();
	void GetGeometrySize(UInt32& indexCount, UInt32& vertexCount);
	
#if GFX_ENABLE_DRAW_CALL_BATCHING
	static void RenderBatch (const BatchInstanceData* instances, size_t count, size_t numIndices, size_t numVertices, const ChannelAssigns& channels);
#endif
	// Context
	const SpriteRenderData* GetSpriteRenderDataInContext(const PPtr<Sprite>& frame);
};

#endif //ENABLE_SPRITES

#endif
