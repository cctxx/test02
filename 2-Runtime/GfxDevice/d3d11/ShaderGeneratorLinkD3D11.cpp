#include "UnityPrefix.h"

#if UNITY_METRO_VS2013 || (UNITY_WIN && !UNITY_WINRT)

#include "ShaderGeneratorD3D11.h"
#include "FixedFunctionStateD3D11.h"
#include "ConstantBuffersD3D11.h"
#include "Runtime/GfxDevice/GpuProgram.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "PlatformDependent/Win/SmartComPointer.h"
#include "Runtime/Utilities/File.h"

#include "Runtime/GfxDevice/d3d11/InternalShaders/FFShaderLib.h"

#if UNITY_WINRT
#include <D3DCompiler.h>
#endif

ConstantBuffersD3D11& GetD3D11ConstantBuffers (GfxDevice& device);


typedef SmartComPointer<ID3D11LinkingNode> NodePtr;
typedef SmartComPointer<ID3D11ModuleInstance> ModuleInstancePtr;
typedef SmartComPointer<ID3D11Module> ModulePtr;
typedef SmartComPointer<ID3D11Linker> LinkerPtr;
typedef SmartComPointer<ID3D11FunctionLinkingGraph> FLGPtr;
typedef SmartComPointer<ID3DBlob> BlobPtr;

const int kMaxSignatureParams = 12;
const int kSamplingModuleCount = 32;


struct D3D11ShaderLinker
{
	D3D11ShaderLinker()
		: createLinkerFunc(0)
		, loadModuleFunc(0)
		, createFunctionLinkingGraphFunc(0)
		, m_Dll(0)
		, m_Initialized(false)
		, m_Valid(false)
	{
	}

	~D3D11ShaderLinker()
	{
		m_Module.Release();
		m_ModuleInstanceVS.Release();
		m_ModuleInstancePS.Release();

		for (int i = 0; i < kSamplingModuleCount; ++i)
		{
			m_SamplingModules[i].Release();
			m_SamplingModuleInstances[i].Release();
		}

		if (m_Dll)
			FreeLibrary(m_Dll);
	}

	void Initialize (
		const char* dllName, 
		const BYTE* shaderLibraryCode, size_t shaderLibrarySize,
		const BYTE** samplingLibraryCodes, const size_t* samplingLibrarySizes);

	typedef HRESULT (WINAPI *D3DCreateLinkerFunc)(ID3D11Linker **ppLinker);
	typedef HRESULT (WINAPI *D3DLoadModuleFunc)(
		const void* srcData,
		SIZE_T srcDataSize,
		ID3D11Module** ppModule);
	typedef HRESULT (WINAPI *D3DCreateFunctionLinkingGraphFunc)(
		UINT uFlags,
		ID3D11FunctionLinkingGraph **ppFunctionLinkingGraph);

	D3DCreateLinkerFunc	createLinkerFunc;
	D3DLoadModuleFunc	loadModuleFunc;
	D3DCreateFunctionLinkingGraphFunc createFunctionLinkingGraphFunc;

	HMODULE		m_Dll;
	bool		m_Initialized;
	bool		m_Valid;

	ModulePtr	m_Module;
	ModuleInstancePtr m_ModuleInstanceVS, m_ModuleInstancePS;

	ModulePtr	m_SamplingModules[kSamplingModuleCount];
	ModuleInstancePtr m_SamplingModuleInstances[kSamplingModuleCount];
};

static D3D11ShaderLinker s_Linker;

void D3D11ShaderLinker::Initialize (
		const char* dllName, 
		const BYTE* shaderLibraryCode, size_t shaderLibrarySize,
		const BYTE** samplingLibraryCodes, const size_t* samplingLibrarySizes)
{
	if (m_Initialized)
		return;

	m_Valid = false;
	m_Initialized = true;

	// Load DLL
	#if UNITY_WINRT
	// We use a proper linked library on Metro Blue
	//m_Dll = LoadPackagedLibrary (ConvertToWindowsPath(dllName)->Data(), 0);
	#else
	m_Dll = LoadLibraryA (dllName);
	if (!m_Dll)
		return;
	#endif

	// Get functions
#if UNITY_WINRT
	createLinkerFunc = (D3DCreateLinkerFunc)&D3DCreateLinker;
	loadModuleFunc = (D3DLoadModuleFunc)&D3DLoadModule;
	createFunctionLinkingGraphFunc = (D3DCreateFunctionLinkingGraphFunc)&D3DCreateFunctionLinkingGraph;
#else
	createLinkerFunc	= (D3DCreateLinkerFunc)	GetProcAddress (m_Dll, "D3DCreateLinker");
	loadModuleFunc		= (D3DLoadModuleFunc)	GetProcAddress (m_Dll, "D3DLoadModule");
	createFunctionLinkingGraphFunc	= (D3DCreateFunctionLinkingGraphFunc)	GetProcAddress (m_Dll, "D3DCreateFunctionLinkingGraph");
#endif

	if (createLinkerFunc == 0 || loadModuleFunc == 0 || createFunctionLinkingGraphFunc == 0)
		return;

	HRESULT hr = loadModuleFunc(shaderLibraryCode, shaderLibrarySize, &m_Module);
	if (FAILED(hr))
	{
		printf("DX11: Failed to load compiled library: 0x%x\n", hr);
		return;
	}

	// Setup HLSL linker
	hr = m_Module->CreateInstance ("", &m_ModuleInstanceVS);
	if (FAILED(hr))
	{
		printf("DX11: Failed to create compiled library instance: 0x%x\n", hr);
		return;
	}
	hr = m_ModuleInstanceVS->BindConstantBufferByName("UnityFFVertex",0,0);

	hr = m_Module->CreateInstance ("", &m_ModuleInstancePS);
	if (FAILED(hr))
	{
		printf("DX11: Failed to create compiled library instance: 0x%x\n", hr);
		return;
	}
	hr = m_ModuleInstancePS->BindConstantBufferByName("UnityFFPixel",0,0);


	// Setup sampling modules
	for (int i = 0; i < kSamplingModuleCount; ++i)
	{
		hr = loadModuleFunc(samplingLibraryCodes[i], samplingLibrarySizes[i], &m_SamplingModules[i]);
		AssertIf(FAILED(hr));
		hr = m_SamplingModules[i]->CreateInstance ("", &m_SamplingModuleInstances[i]);
		AssertIf(FAILED(hr));

		int unit = i % 8;
		hr = m_SamplingModuleInstances[i]->BindResource(unit, unit, 1);
		hr = m_SamplingModuleInstances[i]->BindSampler(unit, unit, 1);
	}

	m_Valid = true;
}

