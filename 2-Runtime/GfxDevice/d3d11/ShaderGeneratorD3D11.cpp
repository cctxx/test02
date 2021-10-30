#include "UnityPrefix.h"
#include "ShaderGeneratorD3D11.h"
#include "FixedFunctionStateD3D11.h"
#include "ConstantBuffersD3D11.h"
#include "D3D11Context.h"
#include "Runtime/GfxDevice/GpuProgram.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "D3D11ByteCode.h"

#define DEBUG_D3D11_FF_SHADERS (!UNITY_RELEASE && 0)
#define DEBUG_D3D11_COMPARE_WITH_HLSL (DEBUG_D3D11_FF_SHADERS && 0)

ConstantBuffersD3D11& GetD3D11ConstantBuffers (GfxDevice& device);


// --- Debugging ---------------------------------------------------------------------------------


#if DEBUG_D3D11_FF_SHADERS

#include "Runtime/GfxDevice/d3d11/D3D11Compiler.h"
#if UNITY_WINRT
#include "PlatformDependent/MetroPlayer/MetroUtils.h"
#endif

static D3D11Compiler s_Compiler;

static bool HasD3D11Compiler()
{
	static bool initialized = false;
	if (!initialized)
	{
		//s_Compiler.Initialize (kD3D11CompilerDLL);
		const char* dllName = kD3D11CompilerDLL;
		s_Compiler.compileFunc = NULL;
		s_Compiler.stripShaderFunc = NULL;
		s_Compiler.reflectFunc = NULL;
		s_Compiler.disassembleFunc = NULL;
		s_Compiler.createBlobFunc = NULL;

		#if UNITY_WINRT
		HMODULE dll = LoadPackagedLibrary (ConvertToWindowsPath(dllName)->Data(), 0);
		#else
		HMODULE dll = LoadLibraryA (dllName);
		#endif
		if (dll)
		{
			s_Compiler.compileFunc		= (D3D11Compiler::D3DCompileFunc)		GetProcAddress (dll, "D3DCompile");
			s_Compiler.stripShaderFunc	= (D3D11Compiler::D3DStripShaderFunc)	GetProcAddress (dll, "D3DStripShader");
			s_Compiler.reflectFunc		= (D3D11Compiler::D3DReflectFunc)		GetProcAddress (dll, "D3DReflect");
			s_Compiler.disassembleFunc	= (D3D11Compiler::D3DDisassembleFunc)	GetProcAddress (dll, "D3DDisassemble");
			s_Compiler.createBlobFunc	= (D3D11Compiler::D3DCreateBlobFunc)	GetProcAddress (dll, "D3DCreateBlob");
		}
	}
	return s_Compiler.IsValid();
}

#endif // #if DEBUG_D3D11_FF_SHADERS


#if DEBUG_D3D11_COMPARE_WITH_HLSL

enum D3DCOMPILER_STRIP_FLAGS
{
	D3DCOMPILER_STRIP_REFLECTION_DATA = 1,
	D3DCOMPILER_STRIP_DEBUG_INFO = 2,
	D3DCOMPILER_STRIP_TEST_BLOBS = 4,
	D3DCOMPILER_STRIP_FORCE_DWORD = 0x7fffffff,
};

#define D3D_DISASM_ENABLE_COLOR_CODE 1
#define D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS 2
#define D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING 4
#define D3D_DISASM_ENABLE_INSTRUCTION_CYCLE 8

