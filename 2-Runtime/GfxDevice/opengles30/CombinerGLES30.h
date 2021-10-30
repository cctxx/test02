#pragma once

namespace ShaderLab { struct TextureBinding; }

struct TextureCombinersGLES3
{
	static TextureCombinersGLES3* Create (int count, const ShaderLab::TextureBinding* texEnvs);
	int count;
	const ShaderLab::TextureBinding* texEnvs;
};
