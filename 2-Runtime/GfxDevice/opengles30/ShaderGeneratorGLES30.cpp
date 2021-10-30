#include "UnityPrefix.h"
#include "FixedFunctionStateGLES30.h"
#include "GpuProgramsGLES30.h"
#include "ShaderGeneratorGLES30.h"
#include "IncludesGLES30.h"
#include "DebugGLES30.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/BuiltinShaderParams.h"
#include "Runtime/GfxDevice/BuiltinShaderParamsNames.h"
#include "Runtime/Utilities/BitUtility.h"

#include <sstream>
#include <assert.h>


#define CMP_STATE(member) {			\
	if (a.member < b.member)		\
		return true;				\
	else if (b.member < a.member)	\
		return false;				\
}

// builtins support only 4 lights
const int kMaxEmulatedVertexLights = 4;//kMaxSupportedVertexLights;

bool FullStateCompareGLES30::operator() (FixedFunctionStateGLES30 const& a, const FixedFunctionStateGLES30& b) const
{
	CMP_STATE(lightingEnabled)
	CMP_STATE(specularEnabled)
	CMP_STATE(lightCount)
	CMP_STATE(onlyDirectionalLights)
	CMP_STATE(lightType)
	CMP_STATE(texUnitMatrix)
	CMP_STATE(useUniformInsteadOfVertexColor)
	CMP_STATE(useVertexColorAsAmbientAndDiffuse)
	CMP_STATE(useVertexColorAsEmission)

	CMP_STATE(fogMode)

	CMP_STATE(texUnitCount);
	for (int i = 0; i < a.texUnitCount; i++)
	{
		CMP_STATE(texUnitCube[i])
		CMP_STATE(texUnitGen[i])
		CMP_STATE(texUnitColorCombiner[i])
		CMP_STATE(texUnitAlphaCombiner[i])
	}

	CMP_STATE(addSpecularAfterTexturing)
	CMP_STATE(alphaTest)

	return false; /* All equal, not lesser. */
}

bool VertexStateCompareGLES30::operator() (FixedFunctionStateGLES30 const& a, FixedFunctionStateGLES30 const& b) const
{
	CMP_STATE(lightingEnabled)
	CMP_STATE(specularEnabled)
	CMP_STATE(lightCount)
	CMP_STATE(onlyDirectionalLights)
	CMP_STATE(lightType)
	CMP_STATE(texUnitMatrix)
	CMP_STATE(useUniformInsteadOfVertexColor)
	CMP_STATE(useVertexColorAsAmbientAndDiffuse)
	CMP_STATE(useVertexColorAsEmission)

	CMP_STATE(fogMode)

	CMP_STATE(texUnitCount);
	for (int i = 0; i < a.texUnitCount; i++)
	{
		CMP_STATE(texUnitCube[i])
		CMP_STATE(texUnitGen[i])
	}
	return false; /* All equal, not lesser. */
}

bool FragmentStateCompareGLES30::operator() (FixedFunctionStateGLES30 const& a, FixedFunctionStateGLES30 const& b) const
{
	CMP_STATE(fogMode)
	CMP_STATE(texUnitCount)

	for (int i = 0; i < a.texUnitCount; i++)
	{
		CMP_STATE(texUnitCube[i])
		CMP_STATE(texUnitGen[i])
		CMP_STATE(texUnitColorCombiner[i])
		CMP_STATE(texUnitAlphaCombiner[i])
	}

	CMP_STATE(addSpecularAfterTexturing)
	CMP_STATE(alphaTest)

	return false; /* All equal, not lesser. */
}


// --- VERTEX program ----------------------------------------------------------------------------