bool HasD3D11Linker()
{
	const char* dllName = "D3DCompiler_47.dll";
	s_Linker.Initialize(dllName, g_FFShaderLibrary, sizeof(g_FFShaderLibrary), g_FFSampleTexLib, g_FFSampleTexLibSize);
	return s_Linker.m_Valid;
}


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
};
enum {
	k11PixelColors = 0,
	k11PixelAlphaRef = 8,
	k11PixelFog = 9,
	k11PixelSize = 10
};


// --- VERTEX program ----------------------------------------------------------------------------


static D3D11_PARAMETER_DESC CreateParamDesc(const char* name, const char* sem, int dim)
{
	D3D11_PARAMETER_DESC desc = {
		name,
		sem,
		D3D_SVT_FLOAT,
		dim == 1 ? D3D_SVC_SCALAR : D3D_SVC_VECTOR,
		1,
		dim,
		D3D_INTERPOLATION_UNDEFINED,
		D3D_PF_NONE,
		0, 0, 0, 0
	};
	return desc;
}


void* BuildVertexShaderD3D11_Link (const FixedFunctionStateD3D11& state, FixedFunctionProgramD3D11::ValueParameters& params, BuiltinShaderParamIndices& matrices, size_t& outSize)
{
	ShaderLab::FastPropertyName cbName; cbName.SetName(kD3D11VertexCB);
	GetD3D11ConstantBuffers(GetRealGfxDevice()).SetCBInfo (cbName.index, k11VertexSize*16);
	params.m_CBID = cbName.index; params.m_CBSize = k11VertexSize*16;

	HRESULT hr = S_OK;
	const bool hasLinker = HasD3D11Linker();
	Assert(hasLinker);
	FLGPtr flg;
	s_Linker.createFunctionLinkingGraphFunc (0, &flg);
	D3D11_PARAMETER_DESC inputSig[kMaxSignatureParams];
	D3D11_PARAMETER_DESC outputSig[kMaxSignatureParams];

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

	int inputRegCounter = 0, outputRegCounter = 0;
	int inPosReg = 0, inColorReg = 0, inNormalReg = 0;
	int inUVReg[8] = {0};
	int outColor0Reg = 0, outColor1Reg = 0, outPosReg = 0;
	int outUVReg[8] = {0};
	int outFogReg = -1;
	

	// ---- Figure out input signature ----------------------------------------

	// Position
	inputSig[inPosReg = inputRegCounter++] = CreateParamDesc("vertex", "POSITION", 4);
	// Vertex color
	if (!state.useUniformInsteadOfVertexColor)
		inputSig[inColorReg = inputRegCounter++] = CreateParamDesc("vertexColor", "COLOR", 4);
	// Normal
	if (eyeNormalRequired)
		inputSig[inNormalReg = inputRegCounter++] = CreateParamDesc("normal", "NORMAL", 3);
	// UVs
	UInt32 gotInputs = 0;
	static const char* kUVNames[kMaxSupportedTextureCoords] = {"uv0","uv1","uv2","uv3","uv4","uv5","uv6","uv7"};
	static const char* kUVOutNames[kMaxSupportedTextureCoords] = {"ouv0","ouv1","ouv2","ouv3","ouv4","ouv5","ouv6","ouv7"};
	static const char* kUVSemantics[kMaxSupportedTextureCoords] = {"TEXCOORD0","TEXCOORD1","TEXCOORD2","TEXCOORD3","TEXCOORD4","TEXCOORD5","TEXCOORD6","TEXCOORD7"};
	UInt64 texSources = state.texUnitSources;
	for (int i = 0; i < state.texUnitCount; i++)
	{
		UInt32 uvSource = texSources & 0xF;
		if (uvSource >= kTexSourceUV0 && uvSource <= kTexSourceUV7)
		{
			unsigned uv = uvSource-kTexSourceUV0;
			if (!(gotInputs & (1<<uv)))
			{
				inputSig[inUVReg[uv] = inputRegCounter++] = CreateParamDesc(kUVNames[uv], kUVSemantics[uv], 4);
				gotInputs |= (1<<uv);
			}
		}
		texSources >>= 4;
	}

	NodePtr inputNode;
	Assert(inputRegCounter <= kMaxSignatureParams);
	hr = flg->SetInputSignature(inputSig, inputRegCounter, &inputNode);


	// ---- Figure out output signature ---------------------------------------

	// color
	outputSig[outColor0Reg = outputRegCounter++] = CreateParamDesc("ocolor", "COLOR0", 4);
	// spec color
	if (state.lightingEnabled && state.specularEnabled)
		outputSig[outColor1Reg = outputRegCounter++] = CreateParamDesc("ospec", "COLOR1", 3);
	// UVs
	for (int i = 0; i < state.texUnitCount; i++)
		outputSig[outUVReg[i] = outputRegCounter++] = CreateParamDesc(kUVOutNames[i], kUVSemantics[i], 4);
	// Fog
	if (state.fogMode != kFogDisabled && outputRegCounter < 8)
		outputSig[outFogReg = outputRegCounter++] = CreateParamDesc("ofog", "FOG0", 1);
	// position
	outputSig[outPosReg = outputRegCounter++] = CreateParamDesc("overtex", "SV_POSITION", 4);

	// ---- Build code --------------------------------------------------------


	NodePtr colorNode, eyePosNode, eyeNormalNode, viewDirNode, eyeReflNode;
	NodePtr ambientNode, diffuseNode, emissionNode;
	NodePtr ambientNodeToUse, diffuseNodeToUse, emissionNodeToUse;
	NodePtr lightNode, specNode, fogNode;

	// color = Vertex or uniform color
	if (state.useUniformInsteadOfVertexColor)
	{
		params.AddVectorParam (k11VertexColor*16, 4, kShaderVecFFColor);
		hr = flg->CallFunction("", s_Linker.m_Module, "LoadVertexColorUniform", &colorNode);
	}
	else
	{
		hr = flg->CallFunction("", s_Linker.m_Module, "LoadVertexColor", &colorNode);
		hr = flg->PassValue(inputNode, inColorReg, colorNode, 0);
	}

	// eyePos = eye position
	if (eyePositionRequired)
	{
		hr = flg->CallFunction("", s_Linker.m_Module, "LoadEyePos", &eyePosNode);
		hr = flg->PassValue(inputNode, inPosReg, eyePosNode, 0);
	}

	// eyeNormal = normalize(normalMatrix * normal)
	if (eyeNormalRequired)
	{
		hr = flg->CallFunction("", s_Linker.m_Module, "LoadEyeNormal", &eyeNormalNode);
		hr = flg->PassValue(inputNode, inNormalReg, eyeNormalNode, 0);
	}

	// view dir
	if (viewDirRequired)
	{
		Assert(eyePosNode);
		hr = flg->CallFunction("", s_Linker.m_Module, "LoadViewDir", &viewDirNode);
		hr = flg->PassValue(eyePosNode, D3D_RETURN_PARAMETER_INDEX, viewDirNode, 0);
	}
	else
	{
		hr = flg->CallFunction("", s_Linker.m_Module, "LoadZero", &viewDirNode);
	}

	// eyeRefl
	if (eyeReflRequired)
	{
		DebugAssert (viewDirRequired);
		// eyeRefl = reflection vector, 2*dot(V,N)*N-V
		Assert(eyeNormalNode);
		Assert(viewDirNode);
		hr = flg->CallFunction("", s_Linker.m_Module, "LoadEyeRefl", &eyeReflNode);
		hr = flg->PassValue(viewDirNode, D3D_RETURN_PARAMETER_INDEX, eyeReflNode, 0);
		hr = flg->PassValue(eyeNormalNode, D3D_RETURN_PARAMETER_INDEX, eyeReflNode, 1);
	}

	// Lighting
	if (state.lightingEnabled)
	{
		if (state.colorMaterial==kColorMatAmbientAndDiffuse)
		{
			ambientNodeToUse = colorNode;
			diffuseNodeToUse = colorNode;
		}
		else
		{
			hr = flg->CallFunction("", s_Linker.m_Module, "LoadAmbientColor", &ambientNode);
			ambientNodeToUse = ambientNode;
			hr = flg->CallFunction("", s_Linker.m_Module, "LoadDiffuseColor", &diffuseNode);
			diffuseNodeToUse = diffuseNode;
		}
		if (state.colorMaterial==kColorMatEmission)
		{
			emissionNodeToUse = colorNode;
		}
		else
		{
			hr = flg->CallFunction("", s_Linker.m_Module, "LoadEmissionColor", &emissionNode);
			emissionNodeToUse = emissionNode;
		}

		params.AddVectorParam (k11VertexAmbient*16, 4, kShaderVecLightModelAmbient);
		params.AddVectorParam (k11VertexMatAmbient*16, 4, kShaderVecFFMatAmbient);
		params.AddVectorParam (k11VertexMatDiffuse*16, 4, kShaderVecFFMatDiffuse);
		params.AddVectorParam (k11VertexMatSpec*16, 4, kShaderVecFFMatSpecular);
		params.AddVectorParam (k11VertexMatEmission*16, 4, kShaderVecFFMatEmission);

		Assert(emissionNodeToUse);
		Assert(ambientNodeToUse);
		hr = flg->CallFunction("", s_Linker.m_Module, "InitLightColor", &lightNode);
		hr = flg->PassValue(emissionNodeToUse, D3D_RETURN_PARAMETER_INDEX, lightNode, 0);
		hr = flg->PassValue(ambientNodeToUse, D3D_RETURN_PARAMETER_INDEX, lightNode, 1);
		hr = flg->CallFunction("", s_Linker.m_Module, "LoadZero", &specNode);
		if (state.lightCount > 0)
		{
			NodePtr lightCallNode;
			std::string lightFuncName = state.specularEnabled ? "ComputeSpotLightSpec" : "ComputeSpotLight";
			lightFuncName += ('0' + state.lightCount);
			hr = flg->CallFunction("", s_Linker.m_Module, lightFuncName.c_str(), &lightCallNode);
			hr = flg->PassValue(eyePosNode, D3D_RETURN_PARAMETER_INDEX, lightCallNode, 0);
			hr = flg->PassValue(eyeNormalNode, D3D_RETURN_PARAMETER_INDEX, lightCallNode, 1);
			hr = flg->PassValue(viewDirNode, D3D_RETURN_PARAMETER_INDEX, lightCallNode, 2);
			hr = flg->PassValue(diffuseNodeToUse, D3D_RETURN_PARAMETER_INDEX, lightCallNode, 3);
			hr = flg->PassValue(specNode, D3D_RETURN_PARAMETER_INDEX, lightCallNode, 4);
			hr = flg->PassValue(lightNode, D3D_RETURN_PARAMETER_INDEX, lightCallNode, 5);

			NodePtr specMoveNode;
			hr = flg->CallFunction("", s_Linker.m_Module, "Load3", &specMoveNode);
			hr = flg->PassValue(lightCallNode, 4, specMoveNode, 0);
			specNode = specMoveNode;

			lightNode = lightCallNode;
		}

		for (int i = 0; i < state.lightCount; ++i)
		{
			params.AddVectorParam ((k11VertexLightPos+i)*16, 4, BuiltinShaderVectorParam(kShaderVecLight0Position+i));
			params.AddVectorParam ((k11VertexLightAtten+i)*16, 4, BuiltinShaderVectorParam(kShaderVecLight0Atten+i));
			params.AddVectorParam ((k11VertexLightColor+i)*16, 4, BuiltinShaderVectorParam(kShaderVecLight0Diffuse+i));
			params.AddVectorParam ((k11VertexLightSpot+i)*16, 4, BuiltinShaderVectorParam(kShaderVecLight0SpotDirection+i));
		}

		NodePtr finalLightColor;
		hr = flg->CallFunction("", s_Linker.m_Module, "LoadLightingColor", &finalLightColor);
		hr = flg->PassValue(lightNode, D3D_RETURN_PARAMETER_INDEX, finalLightColor, 0);
		hr = flg->PassValue(diffuseNodeToUse, D3D_RETURN_PARAMETER_INDEX, finalLightColor, 1);
		colorNode = finalLightColor;

		if (state.specularEnabled)
		{
			NodePtr finalSpecColor;
			hr = flg->CallFunction("", s_Linker.m_Module, "ModulateSpec", &finalSpecColor);
			hr = flg->PassValue(specNode, D3D_RETURN_PARAMETER_INDEX, finalSpecColor, 0);
			specNode = finalSpecColor;
		}
	}

	// Output final color
	NodePtr saturatedColorNode;
	NodePtr saturatedSpecNode;
	hr = flg->CallFunction("", s_Linker.m_Module, "Saturate4", &saturatedColorNode);
	hr = flg->PassValue(colorNode, D3D_RETURN_PARAMETER_INDEX, saturatedColorNode, 0);

	if (state.lightingEnabled && state.specularEnabled)
	{
		Assert(specNode);
		hr = flg->CallFunction("", s_Linker.m_Module, "Saturate3", &saturatedSpecNode);
		hr = flg->PassValue(specNode, D3D_RETURN_PARAMETER_INDEX, saturatedSpecNode, 0);
	}

	// Pass & transform texture coordinates
	NodePtr texNodes[kMaxSupportedTextureCoords] = {0};

	texSources = state.texUnitSources;
	for (int i = 0; i < state.texUnitCount; i++)
	{
		matrices.mat[kShaderInstanceMatTexture0+i].gpuIndex = (k11VertexTex+i*4)*16;
		matrices.mat[kShaderInstanceMatTexture0+i].rows = 4;
		matrices.mat[kShaderInstanceMatTexture0+i].cols = 4;
		matrices.mat[kShaderInstanceMatTexture0+i].cbID = params.m_CBID;

		std::string iname = IntToString(i);
		std::string texFuncName = "MultiplyUV";
		texFuncName += iname;

		UInt32 uvSource = texSources & 0xF;
		if (uvSource >= kTexSourceUV0 && uvSource <= kTexSourceUV7)
		{
			unsigned uv = uvSource-kTexSourceUV0;
			hr = flg->CallFunction("", s_Linker.m_Module, texFuncName.c_str(), &texNodes[i]);
			hr = flg->PassValue(inputNode, inUVReg[uv], texNodes[i], 0);
		}
		else if (uvSource == kTexSourceSphereMap)
		{
			// m = 2*sqrt(Rx*Rx + Ry*Ry + (Rz+1)*(Rz+1))
			// SPHR = Rx/m + 0.5, Ry/m + 0.5
			NodePtr texGenNode;
			hr = flg->CallFunction("", s_Linker.m_Module, "UVSphereMap", &texGenNode);
			hr = flg->PassValue(eyeReflNode, D3D_RETURN_PARAMETER_INDEX, texGenNode, 0);
			hr = flg->CallFunction("", s_Linker.m_Module, texFuncName.c_str(), &texNodes[i]);
			hr = flg->PassValue(texGenNode, D3D_RETURN_PARAMETER_INDEX, texNodes[i], 0);
		}
		else if (uvSource == kTexSourceObject)
		{
			NodePtr texGenNode;
			hr = flg->CallFunction("", s_Linker.m_Module, texFuncName.c_str(), &texNodes[i]);
			hr = flg->PassValue(inputNode, inPosReg, texNodes[i], 0);
		}
		else if (uvSource == kTexSourceEyeLinear)
		{
			NodePtr texGenNode;
			hr = flg->CallFunction("", s_Linker.m_Module, "Float3to4", &texGenNode);
			hr = flg->PassValue(eyePosNode, D3D_RETURN_PARAMETER_INDEX, texGenNode, 0);
			hr = flg->CallFunction("", s_Linker.m_Module, texFuncName.c_str(), &texNodes[i]);
			hr = flg->PassValue(texGenNode, D3D_RETURN_PARAMETER_INDEX, texNodes[i], 0);
		}
		else if (uvSource == kTexSourceCubeNormal)
		{
			NodePtr texGenNode;
			hr = flg->CallFunction("", s_Linker.m_Module, "Float3to4", &texGenNode);
			hr = flg->PassValue(eyeNormalNode, D3D_RETURN_PARAMETER_INDEX, texGenNode, 0);
			hr = flg->CallFunction("", s_Linker.m_Module, texFuncName.c_str(), &texNodes[i]);
			hr = flg->PassValue(texGenNode, D3D_RETURN_PARAMETER_INDEX, texNodes[i], 0);
		}
		else if (uvSource == kTexSourceCubeReflect)
		{
			NodePtr texGenNode;
			hr = flg->CallFunction("", s_Linker.m_Module, "Float3to4", &texGenNode);
			hr = flg->PassValue(eyeReflNode, D3D_RETURN_PARAMETER_INDEX, texGenNode, 0);
			hr = flg->CallFunction("", s_Linker.m_Module, texFuncName.c_str(), &texNodes[i]);
			hr = flg->PassValue(texGenNode, D3D_RETURN_PARAMETER_INDEX, texNodes[i], 0);
		}
		else
		{
			AssertString("Unknown texgen mode");
		}
		texSources >>= 4;
	}

	// fog if we have a spare varying
	if (state.fogMode != kFogDisabled && outFogReg != -1)
	{
		Assert(eyePositionRequired);

		params.AddVectorParam (k11VertexFog*16, 4, kShaderVecFFFogParams);

		static const char* kFogFunction[] = 
		{
			"",				// kFogDisabled
			"FogLinear",	// kFogLinear
			"FogExp",		// kFogExp
			"FogExp2"		// kFogExp2
		};

		hr = flg->CallFunction("", s_Linker.m_Module, kFogFunction[state.fogMode], &fogNode);
		hr = flg->PassValue(eyePosNode, D3D_RETURN_PARAMETER_INDEX, fogNode, 0);
	}

	// Vertex transformation
	matrices.mat[kShaderInstanceMatMVP].gpuIndex = k11VertexMVP*16;
	matrices.mat[kShaderInstanceMatMVP].rows = 4;
	matrices.mat[kShaderInstanceMatMVP].cols = 4;
	matrices.mat[kShaderInstanceMatMVP].cbID = params.m_CBID;
	NodePtr vertexNode;
	hr = flg->CallFunction("", s_Linker.m_Module, "TransformVertex", &vertexNode);
	hr = flg->PassValue(inputNode, inPosReg, vertexNode, 0);


	NodePtr outputNode;
	Assert(outputRegCounter <= kMaxSignatureParams);
	hr = flg->SetOutputSignature(outputSig, outputRegCounter, &outputNode);

	hr = flg->PassValue(vertexNode, D3D_RETURN_PARAMETER_INDEX, outputNode, outPosReg);
	hr = flg->PassValue(saturatedColorNode, D3D_RETURN_PARAMETER_INDEX, outputNode, outColor0Reg);
	if (saturatedSpecNode)
		hr = flg->PassValue(saturatedSpecNode, D3D_RETURN_PARAMETER_INDEX, outputNode, outColor1Reg);
	for (int i = 0; i < state.texUnitCount; i++)
	{
		if (texNodes[i])
			hr = flg->PassValue(texNodes[i], D3D_RETURN_PARAMETER_INDEX, outputNode, outUVReg[i]);
	}
	if (state.fogMode != kFogDisabled && outFogReg != -1)
	{
		hr = flg->PassValue(fogNode, D3D_RETURN_PARAMETER_INDEX, outputNode, outFogReg);
	}

	#if 0
	// Print generated hlsl for debugging
	BlobPtr hlslOutput;
	hr = flg->GenerateHlsl(0, &hlslOutput);
	if (SUCCEEDED(hr))
	{
		printf_console("DX11 debug linked VS:\n%s\n", hlslOutput->GetBufferPointer());
	}
	#endif


	ModuleInstancePtr flgModule;
	BlobPtr flgErrors;
	hr = flg->CreateModuleInstance(&flgModule, &flgErrors);
	if (FAILED(hr))
	{
		const char* errorMsg = (const char*)flgErrors->GetBufferPointer();
		printf_console("DX11: Failed to create FF VS module: %s\n", errorMsg);
	}

	LinkerPtr linker;
	hr = s_Linker.createLinkerFunc(&linker);
	hr = linker->UseLibrary (s_Linker.m_ModuleInstanceVS);


	#if UNITY_WINRT
	const char* target = "vs_4_0_level_9_1";
	#else
	const char* target = "vs_4_0";
	#endif

	BlobPtr linkedCode;
	BlobPtr linkedErrors;
	hr = linker->Link(flgModule, "main", target, 0, &linkedCode, &linkedErrors);
	if (FAILED(hr))
	{
		const char* errorMsg = (const char*)linkedErrors->GetBufferPointer();
		printf_console("\nDX11: Failed to link FF VS: %s\n", errorMsg);
	}

	if (!linkedCode)
	{
		outSize = 0;
		return NULL;
	}
	outSize = linkedCode->GetBufferSize();
	void* finalCode = malloc(outSize);
	memcpy (finalCode, linkedCode->GetBufferPointer(), outSize);
	return finalCode;
}



