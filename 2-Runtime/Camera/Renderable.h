#ifndef RENDERABLE_H
#define RENDERABLE_H

class RenderTexture;
namespace Unity { class Component; }
struct CullResults;

class Renderable {
public:
	virtual void RenderRenderable (const CullResults& cullResults) = 0;
};

typedef void RenderImageFilterFunc (Unity::Component* component, RenderTexture* source, RenderTexture* dest);

struct ImageFilter
{
	Unity::Component* component;
	RenderImageFilterFunc* renderFunc;

	bool transformsToLDR;
	bool afterOpaque;

	ImageFilter (Unity::Component* inComponent, RenderImageFilterFunc* inRenderFunc, bool inLDR, bool inAfterOpaque)
		: component(inComponent), renderFunc(inRenderFunc), afterOpaque(inAfterOpaque),  transformsToLDR(inLDR){ }

	bool operator==(const ImageFilter& o) const { return component==o.component && renderFunc==o.renderFunc; }
	bool operator!=(const ImageFilter& o) const { return component!=o.component || renderFunc!=o.renderFunc; }
};

#endif