std::string BuildVertexShaderSourceGLES30 (const FixedFunctionStateGLES30& state)
{
	DBG_SHADER_VERBOSE_GLES30("ShaderGeneratorGLES30::BuildVertexShaderSource()\n");
	DBG_SHADER_VERBOSE_GLES30("  state: %s\n", state.ToString().c_str());

	bool eyePositionRequired = (state.lightingEnabled && state.lightCount > 0 && !state.onlyDirectionalLights);
	for (int i = 0; i < state.texUnitCount; i++)
		if (state.texUnitGen[i]==kTexGenCubeReflect)
			eyePositionRequired = true;

	std::ostringstream src;

	src << "#version 300 es\n";

	/* Standard uniforms. */
	src << "uniform " << FixedFunctionProgramGLES30::kSLPropTransformBlock.GetName() << " {\n";
	src << "    highp mat4 " << GetShaderInstanceMatrixParamName (kShaderInstanceMatMVP) << ";\n";
	if (eyePositionRequired)
	{
		src << "    highp mat4 " << GetShaderInstanceMatrixParamName (kShaderInstanceMatMV) << ";\n";
	}
	src << "};\n";

	/* Default attributes. */
	src << "in highp vec4 _glesVertex;\n";
	src << "in mediump vec3 _glesNormal;\n";
	if (state.useUniformInsteadOfVertexColor)
		src << "uniform lowp vec4 _glesFFColor;\n";
	else
		src << "in lowp vec4 _glesColor;\n";

	/* Default varyings. */
	src << "out lowp vec4 v_color;\n";

	if (state.fogMode > kFogDisabled)
	{
		src << "uniform highp vec4 _glesFogParams;\n";
		src << "uniform lowp  vec4 _glesFogColor;\n";
		src << "out lowp  vec4 _glesFogColorPreMul;\n";
		src << "out lowp  vec4 _glesFogVar;\n";
	}

	/* Texture coordinates and transformation matrices. */
	bool needTexUnitMatrix = false;
	for (int i = 0; i < state.texUnitCount; i++)
	{
		src << "in " << (state.NeedTexUnitMatrix(i)?"highp":"mediump") << " vec4 _glesMultiTexCoord" << i << ";\n";
		if (!state.texUnitCube[i])
		{
			if (state.texUnitGen[i] == kTexGenObject)
				src << "out highp vec4 v_texGenObjCoord" << i << ";\n";
			else
				src << "out mediump vec2 v_texCoord" << i << ";\n";
		}
		else
		{
			src << "out highp vec3 v_texCoord" << i << ";\n";
		}

		if (!needTexUnitMatrix && state.NeedTexUnitMatrix(i))
		{
			needTexUnitMatrix = true;
		}
	}

	if (needTexUnitMatrix)
	{
		src << "uniform " << FixedFunctionProgramGLES30::kSLPropUVTransformBlock.GetName() << " {\n";
		for (int i = 0; i < state.texUnitCount; ++i)
		{
			if(state.NeedTexUnitMatrix(i))
				src << "    highp mat4 " << GetShaderInstanceMatrixParamName (kShaderInstanceMatTexture0 + i) << ";\n";
		}
		src << "};\n";
	}

	/* Handle color -> material mapping. */
	const char* ambientColor	= state.useVertexColorAsAmbientAndDiffuse ? "vertexColor" : "_glesFrontMaterial.ambient";
	const char* diffuseColor	= state.useVertexColorAsAmbientAndDiffuse ? "vertexColor" : "_glesFrontMaterial.diffuse";
	const char* emissionColor	= state.useVertexColorAsEmission ? "vertexColor" : "_glesFrontMaterial.emission";

	/* Light params. */
	if (state.lightingEnabled)
	{
		src << "struct LightModelParameters {\n";
		src << "    vec4 ambient;\n";
		src << "};\n";

		src << "struct MaterialParameters {\n";
		src << "    vec4 emission;\n";
		src << "    vec4 ambient;\n";
		src << "    vec4 diffuse;\n";
		src << "    vec4 specular;\n";
		src << "    float shininess;\n";
		src << "};\n";

		src << "uniform LightModelParameters _glesLightModel;\n";
		src << "uniform MaterialParameters _glesFrontMaterial;\n";

		if (state.lightCount > 0)
		{
			src << "struct LightSourceParameters {\n";
			src << "    vec4 diffuse;\n";
			src << "    vec4 position;\n";
			src << "    vec3 spotDirection;\n";
			src << "    vec4 atten;\n";
			src << "};\n";

			src << "uniform vec4 " << GetBuiltinVectorParamName(kShaderVecLightModelAmbient) << ";\n";
			for (int q = 0; q < kMaxEmulatedVertexLights; ++q)
			{
				src << "uniform vec4 " << GetBuiltinVectorParamName(kShaderVecLight0Diffuse + q) << ";\n";
				src << "uniform vec4 " << GetBuiltinVectorParamName(kShaderVecLight0Position + q) << ";\n";
				src << "uniform vec4 " << GetBuiltinVectorParamName(kShaderVecLight0SpotDirection + q) << ";\n";
				src << "uniform vec4 " << GetBuiltinVectorParamName(kShaderVecLight0Atten + q) << ";\n";
			}

			src << "uniform mat3 _glesNormalMatrix;\n";

			/* Compute functions. */
			src << "\nvec3 direction (vec4 from, vec4 to)\n";
			src << "{\n";
			src << "    return (to.xyz * from.w - from.xyz * to.w);\n";
			src << "}\n";

			src << "\nvec3 computeLighting(LightSourceParameters light, vec3 dirToLight, vec3 eyeNormal, vec4 vertexColor)\n";
			src << "{\n";
			src << "    float NdotL = max(dot(eyeNormal, dirToLight), 0.0);\n";
			// \note in Unity, light ambient is always zero
			src << "    vec3 color = NdotL * " << diffuseColor << ".rgb * light.diffuse.rgb;\n";
			if (state.specularEnabled)
			{
				src << "    if (NdotL > 0.0)\n";
				src << "    {\n";
				src << "        vec3 h = normalize(dirToLight + vec3(0.0, 0.0, 1.0));\n";
				src << "		float HdotN = max(dot(eyeNormal, h), 0.0);\n";
				// \note in Unity, light specular color is always the same as diffuse color
				src << "        color += pow(HdotN, _glesFrontMaterial.shininess) * _glesFrontMaterial.specular.rgb * light.diffuse.rgb;\n";
				src << "    }\n";
			}
			src << "    return color;\n";
			src << "}\n";

			src << "\nvec3 computeDirLight(LightSourceParameters light, vec3 eyeNormal, vec4 vertexColor)\n";
			src << "{\n";
			src << "    vec3 dirToLight = light.position.xyz;\n";
			// \note D3D backend uses min(val, 1.0). We use clamp(), because Img's wrapper is buggy!
			src << "    return clamp(computeLighting(light, dirToLight, eyeNormal, vertexColor), 0.0, 1.0);\n";
			src << "}\n";

			src << "\nvec3 computePointLight(LightSourceParameters light, vec4 eyePosition, vec3 eyeNormal, vec4 vertexColor)\n";
			src << "{\n";
			src << "    vec3 dirToLight = direction(eyePosition, light.position);\n";
			src << "    float distSqr = dot(dirToLight, dirToLight);\n";
			// \note in Unity, const attenuation=1.0, linear=0.0
			src << "    float att = 1.0 / (1.0 + light.atten.z * distSqr);\n";
			src << "    dirToLight *= inversesqrt(distSqr);\n";
			// \note D3D backend uses min(val, 1.0). We use clamp(), because Img's wrapper is buggy!
			src << "    return clamp(att * computeLighting(light, dirToLight, eyeNormal, vertexColor), 0.0, 1.0);\n";
			src << "}\n";

			src << "\nvec3 computeSpotLight(LightSourceParameters light, vec4 eyePosition, vec3 eyeNormal, vec4 vertexColor)\n";
			src << "{\n";
			src << "    vec3 dirToLight = direction(eyePosition, light.position);\n";
			src << "    float distSqr = dot(dirToLight, dirToLight);\n";
			// \note in Unity, const atten=1.0, linear=0.0
			src << "    float att = 1.0 / (1.0 + light.atten.z * distSqr);\n";
			src << "    dirToLight *= inversesqrt(distSqr);\n";
			src << "    float rho = max(dot(dirToLight, light.spotDirection), 0.0);\n";
			src << "    float spotAtt = (rho - light.atten.x) * light.atten.y;\n";
			src << "    spotAtt = clamp(spotAtt, 0.0, 1.0);\n";
			// \note D3D backend uses min(val, 1.0). We use clamp(), because Img's wrapper is buggy!
			src << "    return clamp(att * spotAtt * computeLighting(light, dirToLight, eyeNormal, vertexColor), 0.0, 1.0);\n";
			//src << "    return computeLighting(light, dirToLight, eyeNormal, vertexColor);\n";	// DEBUG DEBUG

			src << "}\n";
		}
	}



	/* Main body. */
	src << "\nvoid main()\n";
	src << "{\n";

	/* Vertex transformation. */
	src << "    gl_Position = " << GetShaderInstanceMatrixParamName (kShaderInstanceMatMVP) << " * _glesVertex;\n";

	/* Unpack vertex color if necessary. */
	if (state.useUniformInsteadOfVertexColor)
		src << "    vec4 vertexColor = _glesFFColor;\n";
	else
		src << "    vec4 vertexColor = _glesColor;\n";

	if (eyePositionRequired)
		src << "    highp vec4 eyePosition  = " << GetShaderInstanceMatrixParamName (kShaderInstanceMatMV) << " * _glesVertex;\n";

	/* Pass and transform texture coordinates. */
	for (int i = 0; i < state.texUnitCount; i++)
	{
		if (!state.texUnitCube[i])
		{
			if (state.texUnitGen[i] == kTexGenObject)
			{
				Assert(state.NeedTexUnitMatrix(i));
				src << "    v_texGenObjCoord" << i << " = ";
				src << "(" << GetShaderInstanceMatrixParamName (kShaderInstanceMatTexture0 + i) << " * _glesMultiTexCoord" << i << ").xyzw;\n";
			}
			else
			{
				if(state.NeedTexUnitMatrix(i))
				{
					src << "    vec4 tmpTexCoord" << i << " = (" << GetShaderInstanceMatrixParamName (kShaderInstanceMatTexture0 + i) << " * _glesMultiTexCoord" << i << ").xyzw;\n";
					if(state.IsTexUnitProjected(i))
						src << "    v_texCoord" << i << " = tmpTexCoord" << i << ".xy / tmpTexCoord" << i << ".w;\n";
					else
						src << "    v_texCoord" << i << " = tmpTexCoord" << i << ".xy;\n";
				}
				else
				{
					src << "    v_texCoord" << i << " = _glesMultiTexCoord" << i << ".xy;\n";
				}
			}
		}
		else
		{
			src << "    v_texCoord" << i << " = ";
			src << "vec3(" << GetShaderInstanceMatrixParamName (kShaderInstanceMatTexture0 + i) << " * _glesMultiTexCoord" << i << ");\n";
			if (state.texUnitGen[i] == kTexGenCubeReflect)
			{
				Assert(eyePositionRequired);
				src << "{\n";
				src << "    vec3 n = v_texCoord"<< i <<".xyz;\n";
				src << "    v_texCoord"<< i <<" = reflect(eyePosition.xyz * eyePosition.w, n);\n";
				src << "}\n";
			}
		}
	}

	switch (state.fogMode)
	{
	case kFogLinear:
		src << "    _glesFogVar = vec4(clamp (_glesFogParams.z * gl_Position.z + _glesFogParams.w, 0.0, 1.0)); _glesFogVar.a = 1.0;\n";
		src << "    _glesFogColorPreMul = _glesFogColor * (vec4(1.0)-_glesFogVar);\n";
		break;
	case kFogExp:
		src << "    float _patchFog = _glesFogParams.y * gl_Position.z;\n";
		src << "    _glesFogVar = vec4(clamp (exp2(-_patchFog), 0.0, 1.0)); _glesFogVar.a = 1.0;\n";
		src << "    _glesFogColorPreMul = _glesFogColor * (vec4(1.0)-_glesFogVar);\n";
		break;
	case kFogExp2:
		src << "    float _patchFog = _glesFogParams.x * gl_Position.z;\n";
		src << "    _patchFog = _patchFog * _patchFog;\n";
		src << "    _glesFogVar = vec4(clamp (exp2(-_patchFog), 0.0, 1.0)); _glesFogVar.a = 1.0;\n";
		src << "    _glesFogColorPreMul = _glesFogColor * (vec4(1.0)-_glesFogVar);\n";
		break;
	default:
		break;
	}

	/* Vertex color computation. */
	if (state.lightingEnabled)
	{
		src << "    vec3 color        = " << emissionColor << ".rgb + " << ambientColor << ".rgb * _glesLightModel.ambient.rgb;\n";
//		src << " color = vec3(0.0);\n";	// DEBUG DEBUG

		if (state.lightCount > 0)
		{
			src << "    vec3 eyeNormal    = normalize(_glesNormalMatrix * _glesNormal);\n";
			for (int i = 0; i < state.lightCount && i < kMaxEmulatedVertexLights; i++)
			{
				src << "    LightSourceParameters light" << i << ";\n";
				src << "    light" << i << ".diffuse = " << GetBuiltinVectorParamName(kShaderVecLight0Diffuse + i) << ";\n";
				src << "    light" << i << ".position = " << GetBuiltinVectorParamName(kShaderVecLight0Position + i) << ";\n";
				src << "    light" << i << ".spotDirection = " << GetBuiltinVectorParamName(kShaderVecLight0SpotDirection + i) << ".xyz;\n";
				src << "    light" << i << ".atten = " << GetBuiltinVectorParamName(kShaderVecLight0Atten + i) << ";\n";

				if(state.GetLightType(i) == kLightDirectional)
					src << "    color += computeDirLight(light" << i << ", eyeNormal, vertexColor);\n";
				else if(state.GetLightType(i) == kLightSpot)
					src << "    color += computeSpotLight(light" << i << ", eyePosition, eyeNormal, vertexColor);\n";
				else
					src << "    color += computePointLight(light" << i << ", eyePosition, eyeNormal, vertexColor);\n";

				Assert(eyePositionRequired || state.GetLightType(i) == kLightDirectional);
			}
		}

		src << "    float alpha = " << diffuseColor << ".a;\n";
		src << "    v_color = vec4(color, alpha);\n";
	}
	else
	{
		src << "    v_color = vertexColor;\n";
	}

	src << "}\n";

	DBG_SHADER_VERBOSE_GLES30("Generated VERTEX program:\n%s\n---\n", src.str().c_str());

	return src.str().c_str();
}

