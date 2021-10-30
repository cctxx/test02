#include "UnityPrefix.h"
#include "CombinerGL.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "UnityGL.h"
#include "GLAssert.h"
#include "External/shaderlab/Library/pass.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

static const unsigned int kCombinerFuncTable[] = {
	GL_REPLACE, GL_MODULATE, GL_ADD, GL_ADD_SIGNED_ARB, GL_SUBTRACT_ARB, GL_INTERPOLATE_ARB, GL_DOT3_RGB_ARB, GL_DOT3_RGBA_ARB
};
static const unsigned int kCombinerBlendFuncTableATI[4] = {
	GL_INTERPOLATE_ARB, GL_MODULATE_ADD_ATI, GL_MODULATE_SIGNED_ADD_ATI, GL_MODULATE_SUBTRACT_ATI
};		
static const unsigned int kCombinerBlendFuncTableNV[4] = {
	GL_INTERPOLATE_ARB, GL_ADD, GL_ADD_SIGNED_EXT, 0
};		
static const unsigned int kCombinerSourceTable[4] = {
	GL_PREVIOUS_ARB, GL_TEXTURE, GL_CONSTANT_ARB, GL_PRIMARY_COLOR_ARB
};
static const unsigned int kCombinerOperandTable[4] = {
	GL_SRC_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA
};


bool TextureCombinersGL::IsCombineModeSupported( unsigned int combiner )
{
	int cf = COMBINER_GET_FUNC(combiner);
	if( cf & combiner::kBlendFuncMask )
	{
		int blendF = COMBINER_GET_BLEND_FUNC_INDEX(cf);
		if( blendF != 0 ) // anything other than GL_INTERPOLATE_ARB requires extension
		{
			if( gGraphicsCaps.gl.hasTextureEnvCombine3ATI )
				return true;
			if( gGraphicsCaps.gl.hasTextureEnvCombine3NV && blendF != 3 ) // NV fallback can't do a*b-c
				return true;
			return false;
		}
	}

	return true;
}

void ApplyCombinerGL( unsigned int& currentCombColor, unsigned int& currentCombAlpha, unsigned int combcolor, unsigned int combalpha )
{
	// only change combiner color setup if different from the cached version.	
	if( currentCombColor != combcolor )
	{
		// Handle RGB
		int s0 = (combcolor >> combiner::kSrcZeroShift) & 0xFF;
		int cf = COMBINER_GET_FUNC(combcolor);
		int s1 = (combcolor) & 0xFF;

		GLint source0 = kCombinerSourceTable[s0 & combiner::kSourceMask];
		GLint oper0 = kCombinerOperandTable[s0 >> combiner::kOperandShift];
		GLint source1 = kCombinerSourceTable[s1 & combiner::kSourceMask];
		GLint oper1 = kCombinerOperandTable[s1 >> combiner::kOperandShift];

		OGL_CALL(glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB));
		if( !(cf & combiner::kBlendFuncMask) )
		{
			OGL_CALL(glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, kCombinerFuncTable[cf]));
			OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, source0 ));
			OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, oper0 ));
			OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, source1 ));
			OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, oper1 ));
		}
		else
		{
			int blendF = COMBINER_GET_BLEND_FUNC_INDEX(cf);
			int src2 = cf & combiner::kSourceMask;
			int oper2 = ((cf & combiner::kOperandTwo) >> combiner::kOperandShift) | 1;
			if( blendF == 0 || gGraphicsCaps.gl.hasTextureEnvCombine3ATI )
			{
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, source0 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, oper0 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, source1 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, oper1 ));

				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, kCombinerBlendFuncTableATI[blendF] ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, kCombinerSourceTable[src2] ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, kCombinerOperandTable[oper2] ));
			}
			else
			{
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE4_NV ));

				// TNT combiners use a different argument order than ARB/ATI combiners...
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, source0 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, oper0 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, source1 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, oper1 ));

				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, kCombinerBlendFuncTableNV[blendF] ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, kCombinerSourceTable[src2] ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, kCombinerOperandTable[oper2] ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_ZERO ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_COLOR ));
			}
		}
		OGL_CALL(glTexEnvf( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (combcolor >> combiner::kScaleShift) ));

		currentCombColor = combcolor;
	}

	// only change combiner alpha setup if different from the cached version.	
	if( currentCombAlpha != combalpha )
	{
		// Handle alpha
		int s0 = (combalpha >> combiner::kSrcZeroShift) & 0xFF;
		int cf = COMBINER_GET_FUNC(combalpha);
		int s1 = (combalpha) & 0xFF;
		if ((cf & 7) == 6) // dot3 combiner doesn't touch alpha, so use just source0
			cf = 0;

		GLint source0 = kCombinerSourceTable[s0 & combiner::kSourceMask];
		GLint oper0 = kCombinerOperandTable[(s0 >> combiner::kOperandShift)|1];
		GLint source1 = kCombinerSourceTable[s1 & combiner::kSourceMask];
		GLint oper1 = kCombinerOperandTable[(s1 >> combiner::kOperandShift)|1];

		if (!(cf & combiner::kBlendFuncMask))
		{
			OGL_CALL(glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, kCombinerFuncTable[cf]));
			OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, source0 ));
			OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, oper0 ));
			OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, source1 ));
			OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, oper1 ));
		}
		else
		{
			int blendF = COMBINER_GET_BLEND_FUNC_INDEX(cf);
			int src2 = cf & combiner::kSourceMask;
			int oper2 = ((cf & combiner::kOperandTwo) >> combiner::kOperandShift) | 1;
			if( blendF == 0 || gGraphicsCaps.gl.hasTextureEnvCombine3ATI )
			{
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, source0 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, oper0 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, source1 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, oper1 ));

				OGL_CALL(glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, kCombinerBlendFuncTableATI[blendF]));
				OGL_CALL(glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, kCombinerSourceTable[src2]));
				OGL_CALL(glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, kCombinerOperandTable[oper2]));
			}
			else
			{
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE4_NV ));

				// TNT combiners use a different argument order than ARB/ATI combiners...
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, source0 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, oper0 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, source1 ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, oper1 ));

				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, kCombinerBlendFuncTableNV[blendF] ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, kCombinerSourceTable[src2] ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, kCombinerOperandTable[oper2] ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO ));
				OGL_CALL(glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA ));
			}
		}
		OGL_CALL(glTexEnvf( GL_TEXTURE_ENV, GL_ALPHA_SCALE, (combalpha >> combiner::kScaleShift)));

		currentCombAlpha = combalpha;
	}
	GLAssert();
}


TextureCombinersGL* TextureCombinersGL::Create( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props )
{
	// check if supported
	if( count > gGraphicsCaps.maxTexUnits )
		return NULL;

	// TODO: move this to client thread for threaded rendering
	for( int i = 0; i < count; ++i )
		if( !IsCombineModeSupported( texEnvs[i].m_CombColor ) )
			return NULL;

	// For threaded rendering this check is done on the client side (and we get NULL here)
	if( props ) {
		for( int i = 0; i < count; ++i ) {
			TextureDimension texDim;
			TexGenMode texGen;
			GetTexEnvInfoFromName( texEnvs[i].m_TextureName, texDim, texGen, props );
			if( !ShaderLab::IsTexEnvSupported( texEnvs[i].m_TextureName, texDim, texGen ) )
				return NULL;
		}
	}

	// create
	TextureCombinersGL* combiners = new TextureCombinersGL();
	combiners->count = count;
	combiners->texEnvs = texEnvs;
	return combiners;
}

