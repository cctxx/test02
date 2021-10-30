#include "UnityPrefix.h"
#include "FixedFunctionStateGLES30.h"
#include <sstream>


FixedFunctionStateGLES30::FixedFunctionStateGLES30 ()
:	texUnitCount(0),
	lightType(0),
	texUnitMatrix(0),
	lightingEnabled(false),
	specularEnabled(true),
	onlyDirectionalLights(false),
	lightCount(0),
	useUniformInsteadOfVertexColor(false),
	useVertexColorAsAmbientAndDiffuse(false),
	useVertexColorAsEmission(false),
	fogMode(kFogDisabled),
	addSpecularAfterTexturing(false),
	alphaTest(kFuncDisabled)
{
	for (int q = 0; q < kMaxSupportedTextureUnitsGLES; ++q)
	{
		texUnitCube[q] = false;
		texUnitGen[q] = kTexGenDisabled;
		texUnitColorCombiner[q] = ~0UL;
		texUnitAlphaCombiner[q] = ~0UL;
	}
}

static std::string CombinerToString (unsigned int combinerDesc)
{
	std::ostringstream s;
	s << combinerDesc;
	return s.str().c_str();
}


std::string FixedFunctionStateGLES30::ToString () const
{
	std::ostringstream s;

	s << "FixedFunctionStateGLES30::ToString():\n";
	s << "  lightingEnabled = " << lightingEnabled << "\n";
	s << "  specularEnabled = " << specularEnabled << "\n";
	s << "  lights = " << lightCount << "\n";
	for (int i = 0; i < lightCount; ++i)
		s << "    light" << i << " : " << GetLightType(i) << "\n";

	s << "  useUniformInsteadOfVertexColor = " << useUniformInsteadOfVertexColor << "\n";
	s << "  useVertexColorAsAmbientAndDiffuse = " << useVertexColorAsAmbientAndDiffuse << "\n";
	s << "  useVertexColorAsEmission = " << useVertexColorAsEmission << "\n";

	s << "  fogMode = " << fogMode << "\n";

	for (int i = 0; i < texUnitCount; ++i)
	{
		s << "  texture " << i << "\n";

		s << "    CUBE = " << ((texUnitCube[i])? "true": "false") << "\n";
		s << "    rgb combiner = " << CombinerToString(texUnitColorCombiner[i]) << "\n";
		s << "    alpba combiner = " << CombinerToString(texUnitAlphaCombiner[i]) << "\n";
		s << "    texGen = " << texUnitGen[i] << "\n";
		s << "    need matrix: " << (NeedTexUnitMatrix(i)?"true":"false") << "\n";
		s << "    need perspective divide: " << (IsTexUnitProjected(i)?"true":"false") << "\n";
	}

	s << "  addSpecularafterTexturing = " << addSpecularAfterTexturing << "\n";
	s << "  alphaTest = " << alphaTest << "\n";

	return s.str().c_str();
}