// --- FRAGMENT program ----------------------------------------------------------------------------


static void DecodeTextureCombinerDescriptor (unsigned int					combinerDesc,
											 combiner::Operation&	operation,
											 combiner::Source		sources[3],
											 combiner::Operand		operands[3],
											 int&							scale)
{
	int srcBits0	= (combinerDesc >> combiner::kSrcZeroShift) & 0xFF;
	int srcBits1	= combinerDesc & 0xFF;
	int cf			= COMBINER_GET_FUNC(combinerDesc);

	sources[0]	= static_cast<combiner::Source>(srcBits0 & combiner::kSourceMask);
	operands[0]	= static_cast<combiner::Operand>(srcBits0 >> combiner::kOperandShift);
	sources[1]	= static_cast<combiner::Source>(srcBits1 & combiner::kSourceMask);
	operands[1]	= static_cast<combiner::Operand>(srcBits1 >> combiner::kOperandShift);
	scale		= (combinerDesc >> combiner::kScaleShift);

	if (cf & combiner::kBlendFuncMask)
	{
		int blendF	= COMBINER_GET_BLEND_FUNC_INDEX(cf);
		int src2	= cf & combiner::kSourceMask;
		int oper2	= ((cf & combiner::kOperandTwo) >> combiner::kOperandShift) | 1;

		switch( blendF )
		{
		case 1:
			// src0 * src2 alpha + src1
			sources[2]	= static_cast<combiner::Source>(src2);
			operands[2]	= static_cast<combiner::Operand>(oper2);
			operation	= combiner::kOpMulAdd;
			break;
		case 3:
			// src0 * src2 alpha - src1
			sources[2]	= static_cast<combiner::Source>(src2);
			operands[2]	= static_cast<combiner::Operand>(oper2);
			operation	= combiner::kOpMulSub;
			break;
		// If not supported or lerp combiner, must go below
		case 0:
			// src0 lerp(src2 alpha) src1
			// handeled by default
		case 2:
			// src0 * src2 alpha +- src1
			// not supported!
		default:
			if (blendF != 0) ErrorString("Combiner function not supported by OpenGLES, defaulting to LERP!");
			sources[2]	= static_cast<combiner::Source>(src2);
			operands[2]	= static_cast<combiner::Operand>(oper2);
			operation	= combiner::kOpLerp;
			break;
		}


	}
	else
		operation = static_cast<combiner::Operation>(cf);
}

