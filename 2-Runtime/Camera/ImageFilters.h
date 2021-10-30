#pragma once

#include "Renderable.h"

class RenderTexture;
class Texture;
class Vector2f;
class GfxDevice;
namespace Unity { class Material; }


// Image filters functionality. Only used internally by the camera (and other minor places).
class ImageFilters
{
public:
	ImageFilters() : m_FirstTargetTexture(NULL), m_SecondTargetTexture(NULL), m_FinalTargetTexture(NULL) { }

	void AddImageFilter (const ImageFilter& filter);
	void RemoveImageFilter (const ImageFilter& filter);
	bool HasImageFilter() const { return !(m_AfterOpaque.empty() && m_AfterEverything.empty()); }
	bool HasAfterOpaqueFilters() const { return !m_AfterOpaque.empty(); }

	void DoRender (RenderTexture* finalRT, bool forceIntoRT, bool afterOpaque, bool usingScreenToComposite, bool hdr = false);
	void Prepare (bool forceIntoRT, bool hdr = false, int antiAliasing = 1);
	RenderTexture* GetTargetBeforeOpaque ();
	RenderTexture* GetTargetAfterOpaque (bool forceIntoRT, bool usingScreenToComposite);
	RenderTexture* GetTargetFinal ();
	RenderTexture* SwitchTargetToLDR (RenderTexture* oldRt, bool requestLinear);
	void ReleaseTargetForLDR (RenderTexture** oldRt);

	static void Blit (Texture* source, RenderTexture* dest);
	static void Blit (Texture* source, RenderTexture* dest, Unity::Material* mat, int pass, bool setRT);
	static void BlitMultiTap (Texture* source, RenderTexture* dest, Unity::Material* mat, int count, const Vector2f* offsets);
	static void DrawQuadNoGPUTimestamp (GfxDevice& device, bool invertY, float uvX, float uvY);
	static void DrawQuad (GfxDevice& device, bool invertY, float uvX, float uvY);
private:
	static void SetCurrentRenderTarget (RenderTexture* dest, UInt32 flags);

private:
	typedef std::vector<ImageFilter> Filters;
	Filters			m_AfterOpaque;
	Filters			m_AfterEverything;
	RenderTexture*	m_FirstTargetTexture; // has color & depth
	RenderTexture*	m_SecondTargetTexture; // has color only, reuses depth from first
	RenderTexture*	m_FinalTargetTexture;
};
