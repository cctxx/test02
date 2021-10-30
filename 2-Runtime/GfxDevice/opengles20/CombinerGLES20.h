#pragma once

namespace ShaderLab { struct TextureBinding; }

struct TextureCombinersGLES2
{
	static TextureCombinersGLES2* Create (int count, const ShaderLab::TextureBinding* texEnvs);
	int count;
	const ShaderLab::TextureBinding* texEnvs;
};