static void AddTexOperandSrc (
	std::ostringstream&			src,
	int							unitNdx,
	combiner::Channels	channels,
	combiner::Operand	operand,
	combiner::Source		source)
{
	src << "(";

	if (operand == combiner::kOperOneMinusSrcAlpha ||
		operand == combiner::kOperOneMinusSrcColor)
	{
		src << "vec4(1.0) - ";
	}

	switch (source)
	{
		case combiner::kSrcPrimaryColor:
			src << "v_color";
			break;

		case combiner::kSrcPrevious:
			src << "prev";
			break;

		case combiner::kSrcTexture:
			src << "texture";
			break;

		case combiner::kSrcConstant:
			src << "_glesTextureEnvColor" << unitNdx;
			break;

		default:
			printf_console("Error: Unsupported combiner source %d\n", source);
			src << "vec4(1.0)"; /* Dummy value. */
	}

	src << ")";

	switch (operand)
	{
		case combiner::kOperSrcColor:
		case combiner::kOperOneMinusSrcColor:
			if (channels == combiner::kRGBA)
				src << ".rgba";
			else if (channels == combiner::kRGB)
				src << ".rgb";
			else if (channels == combiner::kAlpha)
				src << ".a";
			break;

		case combiner::kOperSrcAlpha:
		case combiner::kOperOneMinusSrcAlpha:
			if (channels == combiner::kRGBA)
				src << ".aaaa";
			else if (channels == combiner::kRGB)
				src << ".aaa";
			else if (channels == combiner::kAlpha)
				src << ".a";
			break;
	}
}

