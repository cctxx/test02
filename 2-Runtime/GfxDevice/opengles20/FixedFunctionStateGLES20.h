#ifndef FIXEDFUNCTIONSTATE_GLES20_H
#define FIXEDFUNCTIONSTATE_GLES20_H

#include "IncludesGLES20.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include <string>

// we can use one var to determine both shift and mask, but let help out the compiler ;-)
#define FFPSTATE_SET_MASK(target, idx, val, shift_mul, mask)	\
do{																\
	target &= ~(mask << ((idx)*shift_mul));						\
	target |= (val << ((idx)*shift_mul));						\
}while(0)														\



class FixedFunctionStateGLES20
{
public:
	FixedFunctionStateGLES20();

	UInt32			texUnitColorCombiner[kMaxSupportedTextureUnitsGLES];
	UInt32			texUnitAlphaCombiner[kMaxSupportedTextureUnitsGLES];
	bool			texUnitCube[kMaxSupportedTextureUnitsGLES];
	int				texUnitGen[kMaxSupportedTextureUnitsGLES];
	int				texUnitCount;

	// we will use 4bits per light - this way we can handle up to 8 lights (though kMaxEmulatedVertexLights = 4)
	UInt32			lightType;
	// we will use 2 bits per tex unit - one for perspective divide, the other one for if we need matrix mul at all
	// this way we can store 16 texunit info (though kMaxSupportedTextureUnitsGLES = 8)
	UInt32			texUnitMatrix;

	int					lightCount : 8;
	FogMode				fogMode : 8;
	CompareFunction		alphaTest : 8;

	bool			lightingEnabled;
	bool			specularEnabled;
	bool			onlyDirectionalLights;
	bool			setupPointSize;

	bool			useUniformInsteadOfVertexColor;
	bool			useVertexColorAsAmbientAndDiffuse;
	bool			useVertexColorAsEmission;
	bool			addSpecularAfterTexturing;

	unsigned		GetLightType(int i) const			{ return (lightType >> (i*4)) & 0xF; }
	void			SetLightType(int i, unsigned type)	{ FFPSTATE_SET_MASK(lightType, i, type, 4, 0xF); }

	bool			NeedTexUnitMatrix(int i) const		{ return ((texUnitMatrix >> (i*2)) & 0x1) != 0; }
	bool			IsTexUnitProjected(int i) const		{ return ((texUnitMatrix >> (i*2)) & 0x2) != 0; }

	void 			SetTexUnitMatrixParam(int i, bool hasMatrix, bool isProjected)
	{
		int mask = (hasMatrix ? 1 : 0) | (isProjected ? 2 : 0);
		FFPSTATE_SET_MASK(texUnitMatrix, i, mask, 2, 0x3);
	}

	std::string		ToString	() const;
};


#undef FFPSTATE_SET_MASK

#endif /* FIXEDFUNCTIONSTATE_GLES20_H */