static void DebugCompileHLSLShaderD3D11 (const std::string& source, bool vertex)
{
	if (!HasD3D11Compiler())
		return;

	ID3D10Blob* shader = NULL;
	ID3D10Blob* errors;
	Assert (s_Compiler.compileFunc);
	HRESULT hr = s_Compiler.compileFunc (
		source.c_str(),
		source.size(),
		"source",
		NULL,
		NULL,
		"main",
		gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0
		? (vertex ? "vs_4_0_level_9_3" : "ps_4_0_level_9_3")
		: (vertex ? "vs_4_0" : "ps_4_0"),
		0,
		0,
		&shader,
		&errors);

	if (FAILED(hr))
	{
		printf_console ("Failed to compile D3D11 shader:\n%s\n", source.c_str());
		if (errors)
		{
			std::string msg (reinterpret_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
			printf_console ("\nErrors:\n%s\n", msg.c_str());
			errors->Release();
		}
		else
		{
			printf_console ("\nErrors unknown!\n");
		}
		AssertString ("Failed to compile fixed function D3D11 shader");
		return;
	}

	if (shader && s_Compiler.stripShaderFunc)
	{
		ID3D10Blob* strippedShader = NULL;

		hr = s_Compiler.stripShaderFunc (shader->GetBufferPointer(), shader->GetBufferSize(), D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO | D3DCOMPILER_STRIP_TEST_BLOBS, &strippedShader);
		if (SUCCEEDED(hr))
		{
			SAFE_RELEASE(shader);
			shader = strippedShader;
		}
	}

	SAFE_RELEASE(errors);

	if (shader && s_Compiler.disassembleFunc)
	{
		ID3D10Blob* disasm = NULL;
		hr = s_Compiler.disassembleFunc (shader->GetBufferPointer(), shader->GetBufferSize(), D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, NULL, &disasm);
		if (SUCCEEDED(hr) && disasm)
		{
			printf_console ("disasm:\n%s\n\n", disasm->GetBufferPointer());
		}
		SAFE_RELEASE(disasm);
	}

	SAFE_RELEASE(shader);
}

static inline void AddToStringList (std::string& str, const char* s)
{
	if (!str.empty())
		str += ',';
	str += s;
}

#endif // #if DEBUG_D3D11_COMPARE_WITH_HLSL



// --- Constant buffers & utilities --------------------------------------------------------------


static const char* kD3D11VertexCB = "UnityFFVertex";
static const char* kD3D11PixelCB = "UnityFFPixel";

enum {
	k11VertexMVP = 0,
	k11VertexMV = 4,
	k11VertexColor = 8,
	k11VertexAmbient = 9,
	k11VertexLightColor = 10,
	k11VertexLightPos = 18,
	k11VertexLightAtten = 26,
	k11VertexLightSpot = 34,
	k11VertexMatDiffuse = 42,
	k11VertexMatAmbient = 43,
	k11VertexMatSpec = 44,
	k11VertexMatEmission = 45,
	k11VertexTex = 46,
	k11VertexFog = 62,
	k11VertexSize = 63,
	k11VertexPosOffset9x = k11VertexSize+1,
};
//k11VertexPosOffset9x will be used like that:
//    mad oPos.xy, v0.w, c63, v0
//    mov oPos.zw, v0

#if DEBUG_D3D11_COMPARE_WITH_HLSL
static const char* kD3D11VertexPrefix =
	"cbuffer UnityFFVertex {\n"
	"	float4x4	ff_matrix_mvp;\n" // 0
	"	float4x4	ff_matrix_mv;\n" // 4
	"	float4		ff_vec_color;\n" // 8
	"	float4		ff_vec_ambient;\n" // 9
	"	float4		ff_light_color[8];\n" // 10
	"	float4		ff_light_pos[8];\n" // 18
	"	float4		ff_light_atten[8];\n" // 26
	"	float4		ff_light_spot[8];\n" // 34
	"	float4		ff_mat_diffuse;\n" // 42
	"	float4		ff_mat_ambient;\n" // 43
	"	float4		ff_mat_spec;\n" // 44
	"	float4		ff_mat_emission;\n" // 45
	"	float4x4	ff_matrix_tex[4];\n" // 46
	"	float4		ff_fog;\n" // 62
	"};\n"; // 62
#endif // #if DEBUG_D3D11_COMPARE_WITH_HLSL


enum {
	k11PixelColors = 0,
	k11PixelAlphaRef = 8,
	k11PixelFog = 9,
	k11PixelSize = 10
};
#if DEBUG_D3D11_COMPARE_WITH_HLSL
static const char* kD3D11PixelPrefix =
"cbuffer UnityFFPixel {\n"
"	float4		ff_vec_colors[8];\n" // 0
"	float		ff_alpha_ref;\n" // 8
"	float4		ff_fog;\n" // 9
"};\n"
"float4 main (\n  ";
#endif // # if DEBUG_D3D11_COMPARE_WITH_HLSL


static void* BuildShaderD3D11 (DXBCBuilder* builder, size_t& outSize)
{
	Assert(builder);

	void* dxbc = dxb_build (builder, outSize);
	Assert(dxbc);
	dxb_destroy (builder);

	#if DEBUG_D3D11_FF_SHADERS
	if (HasD3D11Compiler() && s_Compiler.disassembleFunc)
	{
		ID3D10Blob* disasm = NULL;
		HRESULT hr = s_Compiler.disassembleFunc (dxbc, outSize, D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, NULL, &disasm);
		if (SUCCEEDED(hr) && disasm)
		{
			printf_console ("disasm dxbc:\n%s\n\n", disasm->GetBufferPointer());
		}
		SAFE_RELEASE(disasm);
	}
	#endif

	return dxbc;
}


// --- VERTEX program ----------------------------------------------------------------------------


static void EmitMatrixMul(DXBCBuilderStream& bld, int cbIndex, char srcType, int srcIndex, char dstType, int dstIndex, int tmpIndex, bool wAlways1)
{
	bld.op(kSM4Op_MUL).reg('r',tmpIndex).swz(srcType,srcIndex,kSM4SwzRepY).swz('c',cbIndex+1);
	bld.op(kSM4Op_MAD).reg('r',tmpIndex).swz('c',cbIndex+0).swz(srcType,srcIndex,kSM4SwzRepX).swz('r',tmpIndex);
	bld.op(kSM4Op_MAD).reg('r',tmpIndex).swz('c',cbIndex+2).swz(srcType,srcIndex,kSM4SwzRepZ).swz('r',tmpIndex);
	if (!wAlways1)
		bld.op(kSM4Op_MAD).reg(dstType,dstIndex).swz('c',cbIndex+3).swz(srcType,srcIndex,kSM4SwzRepW).swz('r',tmpIndex);
	else
		bld.op(kSM4Op_ADD).reg(dstType,dstIndex).swz('c',cbIndex+3).swz('r',tmpIndex);
}


void* BuildVertexShaderD3D11 (const FixedFunctionStateD3D11& state, FixedFunctionProgramD3D11::ValueParameters& params, BuiltinShaderParamIndices& matrices, size_t& outSize)
{
	ShaderLab::FastPropertyName cbName; cbName.SetName(kD3D11VertexCB);
	GetD3D11ConstantBuffers(GetRealGfxDevice()).SetCBInfo (cbName.index, k11VertexSize*16);
	params.m_CBID = cbName.index; params.m_CBSize = k11VertexSize*16;

	DXBCBuilder* builder = dxb_create(4, 0, kSM4Shader_Vertex);
	DXBCBuilderStream bld(builder);

	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	std::string helpers, inputs, outputs, code;
	#endif

	bool hasLights = (state.lightingEnabled && state.lightCount > 0);

	bool eyePositionRequired =
		hasLights ||
		(state.fogMode != kFogDisabled);
	bool eyeNormalRequired = hasLights;
	bool viewDirRequired = hasLights && state.specularEnabled;
	bool eyeReflRequired = false;
	{
		UInt64 texSources = state.texUnitSources;
		for (int i = 0; i < state.texUnitCount; i++)
		{
			UInt32 uvSource = texSources & 0xF;
			if (uvSource == kTexSourceEyeLinear)
				eyePositionRequired = true;
			if (uvSource == kTexSourceCubeNormal)
				eyeNormalRequired = true;
			if (uvSource == kTexSourceCubeReflect || uvSource == kTexSourceSphereMap)
				eyeReflRequired = viewDirRequired = eyePositionRequired = eyeNormalRequired = true;
			texSources >>= 4;
		}
	}
	if (eyePositionRequired || eyeNormalRequired || eyeReflRequired)
	{
		matrices.mat[kShaderInstanceMatMV].gpuIndex = k11VertexMV*16;
		matrices.mat[kShaderInstanceMatMV].rows = 4;
		matrices.mat[kShaderInstanceMatMV].cols = 4;
		matrices.mat[kShaderInstanceMatMV].cbID = params.m_CBID;
	}

	dxb_dcl_cb(builder, 0, k11VertexSize);

	int inputRegCounter = 0, outputRegCounter = 0, tempRegCounter = 0;
	int inPosReg = 0, inColorReg = 0, inNormalReg = 0;
	int inUVReg[8] = {0};
	int outColor0Reg = 0, outColor1Reg = 0, outPosReg = 0;
	int outUVReg[8] = {0};
	int eyePosReg = 0, eyeNormalReg = 0, viewDirReg = 0, eyeReflReg = 0, lcolorReg = 0, specColorReg = 0;
	
	dxb_dcl_input(builder, "POSITION", 0, inPosReg = inputRegCounter++);
	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	AddToStringList (inputs, "float4 vertex : POSITION");
	#endif

	// color = Vertex or uniform color
	char inColorType;
	if (state.useUniformInsteadOfVertexColor)
	{
		params.AddVectorParam (k11VertexColor*16, 4, kShaderVecFFColor);
		inColorType = 'c';
		inColorReg = k11VertexColor;
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "float4 color = ff_vec_color;\n";
		#endif
	}
	else
	{
		inColorType = 'v';
		dxb_dcl_input(builder, "COLOR", 0, inColorReg = inputRegCounter++);
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		AddToStringList (inputs, "float4 vertexColor : COLOR");
		code += "float4 color = vertexColor;\n";
		#endif
	}

	// eyePos = eye position
	if (eyePositionRequired)
	{
		eyePosReg = tempRegCounter++;
		EmitMatrixMul (bld, k11VertexMV, 'v',inPosReg, 'r',eyePosReg, eyePosReg, false);
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "float3 eyePos = mul (ff_matrix_mv, vertex).xyz;\n";
		#endif
	}

	// eyeNormal = normalize(normalMatrix * normal)
	if (eyeNormalRequired)
	{
		dxb_dcl_input(builder, "NORMAL", 0, inNormalReg = inputRegCounter++, 0x7);
		eyeNormalReg = tempRegCounter++;
		// mul
		bld.op(kSM4Op_MUL).reg('r',eyeNormalReg,7).swz('v',inNormalReg,kSM4SwzRepY).swz('c',k11VertexMV+1);
		bld.op(kSM4Op_MAD).reg('r',eyeNormalReg,7).swz('c',k11VertexMV+0).swz('v',inNormalReg,kSM4SwzRepX).swz('r',eyeNormalReg);
		bld.op(kSM4Op_MAD).reg('r',eyeNormalReg,7).swz('c',k11VertexMV+2).swz('v',inNormalReg,kSM4SwzRepZ).swz('r',eyeNormalReg);
		// normalize
		bld.op(kSM4Op_DP3).reg('r',eyeNormalReg,8).swz('r',eyeNormalReg,kSM4SwzNone).swz('r',eyeNormalReg,kSM4SwzNone);
		bld.op(kSM4Op_RSQ).reg('r',eyeNormalReg,8).swz('r',eyeNormalReg,kSM4SwzRepW);
		bld.op(kSM4Op_MUL).reg('r',eyeNormalReg,7).swz('r',eyeNormalReg,kSM4SwzRepW).swz('r',eyeNormalReg,kSM4SwzXYZX);

		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		AddToStringList (inputs, "float3 normal : NORMAL");
		code += "float3 eyeNormal = normalize (mul ((float3x3)ff_matrix_mv, normal).xyz);\n"; //@TODO: proper normal matrix
		#endif
	}

	// view dir
	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	code += "float3 viewDir = 0.0;";
	#endif
	if (viewDirRequired)
	{
		// viewDir = normalized vertex-to-eye
		viewDirReg = tempRegCounter++;
		// -normalize
		bld.op(kSM4Op_DP3).reg('r',viewDirReg,8).swz('r',eyePosReg,kSM4SwzNone).swz('r',eyePosReg,kSM4SwzNone);
		bld.op(kSM4Op_RSQ).reg('r',viewDirReg,8).swz('r',viewDirReg,kSM4SwzRepW);
		bld.op(kSM4Op_MUL).reg('r',viewDirReg,7).swz('r',viewDirReg,kSM4SwzRepW).swz('r',eyePosReg,kSM4SwzXYZX,true);

		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "viewDir = -normalize (eyePos);\n";
		#endif
	}

	// eyeRefl
	if (eyeReflRequired)
	{
		DebugAssert (viewDirRequired);
		// eyeRefl = reflection vector, 2*dot(V,N)*N-V
		eyeReflReg = tempRegCounter++;
		bld.op(kSM4Op_DP3).reg('r',eyeReflReg,8).swz('r',viewDirReg,kSM4SwzNone).swz('r',eyeNormalReg,kSM4SwzNone);
		bld.op(kSM4Op_ADD).reg('r',eyeReflReg,8).swz('r',eyeReflReg,kSM4SwzRepW).swz('r',eyeReflReg,kSM4SwzRepW);
		bld.op(kSM4Op_MAD).reg('r',eyeReflReg,7).swz('r',eyeReflReg,kSM4SwzRepW).swz('r',eyeNormalReg,kSM4SwzXYZX).swz('r',viewDirReg,kSM4SwzXYZX,true);

		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "float3 eyeRefl = 2.0f * dot (viewDir, eyeNormal) * eyeNormal - viewDir;\n";
		#endif
	}

	// Lighting
	if (state.lightingEnabled)
	{
		char ambientType, diffuseType, emissionType;
		int ambientReg, diffuseReg, emissionReg;
		if (state.colorMaterial==kColorMatAmbientAndDiffuse)
		{
			ambientType = diffuseType = inColorType;
			ambientReg = diffuseReg = inColorReg;
		}
		else
		{
			ambientType = diffuseType = 'c';
			ambientReg = k11VertexMatAmbient;
			diffuseReg = k11VertexMatDiffuse;
		}
		if (state.colorMaterial==kColorMatEmission)
		{
			emissionType = inColorType;
			emissionReg = inColorReg;
		}
		else
		{
			emissionType = 'c';
			emissionReg = k11VertexMatEmission;
		}

		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		std::string ambientColor	= (state.colorMaterial==kColorMatAmbientAndDiffuse) ? "color" : "ff_mat_ambient";
		std::string diffuseColor	= (state.colorMaterial==kColorMatAmbientAndDiffuse) ? "color" : "ff_mat_diffuse";
		std::string emissionColor = (state.colorMaterial==kColorMatEmission) ? "color" : "ff_mat_emission";
		#endif

		params.AddVectorParam (k11VertexAmbient*16, 4, kShaderVecLightModelAmbient);
		params.AddVectorParam (k11VertexMatAmbient*16, 4, kShaderVecFFMatAmbient);
		params.AddVectorParam (k11VertexMatDiffuse*16, 4, kShaderVecFFMatDiffuse);
		params.AddVectorParam (k11VertexMatSpec*16, 4, kShaderVecFFMatSpecular);
		params.AddVectorParam (k11VertexMatEmission*16, 4, kShaderVecFFMatEmission);

		lcolorReg = tempRegCounter++;
		bld.op(kSM4Op_MAD).reg('r',lcolorReg,7).swz(ambientType,ambientReg,kSM4SwzXYZX).swz('c',k11VertexAmbient,kSM4SwzXYZX).swz(emissionType,emissionReg,kSM4SwzXYZX);

		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "float3 lcolor = " + emissionColor + ".rgb + " + ambientColor + ".rgb * ff_vec_ambient.rgb;\n";
		code += "float3 specColor = 0.0;\n";
		if (state.lightCount > 0)
		{
			helpers += "float3 computeLighting (int idx, float3 dirToLight, float3 eyeNormal, float3 viewDir, float4 diffuseColor, float atten, inout float3 specColor) {\n";
			helpers += "  float NdotL = max(dot(eyeNormal, dirToLight), 0.0);\n";
			helpers += "  float3 color = NdotL * diffuseColor.rgb * ff_light_color[idx].rgb;\n";
			if (state.specularEnabled)
			{
				helpers += "  if (NdotL > 0.0) {\n";
				helpers += "    float3 h = normalize(dirToLight + viewDir);\n";
				helpers += "    float HdotN = max(dot(eyeNormal, h), 0.0);\n";
				helpers += "    float sp = saturate(pow(HdotN, ff_mat_spec.w));\n";
				helpers += "    specColor += atten * sp * ff_light_color[idx].rgb;\n";
				helpers += "  }\n";
			}
			helpers += "  return color * atten;\n";
			helpers += "}\n";

			helpers += "float3 computeSpotLight(int idx, float3 eyePosition, float3 eyeNormal, float3 viewDir, float4 diffuseColor, inout float3 specColor) {\n";
			helpers += "  float3 dirToLight = ff_light_pos[idx].xyz - eyePosition * ff_light_pos[idx].w;\n";
			helpers += "  float distSqr = dot(dirToLight, dirToLight);\n";
			helpers += "  float att = 1.0 / (1.0 + ff_light_atten[idx].z * distSqr);\n";
			helpers += "  if (ff_light_pos[idx].w != 0 && distSqr > ff_light_atten[idx].w) att = 0.0;\n"; // set to 0 if outside of range
			helpers += "  dirToLight *= rsqrt(distSqr);\n";

			helpers += "  float rho = max(dot(dirToLight, ff_light_spot[idx].xyz), 0.0);\n";
			helpers += "  float spotAtt = (rho - ff_light_atten[idx].x) * ff_light_atten[idx].y;\n";
			helpers += "  spotAtt = saturate(spotAtt);\n";
			helpers += "  return min (computeLighting (idx, dirToLight, eyeNormal, viewDir, diffuseColor, att*spotAtt, specColor), 1.0);\n";
			helpers += "}\n";
		}
		#endif // DEBUG_D3D11_COMPARE_WITH_HLSL

		if (state.specularEnabled)
		{
			specColorReg = tempRegCounter++;
			bld.op(kSM4Op_MOV).reg('r',specColorReg,7).float4(0,0,0,0);
		}

		for (int i = 0; i < state.lightCount; ++i)
		{
			params.AddVectorParam ((k11VertexLightPos+i)*16, 4, BuiltinShaderVectorParam(kShaderVecLight0Position+i));
			params.AddVectorParam ((k11VertexLightAtten+i)*16, 4, BuiltinShaderVectorParam(kShaderVecLight0Atten+i));
			params.AddVectorParam ((k11VertexLightColor+i)*16, 4, BuiltinShaderVectorParam(kShaderVecLight0Diffuse+i));
			params.AddVectorParam ((k11VertexLightSpot+i)*16, 4, BuiltinShaderVectorParam(kShaderVecLight0SpotDirection+i));

			Assert(eyePositionRequired);
			Assert(eyeNormalRequired);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "lcolor += computeSpotLight(" + IntToString(i) + ", eyePos, eyeNormal, viewDir, " + diffuseColor + ", specColor);\n";
			#endif

			int ldirReg = tempRegCounter;
			int miscReg = tempRegCounter+1;
			int diffReg = tempRegCounter+2;

			//
			// attenuation

			// float3 dirToLight = ff_light_pos[idx].xyz - eyePosition * ff_light_pos[idx].w;
			// float distSqr = dot(dirToLight, dirToLight);
			// float att = 1.0 / (1.0 + ff_light_atten[idx].z * distSqr);
			// if (ff_light_pos[idx].w != 0 && distSqr > ff_light_atten[idx].w) att = 0.0;
			// dirToLight *= rsqrt(distSqr);
			// float rho = max(dot(dirToLight, ff_light_spot[idx].xyz), 0.0);
			// float spotAtt = (rho - ff_light_atten[idx].x) * ff_light_atten[idx].y;
			// spotAtt = saturate(spotAtt);

			// dirToLight = ff_light_pos[idx].xyz - eyePosition * ff_light_pos[idx].w
			bld.op(kSM4Op_MAD).reg('r',ldirReg,7).swz('r',eyePosReg,kSM4SwzXYZX,true).swz('c',k11VertexLightPos+i,kSM4SwzRepW).swz('c',k11VertexLightPos+i,kSM4SwzXYZX);
			// normalize, distSqr in miscReg.w
			bld.op(kSM4Op_DP3).reg('r',miscReg,8).swz('r',ldirReg,kSM4SwzNone).swz('r',ldirReg,kSM4SwzNone);
			bld.op(kSM4Op_RSQ).reg('r',ldirReg,8).swz('r',miscReg,kSM4SwzRepW);
			bld.op(kSM4Op_MUL).reg('r',ldirReg,7).swz('r',ldirReg,kSM4SwzRepW).swz('r',ldirReg,kSM4SwzXYZX);

			// miscReg.z = float rho = max(dot(dirToLight, ff_light_spot[idx].xyz), 0.0)
			bld.op(kSM4Op_DP3).reg('r',miscReg,4).swz('r',ldirReg,kSM4SwzNone).swz('c',k11VertexLightSpot+i,kSM4SwzNone);
			bld.op(kSM4Op_MAX).reg('r',miscReg,4).swz('r',miscReg,kSM4SwzRepZ).float1(0.0f);
			// miscReg.z = spotAtt = saturate ( (rho - ff_light_atten[idx].x) * ff_light_atten[idx].y )
			bld.op(kSM4Op_ADD).reg('r',miscReg,4).swz('r',miscReg,kSM4SwzRepZ).swz('c',k11VertexLightAtten+i,kSM4SwzRepX,true);
			bld.op_sat(kSM4Op_MUL,miscReg).reg('r',miscReg,4).swz('r',miscReg,kSM4SwzRepZ).swz('c',k11VertexLightAtten+i,kSM4SwzRepY);

			// miscReg.y = float att = 1.0 / (1.0 + ff_light_atten[idx].z * distSqr)
			bld.op(kSM4Op_MAD).reg('r',miscReg,2).swz('c',k11VertexLightAtten+i,kSM4SwzRepZ).swz('r',miscReg,kSM4SwzRepW).float1(1.0f);
			bld.noAutoSM2();
			bld.op(kSM4Op_DIV).reg('r',miscReg,2).float4(1,1,1,1).swz('r',miscReg,kSM4SwzRepY);
			bld.op2(kSM2Op_RCP).reg2('r',miscReg,2).swz2('r',miscReg,kSM4SwzRepY);
			bld.autoSM2();

			// miscReg.y = att * spotAtt
			bld.op(kSM4Op_MUL).reg('r',miscReg,2).swz('r',miscReg,kSM4SwzRepY).swz('r',miscReg,kSM4SwzRepZ);
			// if (ff_light_pos[idx].w != 0 && distSqr > ff_light_atten[idx].w) att = 0.0
			bld.noAutoSM2();
			bld.op(kSM4Op_LT).reg('r',miscReg,1).swz('c',k11VertexLightAtten+i,kSM4SwzRepW).swz('r',miscReg,kSM4SwzRepW);
			bld.op(kSM4Op_NE).reg('r',miscReg,4).swz('c',k11VertexLightPos+i,kSM4SwzRepW).float1(0.0);
			bld.op(kSM4Op_AND).reg('r',miscReg,1).swz('r',miscReg,kSM4SwzRepX).swz('r',miscReg,kSM4SwzRepZ);
			bld.op(kSM4Op_MOVC).reg('r',miscReg,2).swz('r',miscReg,kSM4SwzRepX).float1(0.0).swz('r',miscReg,kSM4SwzRepY);
			//SM2
 			bld.op2(kSM2Op_SLT).reg2('r',miscReg,1).swz2('c',k11VertexLightAtten+i,kSM4SwzRepW).swz2('r',miscReg,kSM4SwzRepW);
			bld.op2(kSM2Op_MUL).reg2('r',miscReg,4).swz2('c',k11VertexLightPos+i,kSM4SwzRepW).swz2('c',k11VertexLightPos+i,kSM4SwzRepW);
			bld.op2(kSM2Op_SLT).reg2('r',miscReg,4).swz2('r',miscReg,kSM4SwzRepZ,true).swz2('r',miscReg,kSM4SwzRepZ);
			bld.op2(kSM2Op_MUL).reg2('r',miscReg,1).swz2('r',miscReg,kSM4SwzRepX).swz2('r',miscReg,kSM4SwzRepZ);
			bld.op2(kSM2Op_MAD).reg2('r',miscReg,2).swz2('r',miscReg,kSM4SwzRepX).swz2('r',miscReg,kSM4SwzRepY,true).swz2('r',miscReg,kSM4SwzRepY);
			bld.autoSM2();

			//
			// diffuse

			// float NdotL = max(dot(eyeNormal, dirToLight), 0.0);
			// float3 color = NdotL * diffuseColor.rgb * ff_light_color[idx].rgb;
			// lcolor += color * atten

			// miscReg.z = float NdotL = max(dot(eyeNormal, dirToLight), 0.0)
			bld.op(kSM4Op_DP3).reg('r',miscReg,4).swz('r',eyeNormalReg,kSM4SwzNone).swz('r',ldirReg,kSM4SwzNone);
			bld.op(kSM4Op_MAX).reg('r',miscReg,4).swz('r',miscReg,kSM4SwzRepZ).float1(0.0f);
			// diffReg.xyz = float3 color = NdotL * diffuseColor.rgb * ff_light_color[idx].rgb
			bld.op(kSM4Op_MUL).reg('r',diffReg,7).swz('r',miscReg,kSM4SwzRepZ).swz(diffuseType,diffuseReg,kSM4SwzXYZX);
			bld.op(kSM4Op_MUL).reg('r',diffReg,7).swz('r',diffReg,kSM4SwzXYZX).swz('c',k11VertexLightColor+i,kSM4SwzXYZX);
			// diffReg.xyz = saturate(color*atten, 1)
			bld.op_sat(kSM4Op_MUL,diffReg).reg('r',diffReg,7).swz('r',diffReg,kSM4SwzXYZX).swz('r',miscReg,kSM4SwzRepY);
			// lcolor += diffReg
			bld.op(kSM4Op_ADD).reg('r',lcolorReg,7).swz('r',lcolorReg,kSM4SwzXYZX).swz('r',diffReg,kSM4SwzXYZX);

			//
			// specular

			if (state.specularEnabled)
			{
				// if (NdotL > 0.0) {
				//   float3 h = normalize(dirToLight + viewDir);
				//   float HdotN = max(dot(eyeNormal, h), 0.0);
				//   float sp = saturate(pow(HdotN, ff_mat_spec.w));
				//   specColor += atten * sp * ff_light_color[idx].rgb;
				// }

				// ldirReg.xyz = h = normalize(dirToLight + viewDir)
				bld.op(kSM4Op_ADD).reg('r',ldirReg,7).swz('r',ldirReg,kSM4SwzXYZX).swz('r',viewDirReg,kSM4SwzXYZX);
				bld.op(kSM4Op_DP3).reg('r',ldirReg,8).swz('r',ldirReg,kSM4SwzNone).swz('r',ldirReg,kSM4SwzNone);
				bld.op(kSM4Op_RSQ).reg('r',ldirReg,8).swz('r',ldirReg,kSM4SwzRepW);
				bld.op(kSM4Op_MUL).reg('r',ldirReg,7).swz('r',ldirReg,kSM4SwzXYZX).swz('r',ldirReg,kSM4SwzRepW);
				// ldirReg.w = HdotN = max(dot(eyeNormal,h),0)
				bld.op(kSM4Op_DP3).reg('r',ldirReg,8).swz('r',ldirReg,kSM4SwzNone).swz('r',eyeNormalReg,kSM4SwzNone);
				bld.op(kSM4Op_MAX).reg('r',ldirReg,8).swz('r',ldirReg,kSM4SwzRepW).float1(0.0f);
				// float sp = saturate(pow(HdotN, ff_mat_spec.w))
				bld.op(kSM4Op_LOG).reg('r',ldirReg,8).swz('r',ldirReg,kSM4SwzRepW);
				bld.op(kSM4Op_MUL).reg('r',ldirReg,8).swz('r',ldirReg,kSM4SwzRepW).swz('c',k11VertexMatSpec,kSM4SwzRepW);
				bld.op(kSM4Op_EXP).reg('r',ldirReg,8).swz('r',ldirReg,kSM4SwzRepW);
				bld.op(kSM4Op_MIN).reg('r',ldirReg,8).swz('r',ldirReg,kSM4SwzRepW).float1(1.0f);
				// atten * sp * ff_light_color[idx].rgb
				bld.op(kSM4Op_MUL).reg('r',ldirReg,8).swz('r',ldirReg,kSM4SwzRepW).swz('r',miscReg,kSM4SwzRepY);
				bld.op(kSM4Op_MUL).reg('r',diffReg,7).swz('r',ldirReg,kSM4SwzRepW).swz('c',k11VertexLightColor+i,kSM4SwzXYZX);
				// nuke specular if NdotL <= 0
				bld.op(kSM4Op_LT).reg('r',miscReg,1).float1(0.0).swz('r',miscReg,kSM4SwzRepZ);
				bld.noAutoSM2();
				bld.op(kSM4Op_AND).reg('r',diffReg,7).swz('r',diffReg,kSM4SwzXYZX).swz('r',miscReg,kSM4SwzRepX);
				bld.op2(kSM2Op_MUL).reg2('r',diffReg,7).swz2('r',diffReg,kSM4SwzXYZX).swz2('r',miscReg,kSM4SwzRepX);
				bld.autoSM2();
				// specColor += computed spec color
				bld.op(kSM4Op_ADD).reg('r',specColorReg,7).swz('r',specColorReg,kSM4SwzXYZX).swz('r',diffReg,kSM4SwzXYZX);
			}
		}

		bld.op(kSM4Op_MOV).reg('r',lcolorReg,8).swz(diffuseType,diffuseReg,kSM4SwzRepW);
		inColorReg = lcolorReg;
		inColorType = 'r';
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "color.rgb = lcolor.rgb;\n";
		code += "color.a = " + diffuseColor + ".a;\n";
		#endif

		if (state.specularEnabled)
		{
			bld.op(kSM4Op_MUL).reg('r',specColorReg,7).swz('r',specColorReg,kSM4SwzXYZX).swz('c',k11VertexMatSpec,kSM4SwzXYZX);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "specColor *= ff_mat_spec.rgb;\n";
			#endif
		}
	}

	// Output final color
	dxb_dcl_output(builder, "COLOR", 0, outColor0Reg = outputRegCounter++);
	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	AddToStringList (outputs, "out float4 ocolor : COLOR0");
	#endif

	bld.op_sat(kSM4Op_MOV,tempRegCounter).reg('o',outColor0Reg).swz(inColorType,inColorReg);
	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	code += "ocolor = saturate(color);\n";
	#endif

	if (state.lightingEnabled && state.specularEnabled)
	{
		dxb_dcl_output(builder, "COLOR", 1, outColor1Reg = outputRegCounter++, 0x7);
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		AddToStringList (outputs, "out float3 ospec : COLOR1");
		#endif

		bld.op_sat(kSM4Op_MOV,tempRegCounter).reg('o',outColor1Reg,7).swz('r',specColorReg,kSM4SwzXYZX);
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "ospec = saturate(specColor);\n";
		#endif
	}

	// we don't need temporary registers from lighting calculations anymore after this point
	if (state.lightingEnabled)
		--tempRegCounter;


	// Pass & transform texture coordinates
	UInt32 gotInputs = 0;
	UInt32 gotOutputs = 0;
	UInt64 texSources = state.texUnitSources;
	for (int i = 0; i < state.texUnitCount; i++)
	{
		matrices.mat[kShaderInstanceMatTexture0+i].gpuIndex = (k11VertexTex+i*4)*16;
		matrices.mat[kShaderInstanceMatTexture0+i].rows = 4;
		matrices.mat[kShaderInstanceMatTexture0+i].cols = 4;
		matrices.mat[kShaderInstanceMatTexture0+i].cbID = params.m_CBID;

		std::string iname = IntToString(i);
		dxb_dcl_output(builder, "TEXCOORD", i, outUVReg[i] = outputRegCounter++);
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		AddToStringList (outputs, ("out float4 ouv" + iname + " : TEXCOORD" + iname).c_str());
		#endif

		UInt32 uvSource = texSources & 0xF;
		if (uvSource >= kTexSourceUV0 && uvSource <= kTexSourceUV7)
		{
			unsigned uv = uvSource-kTexSourceUV0;
			std::string uvStr = IntToString(uv);
			if (!(gotInputs & (1<<uv)))
			{
				dxb_dcl_input(builder, "TEXCOORD", uv, inUVReg[uv] = inputRegCounter++);
				#if DEBUG_D3D11_COMPARE_WITH_HLSL
				AddToStringList (inputs, ("float4 uv"+uvStr+" : TEXCOORD"+uvStr).c_str());
				#endif
				gotInputs |= (1<<uv);
			}
			EmitMatrixMul (bld, k11VertexTex+4*i, 'v',inUVReg[uv], 'o',outUVReg[i], tempRegCounter, false);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "ouv"+iname + " = mul(ff_matrix_tex["+iname + "], uv"+uvStr+");\n";
			#endif
		}
		else if (uvSource == kTexSourceSphereMap)
		{
			// m = 2*sqrt(Rx*Rx + Ry*Ry + (Rz+1)*(Rz+1))
			// SPHR = Rx/m + 0.5, Ry/m + 0.5
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "ouv"+iname + " = mul(ff_matrix_tex["+iname +"], float4(\n";
			code += "  eyeRefl.xy / (2.0*sqrt(eyeRefl.x*eyeRefl.x + eyeRefl.y*eyeRefl.y + (eyeRefl.z+1)*(eyeRefl.z+1))) + 0.5,\n";
			code += "  0,1));\n";
			#endif

			// HLSL generates code like:
			// dp2 r0.w, r0.xyxx, r0.xyxx
			// add r0.z, r0.z, l(1.0)
			// mad r0.z, r0.z, r0.z, r0.w
			// sqrt r0.z, r0.z
			// add r0.z, r0.z, r0.z
			// div r0.xy, r0.xyxx, r0.zzzz
			// add r0.xy, r0.xyxx, l(0.5, 0.5, 0.0, 0.0)
#if 0
			bld.op(kSM4Op_DP2).reg('r',tempRegCounter,8).swz('r',eyeReflReg,kSM4SwzXYXX).swz('r',eyeReflReg,kSM4SwzXYXX);
			bld.op(kSM4Op_ADD).reg('r',tempRegCounter,4).swz('r',eyeReflReg,kSM4SwzRepZ).float1(1.0);
			bld.op(kSM4Op_MAD).reg('r',tempRegCounter,4).swz('r',tempRegCounter,kSM4SwzRepZ).swz('r',tempRegCounter,kSM4SwzRepZ).swz('r',tempRegCounter,kSM4SwzRepW);
			bld.op(kSM4Op_SQRT).reg('r',tempRegCounter,4).swz('r',tempRegCounter,kSM4SwzRepZ);
			bld.op(kSM4Op_ADD).reg('r',tempRegCounter,4).swz('r',tempRegCounter,kSM4SwzRepZ).swz('r',tempRegCounter,kSM4SwzRepZ);
			bld.op(kSM4Op_DIV).reg('r',tempRegCounter,3).swz('r',eyeReflReg,kSM4SwzXYXX).swz('r',tempRegCounter,kSM4SwzRepZ);
			bld.op(kSM4Op_ADD).reg('r',tempRegCounter,3).swz('r',tempRegCounter,kSM4SwzXYXX).float4(0.5f,0.5f,0,0);
#else
			//SM2 compatible
			bld.op(kSM4Op_ADD).reg('r',tempRegCounter,7).swz('r',eyeReflReg,kSM4SwzXYZX).float4(0.0f,0.0f,1.0f,0.0f);
			bld.op(kSM4Op_DP3).reg('r',tempRegCounter,8).swz('r',tempRegCounter,kSM4SwzNone).swz('r',tempRegCounter,kSM4SwzNone);
			bld.op(kSM4Op_RSQ).reg('r',tempRegCounter,8).swz('r',tempRegCounter,kSM4SwzRepW);
			bld.op(kSM4Op_MUL).reg('r',tempRegCounter,8).swz('r',tempRegCounter,kSM4SwzRepW).float4(0.5f,0.5f,0.5f,0.5f);
			bld.op(kSM4Op_MAD).reg('r',tempRegCounter,3).swz('r',eyeReflReg,kSM4SwzXYXX).swz('r',tempRegCounter,kSM4SwzRepW).float4(0.5f,0.5f,0.5f,0.5f);
#endif
			EmitMatrixMul (bld, k11VertexTex+4*i, 'r',tempRegCounter, 'o',outUVReg[i], tempRegCounter+1, true);
		}
		else if (uvSource == kTexSourceObject)
		{
			EmitMatrixMul (bld, k11VertexTex+4*i, 'v',inPosReg, 'o',outUVReg[i], tempRegCounter, false);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "ouv"+iname + " = mul(ff_matrix_tex["+iname +"], vertex);\n";
			#endif
		}
		else if (uvSource == kTexSourceEyeLinear)
		{
			EmitMatrixMul (bld, k11VertexTex+4*i, 'r',eyePosReg, 'o',outUVReg[i], tempRegCounter, true);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "ouv"+iname + " = mul(ff_matrix_tex["+iname +"], float4(eyePos,1.0));\n";
			#endif
		}
		else if (uvSource == kTexSourceCubeNormal)
		{
			EmitMatrixMul (bld, k11VertexTex+4*i, 'r',eyeNormalReg, 'o',outUVReg[i], tempRegCounter, true);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "ouv"+iname + " = mul(ff_matrix_tex["+iname +"], float4(eyeNormal,1.0));\n";
			#endif
		}
		else if (uvSource == kTexSourceCubeReflect)
		{
			EmitMatrixMul (bld, k11VertexTex+4*i, 'r',eyeReflReg, 'o',outUVReg[i], tempRegCounter, true);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "ouv"+iname + " = mul(ff_matrix_tex["+iname +"], float4(eyeRefl,1.0));\n";
			#endif
		}
		else
		{
			AssertString("Unknown texgen mode");
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "ouv"+iname + " = 0.5;\n";
			#endif
		}
		texSources >>= 4;
	}

	// fog if we have a spare varying
	if (state.fogMode != kFogDisabled && outputRegCounter < 8)
	{
		Assert(eyePositionRequired);
		int outFogReg;
		dxb_dcl_output(builder, "FOG", 0, outFogReg = outputRegCounter++, 0x1);
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		AddToStringList (outputs, "out float ofog : FOG0");
		#endif

		params.AddVectorParam (k11VertexFog*16, 4, kShaderVecFFFogParams);

		int fogReg = tempRegCounter++;

		// fogCoord = length(eyePosition.xyz), for radial fog
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "float fogCoord = length(eyePos.xyz);\n";
		#endif

		bld.op(kSM4Op_DP3).reg('r',fogReg,1).swz('r',eyePosReg,kSM4SwzNone).swz('r',eyePosReg,kSM4SwzNone);
#if 0
		bld.op(kSM4Op_SQRT).reg('r',fogReg,1).swz('r',fogReg,kSM4SwzRepX);
#else
		//SM2 compatible
		bld.op(kSM4Op_RSQ).reg('r',fogReg,1).swz('r',fogReg,kSM4SwzRepX);
		bld.op(kSM4Op_RCP).reg('r',fogReg,1).swz('r',fogReg,kSM4SwzRepX);
#endif
		if (state.fogMode == kFogLinear)
		{
			// fogParams.z * fogCoord + fogParams.w
			bld.op_sat(kSM4Op_MAD,tempRegCounter).reg('o',outFogReg,1).swz('r',fogReg,kSM4SwzRepX).swz('c',k11VertexFog,kSM4SwzRepZ).swz('c',k11VertexFog,kSM4SwzRepW);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "ofog = saturate(fogCoord * ff_fog.z + ff_fog.w);\n";
			#endif
		}
		else if (state.fogMode == kFogExp)
		{
			// fogArg = fogParams.y * fogCoord
			// exp2(-fogArg)
			bld.op(kSM4Op_MUL).reg('r',fogReg,1).swz('r',fogReg,kSM4SwzRepX).swz('c',k11VertexFog,kSM4SwzRepY);
			bld.op_sat(kSM4Op_EXP,tempRegCounter).reg('o',outFogReg,1).swz('r',fogReg,kSM4SwzRepX,true);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "ofog = saturate(exp2(-(fogCoord * ff_fog.y)));\n";
			#endif
		}
		else if (state.fogMode == kFogExp2)
		{
			// fogArg = fogParams.y * fogCoord
			// exp2(-fogArg*fogArg)
			bld.op(kSM4Op_MUL).reg('r',fogReg,1).swz('r',fogReg,kSM4SwzRepX).swz('c',k11VertexFog,kSM4SwzRepY);
			bld.op(kSM4Op_MUL).reg('r',fogReg,1).swz('r',fogReg,kSM4SwzRepX).swz('r',fogReg,kSM4SwzRepX);
			bld.op_sat(kSM4Op_EXP,tempRegCounter).reg('o',outFogReg,1).swz('r',fogReg,kSM4SwzRepX,true);
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "fogCoord = fogCoord * ff_fog.y;\n";
			code += "ofog = saturate(exp2(-fogCoord * fogCoord));\n";
			#endif
		}
		--tempRegCounter;
	}

	dxb_dcl_output(builder, "SV_POSITION", 0, outPosReg = outputRegCounter++);
	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	AddToStringList (outputs, "out float4 overtex : SV_POSITION");
	#endif

	// Vertex transformation
	matrices.mat[kShaderInstanceMatMVP].gpuIndex = k11VertexMVP*16;
	matrices.mat[kShaderInstanceMatMVP].rows = 4;
	matrices.mat[kShaderInstanceMatMVP].cols = 4;
	matrices.mat[kShaderInstanceMatMVP].cbID = params.m_CBID;
	bld.op(kSM4Op_MUL).reg('r',0).swz('v',inPosReg,kSM4SwzRepY).swz('c',k11VertexMVP+1);
	bld.op(kSM4Op_MAD).reg('r',0).swz('c',k11VertexMVP+0).swz('v',inPosReg,kSM4SwzRepX).swz('r',0);
	bld.op(kSM4Op_MAD).reg('r',0).swz('c',k11VertexMVP+2).swz('v',inPosReg,kSM4SwzRepZ).swz('r',0);
	bld.op(kSM4Op_MAD).reg('r',0).swz('c',k11VertexMVP+3).swz('v',inPosReg,kSM4SwzRepW).swz('r',0);
	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	code += "overtex = mul (ff_matrix_mvp, vertex);\n";
	#endif

	//correct output pos with Vertex Shader position offset
	//mad oPos.xy, v0.w, c63, v0
	bld.op2(kSM2Op_MAD).reg2('o',outPosReg,3).swz2('r',0,kSM4SwzRepW).swz2('c',k11VertexPosOffset9x).swz2('r',0);
	//mov oPos.zw, v0
	bld.op2(kSM2Op_MOV).reg2('o',outPosReg,12).swz2('r',0);

	//copy output pos for sm40
	bld.noAutoSM2();
	bld.op(kSM4Op_MOV).reg('o',outPosReg).swz('r',0);
	bld.autoSM2();

	bld.op(kSM4Op_RET);


	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	std::string src =
		kD3D11VertexPrefix +
		helpers + '\n' +
		"void main (\n  " +
		inputs + ",\n  " +
		outputs + ") {\n" +
		code + "\n}";
	printf_console ("d3d11 FF VS HLSL:\n%s\n", src.c_str());
	DebugCompileHLSLShaderD3D11 (src, true);
	#endif


	void* blob = BuildShaderD3D11 (builder, outSize);
	return blob;
}


// --- FRAGMENT program ----------------------------------------------------------------------------

enum CombinerWriteMask { kCombWriteRGBA, kCombWriteRGB, kCombWriteA };

static bool EmitCombinerMath11 (
	int stage,
	UInt32 combiner,
	CombinerWriteMask writeMaskMode,
	int texUnitCount,
	DXBCBuilderStream& bld
	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	, std::string& code
	#endif
)
{
	Assert (texUnitCount < 10 && stage < 10);

	combiner::Source	sources[3];
	combiner::Operand	operands[3];
	combiner::Operation	op;
	int					scale;
	combiner::DecodeTextureCombinerDescriptor (combiner, op, sources, operands, scale, true);

	// dot3 and dot3rgba write into RGBA; alpha combiner is always ignored
	if (op == combiner::kOpDot3RGB || op == combiner::kOpDot3RGBA)
	{
		if (writeMaskMode == kCombWriteA)
			return false;
		writeMaskMode = kCombWriteRGBA;
	}

	unsigned tmpIdx = 1;

	bool usedConstant = false;
	char regFile[3];
	unsigned regIdx[3];
	unsigned regSrcAlphaSwz[3];
	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	std::string reg[3];
	#endif
	for (int r = 0; r < 3; ++r)
	{
		combiner::Source source = sources[r];
		regSrcAlphaSwz[r] = kSM4SwzRepW;
		if (stage == 0 && source == combiner::kSrcPrevious)
			source = combiner::kSrcPrimaryColor; // first stage, "previous" the same as "primary"
		switch (source)
		{
		case combiner::kSrcPrimaryColor:
			regFile[r] = 'v'; regIdx[r] = 0;
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			reg[r] = "icolor";
			#endif
			break;
		case combiner::kSrcPrevious:
			regFile[r] = 'r'; regIdx[r] = 0;
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			reg[r] = "col";
			#endif
			break;
		case combiner::kSrcTexture:
			regFile[r] = 'r'; regIdx[r] = 1; tmpIdx = 2; 
			regSrcAlphaSwz[r] = kSM4SwzRepW;
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			reg[r] = "tex";
			#endif
			break;
		case combiner::kSrcConstant:
			usedConstant |= true; regFile[r] = 'c'; regIdx[r] = stage;
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			reg[r] = std::string("ff_vec_colors[") + char('0'+stage) + ']';
			#endif
			break;
		default:
			AssertString("unknown source"); //reg[r] = "foo";
		}
	}

	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	const char* writeMask = "";
	#endif
	unsigned writeMaskBin = 0xF; // rgba
	if (writeMaskMode == kCombWriteRGB)
	{
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		writeMask = ".rgb";
		#endif
		writeMaskBin = 0x7; // rgb
	}
	else if (writeMaskMode == kCombWriteA)
	{
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		writeMask = ".a";
		#endif
		writeMaskBin = 0x8; // a
	}

	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	const char* regSwizzle[3];
	#endif
	unsigned regSwizzleBin[3];
	for (int r = 0; r < 3; ++r)
	{
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		regSwizzle[r] = "";
		#endif
		regSwizzleBin[r] = kSM4SwzNone;
		// 1-x: into tmpN and use that
		if (operands[r] == combiner::kOperOneMinusSrcColor || operands[r] == combiner::kOperOneMinusSrcAlpha)
		{
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += std::string("tmp")+char('0'+r)+" = 1.0 - " + reg[r]+regSwizzle[r] + ";\n";
			reg[r] = std::string("tmp")+char('0'+r);
			#endif
			bld.op(kSM4Op_ADD).reg('r', tmpIdx, writeMaskBin).swz(regFile[r], regIdx[r], regSwizzleBin[r], true).float1(1.0f);
			regFile[r] = 'r';
			regIdx[r] = tmpIdx;
			++tmpIdx;
		}
		// replicate alpha swizzle?
		if (operands[r] == combiner::kOperSrcAlpha || operands[r] == combiner::kOperOneMinusSrcAlpha)
		{
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			regSwizzle[r] = ".a";
			#endif
			regSwizzleBin[r] = kSM4SwzRepW;
		}
	}
	switch (op)
	{
	case combiner::kOpReplace:
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = " + reg[0]+regSwizzle[0] + ";\n";
		#endif
		bld.op(kSM4Op_MOV).reg('r', 0, writeMaskBin).swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		break;				
	case combiner::kOpModulate:
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = " + reg[0]+regSwizzle[0] + " * " + reg[1]+regSwizzle[1] + ";\n";
		#endif
		bld.op(kSM4Op_MUL);
		bld.reg('r', 0, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1]);
		break;
	case combiner::kOpAdd:
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = " + reg[0]+regSwizzle[0] + " + " + reg[1]+regSwizzle[1] + ";\n";
		#endif
		bld.op(kSM4Op_ADD);
		bld.reg('r', 0, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1]);
		break;
	case combiner::kOpAddSigned:
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = " + reg[0]+regSwizzle[0] + " + " + reg[1]+regSwizzle[1] + " - 0.5;\n";
		#endif
		bld.op(kSM4Op_ADD);
		bld.reg('r', 0, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1]);
		bld.op(kSM4Op_ADD);
		bld.reg('r', 0, writeMaskBin);
		bld.swz('r', 0);
		bld.float4(-.5f,-.5f,-.5f,-.5f);
		break;
	case combiner::kOpSubtract:
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = " + reg[0]+regSwizzle[0] + " - " + reg[1]+regSwizzle[1] + ";\n";
		#endif
		bld.op(kSM4Op_ADD);
		bld.reg('r', 0, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1], true);
		break;
	case combiner::kOpLerp:
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = lerp (" + reg[1]+regSwizzle[1] + ", " + reg[0]+regSwizzle[0] + ", " + reg[2]+ ".a);\n";
		#endif
		// tmp = r0-r1
		// res = tmp * r2 + r1
		bld.op(kSM4Op_ADD);
		bld.reg('r', tmpIdx, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1], true);
		bld.op(kSM4Op_MAD);
		bld.reg('r', 0, writeMaskBin);
		bld.swz('r', tmpIdx);
		bld.swz(regFile[2], regIdx[2], regSrcAlphaSwz[2]);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1]);
		++tmpIdx;
		break;
	case combiner::kOpDot3RGB:
		DebugAssert(writeMaskMode == kCombWriteRGBA);
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col.rgb = 4.0 * dot ((") + reg[0]+regSwizzle[0] + ")-0.5, (" + reg[1]+regSwizzle[1] + ")-0.5);\n";
		code += std::string("col.a = ") + reg[0]+".a;\n";
		#endif

		// tmp+0 = r0-0.5
		bld.op(kSM4Op_ADD);
		bld.reg('r', tmpIdx+0, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.float4(-.5f,-.5f,-.5f,-.5f);
		// tmp+1 = r1-0.5
		bld.op(kSM4Op_ADD);
		bld.reg('r', tmpIdx+1, writeMaskBin);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1]);
		bld.float4(-.5f,-.5f,-.5f,-.5f);
		// tmp0.rgb = dp4(tmp+0, tmp+1)
		bld.op(kSM4Op_DP3);
		bld.reg('r', 0, 0x7);
		bld.swz('r', tmpIdx+0);
		bld.swz('r', tmpIdx+1);
		// tmp0.rgb *= 4
		bld.op(kSM4Op_MUL);
		bld.reg('r', 0, 0x7);
		bld.swz('r', 0);
		bld.float4(4.0f,4.0f,4.0f,4.0f);
		// tmp0.a = r0.a
		bld.op(kSM4Op_MOV);
		bld.reg('r', 0, 0x8);
		bld.swz(regFile[0], regIdx[0], kSM4SwzRepW);
		tmpIdx += 2;
		break;
	case combiner::kOpDot3RGBA:
		DebugAssert(writeMaskMode == kCombWriteRGBA);
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = 4.0 * dot ((" + reg[0]+regSwizzle[0] + ")-0.5, (" + reg[1]+regSwizzle[1] + ")-0.5);\n";
		#endif
		// tmp+0 = r0-0.5
		bld.op(kSM4Op_ADD);
		bld.reg('r', tmpIdx+0, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.float4(-.5f,-.5f,-.5f,-.5f);
		// tmp+1 = r1-0.5
		bld.op(kSM4Op_ADD);
		bld.reg('r', tmpIdx+1, writeMaskBin);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1]);
		bld.float4(-.5f,-.5f,-.5f,-.5f);
		// tmp0 = dp4(tmp+0, tmp+1)
		bld.op(kSM4Op_DP3);
		bld.reg('r', 0, writeMaskBin);
		bld.swz('r', tmpIdx+0);
		bld.swz('r', tmpIdx+1);
		// tmp0 *= 4
		bld.op(kSM4Op_MUL);
		bld.reg('r', 0, writeMaskBin);
		bld.swz('r', 0);
		bld.float4(4.0f,4.0f,4.0f,4.0f);
		tmpIdx += 2;
		break;
	case combiner::kOpMulAdd:
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = " + reg[0]+regSwizzle[0] + " * " + reg[2]+".a + " + reg[1]+regSwizzle[1] + ";\n";
		#endif
		bld.op(kSM4Op_MAD);
		bld.reg('r', 0, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.swz(regFile[2], regIdx[2], regSrcAlphaSwz[2]);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1]);
		break;
	case combiner::kOpMulSub:
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = " + reg[0]+regSwizzle[0] + " * " + reg[2]+".a - " + reg[1]+regSwizzle[1] + ";\n";
		#endif
		bld.op(kSM4Op_MAD);
		bld.reg('r', 0, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.swz(regFile[2], regIdx[2], regSrcAlphaSwz[2]);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1], true);
		break;
	case combiner::kOpMulAddSigned:
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = " + reg[0]+regSwizzle[0] + " * " + reg[2]+".a + " + reg[1]+regSwizzle[1] + " - 0.5;\n";
		#endif
		bld.op(kSM4Op_MAD);
		bld.reg('r', 0, writeMaskBin);
		bld.swz(regFile[0], regIdx[0], regSwizzleBin[0]);
		bld.swz(regFile[2], regIdx[2], regSrcAlphaSwz[2]);
		bld.swz(regFile[1], regIdx[1], regSwizzleBin[1]);
		bld.op(kSM4Op_ADD);
		bld.reg('r', 0, writeMaskBin);
		bld.swz('r', 0);
		bld.float4(-.5f,-.5f,-.5f,-.5f);
		break;
	default:
		AssertString ("Unknown combiner op!");
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col")+writeMask + " = " + reg[0]+regSwizzle[0] + ";\n";
		#endif
		break;				
	}

	// scale
	if (scale > 1)
	{
		DebugAssert (scale == 2 || scale == 4);
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += std::string("col *= ") + char('0'+scale) + ".0;\n";
		#endif
		if (scale == 2)
		{
			bld.op(kSM4Op_ADD);
			bld.reg('r', 0, writeMaskBin);
			bld.swz('r', 0);
			bld.swz('r', 0);
		}
		else if (scale == 4)
		{
			bld.op(kSM4Op_MUL);
			bld.reg('r', 0, writeMaskBin);
			bld.swz('r', 0);
			bld.float4(4.0f,4.0f,4.0f,4.0f);
		}
	}

	return usedConstant;
}