static void AddTextureCombinerBody (std::ostringstream& src, int unitNdx, UInt32 combinerDesc, combiner::Channels channels)
{
	Assert(combiner::kRGBA == 0);
	Assert(combiner::kRGB == 1);
	Assert(combiner::kAlpha == 2);

	const std::string channelTypes[] = { "vec4", "vec3", "float" };
	const std::string channelMask[] = { "", ".rgb", ".a" };


	combiner::Source		sources[3];
	combiner::Operand	operands[3];
	combiner::Operation	op;
	int							scale;

	DecodeTextureCombinerDescriptor(combinerDesc,
									op, sources, operands, scale);

	if ((op == combiner::kOpDot3RGBA || op == combiner::kOpDot3RGB))
	{
		if (channels == combiner::kAlpha)
			return;
		channels = combiner::kRGBA;
	}

	src << "        color" << channelMask[channels] << " = ";

	switch (op)
	{
		case combiner::kOpReplace:
			AddTexOperandSrc(src, unitNdx, channels, operands[0], sources[0]);
			break;

		case combiner::kOpModulate:
		{
			AddTexOperandSrc(src, unitNdx, channels, operands[0], sources[0]);
			src << " * ";
			AddTexOperandSrc(src, unitNdx, channels, operands[1], sources[1]);
			break;
		}

		case combiner::kOpAdd:
		{
			src << "(";
			AddTexOperandSrc(src, unitNdx, channels, operands[0], sources[0]);
			src << " + ";
			AddTexOperandSrc(src, unitNdx, channels, operands[1], sources[1]);
			src << ")";
			break;
		}

		case combiner::kOpSubtract:
		{
			src << "(";
			AddTexOperandSrc(src, unitNdx, channels, operands[0], sources[0]);
			src << " - ";
			AddTexOperandSrc(src, unitNdx, channels, operands[1], sources[1]);
			src << ")";
			break;
		}

		case combiner::kOpAddSigned:
		{
			src << "(";
			AddTexOperandSrc(src, unitNdx, channels, operands[0], sources[0]);
			src << " + ";
			AddTexOperandSrc(src, unitNdx, channels, operands[1], sources[1]);
			src << " - " << channelTypes[channels] << "(0.5)";
			src << ")";
			break;
		}

		case combiner::kOpLerp:
		{
			// NOTE: arguments of Unity LERP combiner are reversed for some reason
			src << "mix(";
			AddTexOperandSrc(src, unitNdx, channels, operands[1], sources[1]);
			src << ", ";
			AddTexOperandSrc(src, unitNdx, channels, operands[0], sources[0]);
			src << ", ";
			AddTexOperandSrc(src, unitNdx, combiner::kAlpha, operands[2], sources[2]);
			src << ")";
			break;
		}

		case combiner::kOpDot3RGB:
		{
			if (channels == combiner::kRGBA)
			{
				src << channelTypes[channels] << "(vec3(4.0 * dot(";
				AddTexOperandSrc(src, unitNdx, combiner::kRGB, operands[0], sources[0]);
				src << " - vec3(0.5), ";
				AddTexOperandSrc(src, unitNdx, combiner::kRGB, operands[1], sources[1]);
				src << " - vec3(0.5))), ";
				AddTexOperandSrc(src, unitNdx, combiner::kAlpha, operands[0], sources[0]);
				src << ")";
				// Note: I am really not sure what goes into alpha channel when dot3_rgb is performed, it's definetly not the from dot
				//       My best guess, that original alpha value is kept

			}
			else
			{
				src << channelTypes[channels] << "(4.0* dot(";
				AddTexOperandSrc(src, unitNdx, combiner::kRGB, operands[0], sources[0]);
				src << " - vec3(0.5), ";
				AddTexOperandSrc(src, unitNdx, combiner::kRGB, operands[1], sources[1]);
				src << " - vec3(0.5)))";
			}
			break;
		}

		case combiner::kOpDot3RGBA:
		{
			src << channelTypes[channels] << "(4.0 * dot(";
			AddTexOperandSrc(src, unitNdx, combiner::kRGB, operands[0], sources[0]);
			src << " - vec3(0.5), ";
			AddTexOperandSrc(src, unitNdx, combiner::kRGB, operands[1], sources[1]);
			src << " - vec3(0.5)))";
			break;
		}
		case combiner::kOpMulAdd:
		{
			src << "(";
			AddTexOperandSrc(src, unitNdx, channels, operands[0], sources[0]);
			src << " * ";
			AddTexOperandSrc(src, unitNdx, channels, operands[2], sources[2]);
			src << " + ";
			AddTexOperandSrc(src, unitNdx, channels, operands[1], sources[1]);
			src << ")";
			break;
		}
		case combiner::kOpMulSub:
		{
			src << "(";
			AddTexOperandSrc(src, unitNdx, channels, operands[0], sources[0]);
			src << " * ";
			AddTexOperandSrc(src, unitNdx, channels, operands[2], sources[2]);
			src << " - ";
			AddTexOperandSrc(src, unitNdx, channels, operands[1], sources[1]);
			src << ")";
			break;
		}
		default:
			ErrorString(Format("Error: Unsupported combiner operation %d\n", op).c_str());

			/* Dummy value. */
			src << channelTypes[channels] << "(1.0)";
			break;
	}

	if (scale != 1)
		src << " * float(" << scale << ".0)";
	src << ";\n";
}