// --- FRAGMENT program ----------------------------------------------------------------------------


enum CombinerWriteMask { kCombWriteRGBA, kCombWriteRGB, kCombWriteA };
static const char* k11LinkCombOpNames[combiner::kCombinerOpCount] = {
	"CombReplace", "CombModulate", "CombAdd", "CombAddSigned", "CombSubtract", "CombLerp", "CombDot3", "CombDot3rgba", "CombMulAdd", "CombMulSub", "CombMulAddSigned"
};
static int k11LinkCompOpArgs[combiner::kCombinerOpCount] = {
	1, 2, 2, 2, 2, 3, 2, 2, 3, 3, 3
};

static bool EmitCombinerMath11Link (
	int stage,
	UInt32 combiner,
	CombinerWriteMask writeMaskMode,
	int texUnitCount,
	FLGPtr& flg,
	NodePtr& inputNode, NodePtr& prevNode, NodePtr& texNode,
	NodePtr& outNewNode)
{
	Assert (texUnitCount < 10 && stage < 10);

	HRESULT hr = S_OK;

	combiner::Source	sources[3];
	combiner::Operand	operands[3];
	combiner::Operation	op;
	int					scale;
	combiner::DecodeTextureCombinerDescriptor (combiner, op, sources, operands, scale, true);

	// dot3 and dot3rgba write into RGBA; alpha combiner is always ignored
	if (op == combiner::kOpDot3RGB || op == combiner::kOpDot3RGBA)
	{
		if (writeMaskMode == kCombWriteA)
		{
			outNewNode.Release();
			return false;
		}
		writeMaskMode = kCombWriteRGBA;
	}

	bool usedConstant = false;
	NodePtr reg[3];
	int regIndex[3] = {D3D_RETURN_PARAMETER_INDEX, D3D_RETURN_PARAMETER_INDEX, D3D_RETURN_PARAMETER_INDEX};
	for (int r = 0; r < 3; ++r)
	{
		combiner::Source source = sources[r];
		if (stage == 0 && source == combiner::kSrcPrevious)
			source = combiner::kSrcPrimaryColor; // first stage, "previous" the same as "primary"
		switch (source)
		{
		case combiner::kSrcPrimaryColor:
			reg[r] = inputNode;
			regIndex[r] = 0;
			break;
		case combiner::kSrcPrevious:
			reg[r] = prevNode;
			break;
		case combiner::kSrcTexture:
			reg[r] = texNode;
			break;
		case combiner::kSrcConstant:
			usedConstant |= true;
			{
				std::string funcName = "LoadConstantColor";
				funcName += ('0'+stage);
				hr = flg->CallFunction("", s_Linker.m_Module, funcName.c_str(), &reg[r]);
			}
			break;
		default:
			AssertString("unknown source"); //reg[r] = "foo";
		}
	}

	const char* regSwizzle[3];
	for (int r = 0; r < 3; ++r)
	{
		regSwizzle[r] = "rgba";
		// 1-x: into tmpN and use that
		if (operands[r] == combiner::kOperOneMinusSrcColor || operands[r] == combiner::kOperOneMinusSrcAlpha)
		{
			NodePtr tmpNode;
			hr = flg->CallFunction("", s_Linker.m_Module, "OneMinus4", &tmpNode);
			hr = flg->PassValue(reg[r], regIndex[r], tmpNode, 0);
			reg[r] = tmpNode;
			regIndex[r] = D3D_RETURN_PARAMETER_INDEX;
		}
		// replicate alpha swizzle?
		if (operands[r] == combiner::kOperSrcAlpha || operands[r] == combiner::kOperOneMinusSrcAlpha)
		{
			regSwizzle[r] = "aaaa";
		}
	}

	// combiner op
	NodePtr opNode;	
	hr = flg->CallFunction("", s_Linker.m_Module, k11LinkCombOpNames[op], &opNode);
	for (int i = 0; i < k11LinkCompOpArgs[op]; ++i)
		hr = flg->PassValueWithSwizzle(reg[i], regIndex[i], regSwizzle[i], opNode, i, "rgba");

	// scale
	if (scale > 1)
	{
		DebugAssert (scale == 2 || scale == 4);
		NodePtr scaleNode;
		hr = flg->CallFunction("", s_Linker.m_Module, scale == 2 ? "Scale2" : "Scale4", &scaleNode);
		hr = flg->PassValue(opNode, D3D_RETURN_PARAMETER_INDEX, scaleNode, 0);
		opNode = scaleNode;
	}

	outNewNode = opNode;
	return usedConstant;
}


