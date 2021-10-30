#ifndef SHADER_TAGS_H
#define SHADER_TAGS_H

// The pass type of a given pass.
// The shader sets this as a hint to the app as to how it wants to be rendered using the LightMode tag
enum ShaderPassType {
	kPassAlways,			// Always executed. No lighting.
	kPassVertex,			// Vertex lights + ambient
	kPassVertexLM,			// Vertex lighting w/ lightmaps
	kPassVertexLMRGBM,		// Vertex lighting w/ lightmaps encoded as RGBM
	kPassForwardBase,
	kPassForwardAdd,
	kPassLightPrePassBase,
	kPassLightPrePassFinal,
	kPassShadowCaster,		// Renders object as shadow caster
	kPassShadowCollector,	// Renders object, collecting shadows into screen space texture
	kShaderPassTypeCount 	// Keep this last!
};


enum ShaderRenderOptions {
	kShaderOptionSoftVegetation = 0, // soft vegetation is currently on
	kShaderRenderOptionCount // keep this last!
};


#endif