static void AddTextureCombinerSrc (std::ostringstream& src, int unitNdx, bool isCube, UInt32 colorCombiner, UInt32 alphaCombiner)
{
	src << "    {\n        /* Combiner " << unitNdx << " */\n";

	/* Perform lookup. */
	src << "        lowp vec4 texture = " << "texture(u_sampler" << unitNdx << ", v_texCoord" << unitNdx << ");\n";

	src << "        lowp vec4 prev = " << ((unitNdx > 0)? "color": "v_color") << ";\n";

	/* Combine. */
	if (colorCombiner == alphaCombiner)
	{
		// In case of color and alpha combiner being the same
		// we calc all 4 channels in a single operation
		// as some GLSL compilers (iPhone) will silently fail on following:
		//   color.rgb = arg0.rgb (op) arg1.rgb
		//   color.a   = arg0.a   (op) arg1.a
		// instead we spit:
		//   color = arg0 (op) arg1
		// plus it is more readable
		AddTextureCombinerBody(src, unitNdx, colorCombiner, combiner::kRGBA);
	}
	else
	{
		AddTextureCombinerBody(src, unitNdx, colorCombiner, combiner::kRGB);
		AddTextureCombinerBody(src, unitNdx, alphaCombiner, combiner::kAlpha);
	}

	src << "    }\n";
}