void* BuildFragmentShaderD3D11 (const FixedFunctionStateD3D11& state, FixedFunctionProgramD3D11::ValueParameters& params, size_t& outSize)
{
	ShaderLab::FastPropertyName cbName; cbName.SetName(kD3D11PixelCB);
	GetD3D11ConstantBuffers(GetRealGfxDevice()).SetCBInfo (cbName.index, k11PixelSize*16);
	params.m_CBID = cbName.index; params.m_CBSize = k11PixelSize*16;

	DXBCBuilder* builder = dxb_create(4, 0, kSM4Shader_Pixel);
 	DXBCBuilderStream bld(builder);

	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	std::string textures, inputs, code;
	#endif

	dxb_dcl_output(builder, "SV_Target", 0, 0);

	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	AddToStringList (inputs, "float4 icolor : COLOR0");
	#endif
	int inputRegCounter = 0;
	dxb_dcl_input(builder, "COLOR", 0, inputRegCounter++);

	if (state.lightingEnabled && state.specularEnabled)
	{
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		AddToStringList (inputs, "float3 ispec : COLOR1");
		#endif
		dxb_dcl_input(builder, "COLOR", 1, inputRegCounter++, 0x7);
	}

	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	code += "float4 col;\n";
	#endif

	if (state.texUnitCount == 0)
	{
		// No combiners is special case: output primary color
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "col = icolor;\n";
		#endif
		bld.op(kSM4Op_MOV).reg('r', 0).swz('v', 0);

		// BUG, using for ex., 
		// 	SubShader { Pass { Color (1,0,0,0) } }
		// produces white color instead of red on IvyBridge UltraBook
	}
	else
	{
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "float4 tex, tmp0, tmp1, tmp2;\n";
		#endif
		for (int i = 0; i < state.texUnitCount; i++)
		{
			std::string iname = IntToString(i);

			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			AddToStringList (inputs, ("float4 iuv"+iname + " : TEXCOORD"+iname).c_str());
			textures += "SamplerState ff_smp"+iname + " : register(s"+iname+");\n";
			#endif

			// sample the texture into tmp1
			if (state.texUnit3D & (1<<i)) // 3D
			{
				dxb_dcl_input(builder, "TEXCOORD", i, inputRegCounter,0x7);

				#if DEBUG_D3D11_COMPARE_WITH_HLSL
				textures += "Texture3D ff_tex"+iname + " : register(t"+iname+");\n";
				code += "tex = ff_tex"+iname + ".Sample(ff_smp"+iname + ", iuv"+iname + ".xyz);\n";
				#endif

				dxb_dcl_tex(builder, i, kSM4Target_TEXTURE3D);
				bld.op(kSM4Op_SAMPLE).reg('r', 1).swz('v', inputRegCounter, kSM4SwzXYZX).swz('t', i).reg('s', i);
			}
			else if (state.texUnitCube & (1<<i)) // cubemap
			{
				dxb_dcl_input(builder, "TEXCOORD", i, inputRegCounter,0x7);

				#if DEBUG_D3D11_COMPARE_WITH_HLSL
				textures += "TextureCube ff_tex"+iname + " : register(t"+iname+");\n";
				code += "tex = ff_tex"+iname + ".Sample(ff_smp"+iname + ", iuv"+iname + ".xyz);\n";
				#endif

				dxb_dcl_tex(builder, i, kSM4Target_TEXTURECUBE);
				bld.op(kSM4Op_SAMPLE).reg('r', 1).swz('v', inputRegCounter, kSM4SwzXYZX).swz('t', i).reg('s', i);
			}
			else if (state.texUnitProjected & (1<<i)) // projected sample
			{
				dxb_dcl_input(builder, "TEXCOORD", i, inputRegCounter,0xB); // xyw mask

				#if DEBUG_D3D11_COMPARE_WITH_HLSL
				textures += "Texture2D ff_tex"+iname + " : register(t"+iname+");\n";
				code += "tex = ff_tex"+iname + ".Sample(ff_smp"+iname + ", iuv"+iname + ".xy / iuv"+iname + ".w);\n";
				#endif

				dxb_dcl_tex(builder, i, kSM4Target_TEXTURE2D);

				// SM4: use DIV; Intel IvyBridge seems to prefer that
				bld.noAutoSM2();
				bld.op(kSM4Op_DIV).reg('r', 1, 0x3).swz('v', inputRegCounter, kSM4SwzXYXX).swz('v', inputRegCounter, kSM4SwzRepW);
				bld.autoSM2();

				// SM2: use RCP+MUL
				bld.op2(kSM2Op_RCP).reg2('r', 1, 8).swz2('v', inputRegCounter, kSM4SwzRepW);
				bld.op2(kSM2Op_MUL).reg2('r', 1, 0x3).swz2('v', inputRegCounter, kSM4SwzXYXX).swz2('r',1, kSM4SwzRepW);

				bld.op(kSM4Op_SAMPLE).reg('r', 1).swz('r', 1, kSM4SwzXYXX).swz('t', i).reg('s', i);
			}
			else // regular sample
			{
				dxb_dcl_input(builder, "TEXCOORD", i, inputRegCounter,0x3);

				#if DEBUG_D3D11_COMPARE_WITH_HLSL
				textures += "Texture2D ff_tex"+iname + " : register(t"+iname+");\n";
				code += "tex = ff_tex"+iname + ".Sample(ff_smp"+iname + ", iuv"+iname + ".xy);\n";
				#endif

				dxb_dcl_tex(builder, i, kSM4Target_TEXTURE2D);
				bld.op(kSM4Op_SAMPLE).reg('r', 1).swz('v', inputRegCounter, kSM4SwzXYXX).swz('t', i).reg('s', i);
			}

			// emit color & alpha combiners; result in tmp0
			UInt32 colorComb = state.texUnitColorCombiner[i];
			UInt32 alphaComb = state.texUnitAlphaCombiner[i];
			bool usedConstant = false;
			if (colorComb == alphaComb)
			{
				usedConstant |= EmitCombinerMath11 (i, colorComb, kCombWriteRGBA, state.texUnitCount, bld
					#if DEBUG_D3D11_COMPARE_WITH_HLSL
					, code
					#endif
					);
			}
			else
			{
				usedConstant |= EmitCombinerMath11 (i, colorComb, kCombWriteRGB, state.texUnitCount, bld
					#if DEBUG_D3D11_COMPARE_WITH_HLSL
					, code
					#endif
					);
				usedConstant |= EmitCombinerMath11 (i, alphaComb, kCombWriteA, state.texUnitCount, bld
					#if DEBUG_D3D11_COMPARE_WITH_HLSL
					, code
					#endif
					);
			}

			if (usedConstant)
				params.AddVectorParam ((k11PixelColors+i)*16, 4, BuiltinShaderVectorParam(kShaderVecFFTextureEnvColor0+i));
			++inputRegCounter;
		}
	}

	// alpha test
	if (state.alphaTest != kFuncDisabled && state.alphaTest != kFuncAlways)
	{
		params.AddVectorParam (k11PixelAlphaRef*16, 1, kShaderVecFFAlphaTestRef);
		if (state.alphaTest == kFuncNever)
		{
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += "discard;\n";
			#endif
			bld.op(kSM4Op_DISCARD).float1(-1); // int is not sm20 compatible; old comment: HLSL emits 'l(-1)' for plain discard; with the value being integer -1 (all bits set)
		}
		else
		{
			// Reverse logic because we're using here 'discard'
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			static const char* kCmpOps[] =
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
			#endif // #if DEBUG_D3D11_COMPARE_WITH_HLSL
			static SM4Opcode kCmpOpcodes[] =
			{
				kSM4Op_ADD,	// kFuncDisabled
				kSM4Op_ADD,	// kFuncNever
				kSM4Op_GE,	// kFuncLess
				kSM4Op_NE,	// kFuncEqual
				kSM4Op_LT,	// kFuncLEqual
				kSM4Op_GE,	// kFuncGreater
				kSM4Op_EQ,	// kFuncNotEqual
				kSM4Op_LT,	// kFuncGEqual
				kSM4Op_ADD,	// kFuncAlways
			};
			static bool kCmpOrder[] =
			{
				false,	// kFuncDisabled
				false,	// kFuncNever
				true,	// kFuncLess
				true,	// kFuncEqual
				false,	// kFuncLEqual
				false,	// kFuncGreater
				true,	// kFuncNotEqual
				true,	// kFuncGEqual
				false,	// kFuncAlways
			};
			#if DEBUG_D3D11_COMPARE_WITH_HLSL
			code += std::string("if (col.a ") + kCmpOps[state.alphaTest] + " ff_alpha_ref) discard;\n";
			#endif

			bld.noAutoSM2();
			bld.op(kCmpOpcodes[state.alphaTest]).reg('r', 1, 0x1);
			if (kCmpOrder[state.alphaTest])
			{
				bld.swz('r', 0, kSM4SwzRepW);
				bld.swz('c', k11PixelAlphaRef, kSM4SwzRepX);
			}
			else
			{
				bld.swz('c', k11PixelAlphaRef, kSM4SwzRepX);
				bld.swz('r', 0, kSM4SwzRepW);
			}
			bld.op(kSM4Op_DISCARD).reg('r', 1, 1);
			bld.autoSM2();
			
			//SM20
			static float bConst[][2] = 
			{
				{0,0},
				{0,0},

				{0,-1},
				{0,-1},
				{-1,0},
				{0,-1},
				{-1,0},
				{-1,0},

				{0,0},
			};
			static bool bRefSign[] = 
			{
				false,
				false,

				false,
				true,
				true,
				true,
				true,
				false,
				false,
			};

			bld.op2(kSM2Op_ADD).
				reg2('r',1,1).
				swz2('c',k11PixelAlphaRef, kSM4SwzRepX,bRefSign[state.alphaTest]).
				swz2('r', 0, kSM4SwzRepW,!bRefSign[state.alphaTest]);
			if (state.alphaTest == kFuncEqual || state.alphaTest == kFuncNotEqual)
				bld.op2(kSM2Op_MUL).reg2('r',1,1).swz2('r',1,kSM4SwzRepX).swz2('r',1,kSM4SwzRepX);
			bld.op2(kSM2Op_CMP).reg2('r',1).
				swz2('r',1,kSM4SwzRepX,state.alphaTest == kFuncEqual || state.alphaTest == kFuncNotEqual).
				float1_2(bConst[state.alphaTest][0]).
				float1_2(bConst[state.alphaTest][1]);
			bld.op2(kSM2Op_TEXKILL).reg2('r', 1);
		}
	}

	// add specular
	if (state.lightingEnabled && state.specularEnabled)
	{
		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		code += "col.rgb += ispec;\n";
		#endif
		// add r0.xyz, r0.xyz, v1.xyz
		bld.op(kSM4Op_ADD).reg('r', 0, 0x7).swz('r', 0, kSM4SwzXYZX).swz('v', 1, kSM4SwzXYZX);
	}

	// fog
	if (state.fogMode != kFogDisabled && inputRegCounter < 8)
	{
		int fogVar = inputRegCounter;
		dxb_dcl_input(builder, "FOG", 0, fogVar, 0x1);
		params.AddVectorParam (k11PixelFog*16, 4, kShaderVecFFFogColor);
		// color.rgb = lerp (fogColor.rgb, color.rgb, fogVar) =
		//		(color.rgb-fogColor.rgb) * fogVar + fogColor.rgb
		bld.op(kSM4Op_ADD).reg('r',0,7).swz('r',0,kSM4SwzXYZX).swz('c',k11PixelFog,kSM4SwzXYZX, true);
		bld.op(kSM4Op_MAD).reg('r',0,7).swz('r',0,kSM4SwzXYZX).swz('v',fogVar,kSM4SwzRepX).swz('c',k11PixelFog,kSM4SwzXYZX);

		#if DEBUG_D3D11_COMPARE_WITH_HLSL
		AddToStringList (inputs, "float ifog : FOG");
		code += "col.rgb = lerp (ff_fog.rgb, col.rgb, ifog);\n";
		#endif
	}

	if (params.HasVectorParams())
		dxb_dcl_cb(builder, 0, k11PixelSize);

	// mov o0.xyzw, r0.xyzw
	bld.op(kSM4Op_MOV).reg('o', 0).swz('r', 0);
	// ret
	bld.op(kSM4Op_RET);

	#if DEBUG_D3D11_COMPARE_WITH_HLSL
	code += "return col;\n";
	std::string src = textures + kD3D11PixelPrefix + inputs + ") : SV_TARGET {\n" + code + "\n}";
	printf_console ("d3d11 FF PS HLSL:\n%s\n", src.c_str());
	DebugCompileHLSLShaderD3D11 (src, false);
	#endif

	void* blob = BuildShaderD3D11 (builder, outSize);
	return blob;
}