void* BuildFragmentShaderD3D11_Link (const FixedFunctionStateD3D11& state, FixedFunctionProgramD3D11::ValueParameters& params, size_t& outSize)
{
	ShaderLab::FastPropertyName cbName; cbName.SetName(kD3D11PixelCB);
	GetD3D11ConstantBuffers(GetRealGfxDevice()).SetCBInfo (cbName.index, k11PixelSize*16);
	params.m_CBID = cbName.index; params.m_CBSize = k11PixelSize*16;

	HRESULT hr = S_OK;
	const bool hasLinker = HasD3D11Linker();
	Assert(hasLinker);

	FLGPtr flg;
	s_Linker.createFunctionLinkingGraphFunc (0, &flg);

	LinkerPtr linker;
	hr = s_Linker.createLinkerFunc(&linker);

	// ---- Figure out input signature ----------------------------------------

	D3D11_PARAMETER_DESC inputSig[kMaxSignatureParams];
	int inputRegCounter = 0;
	int inColorReg = 0, inSpecReg = 0, inFogReg = 0;
	int inUVReg[kMaxSupportedTextureCoords] = {0};
	static const char* kUVNames[kMaxSupportedTextureCoords] = {"uv0","uv1","uv2","uv3","uv4","uv5","uv6","uv7"};
	static const char* kUVSemantics[kMaxSupportedTextureCoords] = {"TEXCOORD0","TEXCOORD1","TEXCOORD2","TEXCOORD3","TEXCOORD4","TEXCOORD5","TEXCOORD6","TEXCOORD7"};

	inputSig[inColorReg = inputRegCounter++] = CreateParamDesc("icolor", "COLOR0", 4);
	if (state.lightingEnabled && state.specularEnabled)
		inputSig[inSpecReg = inputRegCounter++] = CreateParamDesc("ispec", "COLOR1", 3);
	for (int i = 0; i < state.texUnitCount; i++)
		inputSig[inUVReg[i] = inputRegCounter++] = CreateParamDesc(kUVNames[i], kUVSemantics[i], 4);
	if (state.fogMode != kFogDisabled && inputRegCounter < 8)
		inputSig[inFogReg = inputRegCounter++] = CreateParamDesc("ifog", "FOG0", 1);

	NodePtr inputNode;
	Assert(inputRegCounter <= kMaxSignatureParams);
	hr = flg->SetInputSignature(inputSig, inputRegCounter, &inputNode);


	// ---- Figure out output signature ---------------------------------------

	D3D11_PARAMETER_DESC outputSig[kMaxSignatureParams];
	int outputRegCounter = 0;
	int outColorReg = 0;

	outputSig[outColorReg = outputRegCounter++] = CreateParamDesc("ocolor", "SV_Target", 4);


	// ---- Build code --------------------------------------------------------

	NodePtr colorNode;

	if (state.texUnitCount == 0)
	{
		// No combiners is special case: output primary color
		flg->CallFunction("", s_Linker.m_Module, "LoadVertexColor", &colorNode);
		flg->PassValue(inputNode, inColorReg, colorNode, 0);
	}
	else
	{
		for (int i = 0; i < state.texUnitCount; i++)
		{
			std::string funcName = "LoadTex";
			funcName += ('0'+i);

			// type: 0 - 2d, 1 - 2d proj, 2 - 3d, 3 - cube
			int type = 0;
			if (state.texUnit3D & (1<<i))
				type = 2;
			else if (state.texUnitCube & (1<<i))
				type = 3;
			else if (state.texUnitProjected & (1<<i))
				type = 1;

			// Sampling modules are layed out by InternalShader/CompileShaderLib/CompileShaderLib.cpp
			int samplingModule = 8 * type + i;

			hr = linker->UseLibrary(s_Linker.m_SamplingModuleInstances[samplingModule]);
			AssertIf(FAILED(hr));

			NodePtr texNode;
			hr = flg->CallFunction("", s_Linker.m_SamplingModules[samplingModule], funcName.c_str(), &texNode);
			AssertIf(FAILED(hr));
			hr = flg->PassValue(inputNode, inUVReg[i], texNode, 0);
			AssertIf(FAILED(hr));

			// emit color & alpha combiners
			NodePtr newColorNode;
			UInt32 colorComb = state.texUnitColorCombiner[i];
			UInt32 alphaComb = state.texUnitAlphaCombiner[i];
			bool usedConstant = false;
			if (colorComb == alphaComb)
			{
				usedConstant |= EmitCombinerMath11Link (i, colorComb, kCombWriteRGBA, state.texUnitCount, flg, inputNode, colorNode, texNode, newColorNode);
			}
			else
			{
				usedConstant |= EmitCombinerMath11Link (i, colorComb, kCombWriteRGB, state.texUnitCount, flg, inputNode, colorNode, texNode, newColorNode);
				NodePtr newAlphaNode;
				usedConstant |= EmitCombinerMath11Link (i, alphaComb, kCombWriteA, state.texUnitCount, flg, inputNode, colorNode, texNode, newAlphaNode);
				if (newAlphaNode)
				{
					NodePtr combinedNode;
					flg->CallFunction("", s_Linker.m_Module, "CombineAlpha", &combinedNode);
					flg->PassValue(newColorNode, D3D_RETURN_PARAMETER_INDEX, combinedNode, 0);
					flg->PassValue(newAlphaNode, D3D_RETURN_PARAMETER_INDEX, combinedNode, 1);
					newColorNode = combinedNode;
				}
			}

			if (usedConstant)
				params.AddVectorParam ((k11PixelColors+i)*16, 4, BuiltinShaderVectorParam(kShaderVecFFTextureEnvColor0+i));

			colorNode = newColorNode;
		}
	}

	if (state.alphaTest != kFuncDisabled && state.alphaTest != kFuncAlways)
	{
		params.AddVectorParam (k11PixelAlphaRef*16, 1, kShaderVecFFAlphaTestRef);

		static const char* kCmpFunc[] =
		{
			"",						// kFuncDisabled
			"AlphaTestNever",		// kFuncNever
			"AlphaTestLess",		// kFuncLess
			"AlphaTestEqual",		// kFuncEqual
			"AlphaTestLEqual",		// kFuncLEqual
			"AlphaTestGreater",		// kFuncGreater
			"AlphaTestNotEqual",	// kFuncNotEqual
			"AlphaTestGEqual",		// kFuncGEqual
			"",						// kFuncAlways
		};

		NodePtr alphaTestNode;
		flg->CallFunction("", s_Linker.m_Module, kCmpFunc[state.alphaTest], &alphaTestNode);
		flg->PassValue(colorNode, D3D_RETURN_PARAMETER_INDEX, alphaTestNode, 0);
		colorNode = alphaTestNode;
	}

	// add specular
	if (state.lightingEnabled && state.specularEnabled)
	{
		NodePtr specNode;
		flg->CallFunction("", s_Linker.m_Module, "AddSpec", &specNode);
		flg->PassValue(colorNode, D3D_RETURN_PARAMETER_INDEX, specNode, 0);
		flg->PassValue(inputNode, inSpecReg, specNode, 1);
		colorNode = specNode;
	}

	// fog
	if (state.fogMode != kFogDisabled && inputRegCounter < 8)
	{
		
		params.AddVectorParam (k11PixelFog*16, 4, kShaderVecFFFogColor);

		NodePtr fogNode;
		flg->CallFunction("", s_Linker.m_Module, "ApplyFog", &fogNode);
		flg->PassValue(colorNode, D3D_RETURN_PARAMETER_INDEX, fogNode, 0);
		flg->PassValue(inputNode, inFogReg, fogNode, 1);
		colorNode = fogNode;
	}

	// ---- final steps

	NodePtr outputNode;
	Assert(outputRegCounter <= kMaxSignatureParams);
	hr = flg->SetOutputSignature(outputSig, outputRegCounter, &outputNode);

	hr = flg->PassValue(colorNode, D3D_RETURN_PARAMETER_INDEX, outputNode, outColorReg);

	#if 0
	// Print generated hlsl for debugging
	BlobPtr hlslOutput;
	hr = flg->GenerateHlsl(0, &hlslOutput);
	if (SUCCEEDED(hr))
	{
		printf_console("DX11 debug linked PS:\n%s\n", hlslOutput->GetBufferPointer());
	}
	#endif

	ModuleInstancePtr flgModule;
	BlobPtr flgErrors;
	hr = flg->CreateModuleInstance(&flgModule, &flgErrors);
	if (FAILED(hr))
	{
		const char* errorMsg = (const char*)flgErrors->GetBufferPointer();
		printf_console("DX11: Failed to create FF PS module: %s\n", errorMsg);
	}

	hr = linker->UseLibrary (s_Linker.m_ModuleInstancePS);

	#if UNITY_WINRT
	const char* target = "ps_4_0_level_9_1";
	#else
	const char* target = "ps_4_0";
	#endif

	BlobPtr linkedCode;
	BlobPtr linkedErrors;	
	hr = linker->Link(flgModule, "main", target, 0, &linkedCode, &linkedErrors);
	if (FAILED(hr))
	{
		const char* errorMsg = (const char*)linkedErrors->GetBufferPointer();
		printf_console("\nDX11: Failed to link FF PS: %s\n", errorMsg);
	}

	if (!linkedCode)
	{
		outSize = 0;
		return NULL;
	}
	outSize = linkedCode->GetBufferSize();
	void* finalCode = malloc(outSize);
	memcpy (finalCode, linkedCode->GetBufferPointer(), outSize);
	return finalCode;
}

#endif