std::string BuildFragmentShaderSourceGLES30 (const FixedFunctionStateGLES30& state)
{
	DBG_SHADER_VERBOSE_GLES30("ShaderGeneratorGLES30::BuildFragmentShaderSource()\n");
	DBG_SHADER_VERBOSE_GLES30("  state: %s\n", state.ToString().c_str());

	std::ostringstream src;

	src << "#version 300 es\n";

	bool alphaTestEnabled = state.alphaTest != kFuncDisabled &&
							state.alphaTest != kFuncAlways;

	/* Default varyings. */
	src << "in lowp vec4 v_color;\n";

	/* Uniforms. */
	if (alphaTestEnabled)
		src << "uniform lowp float _glesAlphaTestReference;\n";

	if (state.fogMode > kFogDisabled)
	{
		src << "in lowp vec4 _glesFogColorPreMul;\n";
		src << "in lowp vec4 _glesFogVar;\n";
	}

	/* Texture units. */
	for (int i = 0; i < state.texUnitCount; i++)
	{
		if (!state.texUnitCube[i])
		{
			if (state.texUnitGen[i] == kTexGenObject)
				src << "in highp vec4 v_texGenObjCoord" << i << ";\n";
			else
				src << "in mediump vec2 v_texCoord" << i << ";\n";

			src << "uniform sampler2D u_sampler" << i << ";\n";
		}
		else
		{
			src << "in highp vec3 v_texCoord" << i << ";\n";
			src << "uniform samplerCube u_sampler" << i << ";\n";
		}

		src << "uniform lowp vec4 _glesTextureEnvColor" << i << ";\n";
	}


	/* Main body. */
	src << "out lowp vec4 _glesFragColor;\n";
	src << "\nvoid main()\n";
	src << "{\n";

	/* Initialize color. */
	src << "    lowp vec4 color = v_color;\n";

	/* Generate correct texCoords if we have texGenObject */
	for (int i = 0; i < state.texUnitCount; i++)
	{
		if (!state.texUnitCube[i] && state.texUnitGen[i] == kTexGenObject)
			src << "	highp vec2 v_texCoord" << i << " = v_texGenObjCoord" << i << ".xy / v_texGenObjCoord" << i << ".w;\n";
	}

	/* Texturing. */
	for (int i = 0; i < state.texUnitCount; i++)
		AddTextureCombinerSrc(src, i, state.texUnitCube[i], state.texUnitColorCombiner[i], state.texUnitAlphaCombiner[i]);

	if (state.fogMode > kFogDisabled)
		src << "    _glesFragColor = color * _glesFogVar + _glesFogColorPreMul;\n";
	else
		src << "    _glesFragColor = color;\n";

	/* Alpha test. */
	if (alphaTestEnabled)
	{
		Assert(gGraphicsCaps.gles30.hasAlphaTestQCOM == false);

		if (state.alphaTest == kFuncNever)
		{
			// ToDo: Do we just discard everything, or skip drawing itself at vbo level?
			src << "        discard;\n";
		}
		else
		{
			// Reverse logic because we're using here 'discard'
			static const char* s_cmpOps[] =
			{
				"",		// kFuncDisabled
				"",		// kFuncNever
				">=",	// kFuncLess
				"!=",	// kFuncEqual
				">",	// kFuncLEqual
				"<=",	// kFuncGreater
				"==",	// kFuncNotEqual
				"<",	// kFuncGEqual
				"",		// kFuncAlways
			};

			src << "    if (color.a " << s_cmpOps[state.alphaTest] << "_glesAlphaTestReference)\n";
			src << "        discard;\n";
		}
	}
//	src << "    gl_FragColor = vec4(v_color.xyz, 1.0);\n"; // DEBUG DEBUG
//	src << "    gl_FragColor = 0.5 * texture2D(u_sampler0, v_texCoord0);\n"; // DEBUG DEBUG
//	src << "    gl_FragColor = vec4(_glesTextureEnvColor0.rgb, 1.0);\n"; // DEBUG DEBUG
	src << "}\n";

	DBG_SHADER_VERBOSE_GLES30("Generated FRAGMENT program:\n%s\n---\n", src.str().c_str());

	return src.str().c_str();
}
