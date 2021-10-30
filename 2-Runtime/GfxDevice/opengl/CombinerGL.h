#pragma once

namespace ShaderLab {
	struct TextureBinding;
	class PropertySheet;
}


struct TextureCombinersGL
{
	static bool IsCombineModeSupported( unsigned int combiner );
	static TextureCombinersGL* Create( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props );

	int count;
	const ShaderLab::TextureBinding* texEnvs;
};

void ApplyCombinerGL( unsigned int& currentCombColor, unsigned int& currentCombAlpha, unsigned int combcolor, unsigned int combalpha );
