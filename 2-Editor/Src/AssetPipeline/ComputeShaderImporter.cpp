#include "UnityPrefix.h"
#include "ComputeShaderImporter.h"
#include "Runtime/Shaders/ComputeShader.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Scripting/TextAsset.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Editor/Src/Utility/TextUtilities.h"

#define CAN_IMPORT_COMPUTE_SHADERS (UNITY_WIN || UNITY_OSX)


#if CAN_IMPORT_COMPUTE_SHADERS
#include "Editor/Src/Utility/d3d11/D3D11Compiler.h"
#include "Editor/Src/Utility/d3d11/D3D11ReflectionAPI.h"

static D3D11Compiler s_Compiler;
#endif



// Increasing the version number causes an automatic reimport when the last imported version is a different.
// Only increase this number if an older version of the importer generated bad data or the importer applies data optimizations that can only be done in the importer.
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
enum { kComputeShaderImporterVersion = 4 };



IMPLEMENT_CLASS_HAS_INIT (ComputeShaderImporter);
IMPLEMENT_OBJECT_SERIALIZE (ComputeShaderImporter);

ComputeShaderImporter::ComputeShaderImporter(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

ComputeShaderImporter::~ComputeShaderImporter()
{
}


static int CanLoadPathName (const string& pathName, int* queue)
{
	const char* ext = GetPathNameExtension (pathName.c_str(), pathName.size());
	if (StrICmp(ext, "compute") == 0)
		return true;

	return false;
}


#if CAN_IMPORT_COMPUTE_SHADERS
static std::string GetComputeShaderCompilerPath()
{
	std::string compilerDllPath = AppendPathName (GetApplicationContentsPath(), "Tools/" kD3D11CompilerDLL);
	return compilerDllPath;
}
#endif


void ComputeShaderImporter::InitializeClass ()
{
	#if CAN_IMPORT_COMPUTE_SHADERS
	s_Compiler.Initialize (GetComputeShaderCompilerPath().c_str());
	#endif

	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (ComputeShaderImporter), kComputeShaderImporterVersion);
}

void ComputeShaderImporter::CleanupClass ()
{
	#if CAN_IMPORT_COMPUTE_SHADERS
	s_Compiler.Shutdown ();
	#endif
}

void ComputeShaderImporter::GenerateAssetData ()
{
	#if CAN_IMPORT_COMPUTE_SHADERS

	if (!s_Compiler.IsValid())
	{
		LogImportError (Format("Import of ComputeShader assets requires %s to be present", kD3D11CompilerDLL));
		return;
	}

	GenerateComputeShaderData ();

	#else
	LogImportError ("Importing ComputeShaders requires Windows.");
	#endif
}

template<class TransferFunction>
void ComputeShaderImporter::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	PostTransfer (transfer);
}

// --------------------------------------------------------------------------



#if CAN_IMPORT_COMPUTE_SHADERS

struct ComputeShaderIncludeHandler : D3D10Include
{
	string m_AssetFolder;

	virtual HRESULT WINAPI_IMPL Open(D3D10_INCLUDE_TYPE IncludeType, const char* pFileName, const void* pParentData, const void* *ppData, UINT *pBytes)
	{
		string pathName = AppendPathName (m_AssetFolder, pFileName);
		InputString fileData;
		if (!ReadStringFromFile (&fileData, pathName))
			return E_FAIL_IMPL;

		char* data = new char[fileData.size()];
		if (data == NULL)
			return E_OUTOFMEMORY_IMPL;

		memcpy (data, fileData.c_str(), fileData.size());
		*ppData = data;
		*pBytes = fileData.size();

		return S_OK_IMPL;
	}

	virtual HRESULT WINAPI_IMPL Close(const void* pData)
	{
		delete[] (char*)pData;
		return S_OK_IMPL;
	}
};


static void ParseHLSLErrorMessages (const string& listing, bool warnings, const string& token, ShaderErrors& outErrors)
{
	// HLSL error lines look like:
	// (11,3): warning X3568: 'vertex' : unknown pragma ignored
	// (26,7): error X3502: Microcode Compiler 'vert': input parameter 'v' missing semantics
	// error X3501: entrypoint not found

	size_t pos = listing.find (token, 0);
	if (pos == string::npos)
		return;

	while (pos != string::npos)
	{
		// before token comes either nothing, or newline, or "line,column): "
		int lineNumber = 0;
		if (pos > 3 && listing[pos-1]==' ' && listing[pos-2]==':' && listing[pos-3]==')')
		{
			int numberPos = pos-4;
			while (numberPos >= 0 && (isdigit(listing[numberPos]) || listing[numberPos]==','))
				--numberPos;
			++numberPos;
			string lineNumberText = listing.substr( numberPos, pos - numberPos );
			lineNumber = StringToInt(lineNumberText);
		}

		// extract error message (rest of line after token)
		string message = ExtractRestOfLine( listing, pos + token.size() );

		// Cg & HLSL put error codes right here, which are in the form of "C0501: " etc. If we match that, then just strip it.
		if (message.size() > 7)
		{
			if (isalpha(message[0]) && isdigit(message[1]) && isdigit(message[2]) && isdigit(message[3]) && isdigit(message[4]) && message[5]==':' && message[6]==' ')
				message.erase (0, 7);
		}

		outErrors.AddShaderError (message, lineNumber, warnings, true);

		pos = listing.find( token, pos + token.size() );
	}
}

typedef std::vector<ComputeShaderCB> ComputeShaderCBs;

static void ReflectComputeShader (const void* shaderCode, size_t shaderSize, ComputeShaderKernel& outKernel, ComputeShaderCBs& outCBs, ShaderErrors& outErrors)
{
	D3D11ShaderReflection* reflector = NULL;
	HRESULT hr = s_Compiler.D3DReflect (shaderCode, shaderSize, (void**)&reflector);
	if (FAILED_IMPL(hr))
	{
		outErrors.AddShaderError ("Failed to retrieve ComputeShader information", 0, false, true);
		return;
	}
	
	D3D11_SHADER_DESC shaderDesc;
	reflector->GetDesc (&shaderDesc);

	// constant buffers
	for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
	{
		D3D11ShaderReflectionConstantBuffer* cb = reflector->GetConstantBufferByIndex (i);
		D3D11_SHADER_BUFFER_DESC desc;
		cb->GetDesc (&desc);
		if (desc.Type == D3D11_CT_CBUFFER)
		{
			ComputeShaderCB shaderCB;
			shaderCB.byteSize = desc.Size;
			shaderCB.name.SetName (desc.Name);

			for (UINT j = 0; j < desc.Variables; ++j)
			{
				D3D11ShaderReflectionVariable* var = cb->GetVariableByIndex (j);
				D3D11_SHADER_VARIABLE_DESC vdesc;
				var->GetDesc (&vdesc);
				if (!(vdesc.uFlags & D3D10_SVF_USED))
					continue;
				D3D11_SHADER_TYPE_DESC tdesc;
				var->GetType()->GetDesc (&tdesc);
				ComputeShaderParam param;
				param.offset = vdesc.StartOffset;
				param.name.SetName (vdesc.Name);
				switch (tdesc.Type) {
				case D3D10_SVT_FLOAT: param.type = kCSParamFloat; break;
				case D3D10_SVT_INT: param.type = kCSParamInt; break;
				case D3D10_SVT_UINT: param.type = kCSParamUInt; break;
				default:
					outErrors.AddShaderError (Format("Unknown parameter type (%d) for %s", tdesc.Type, vdesc.Name), 0, false, true);
					continue;
				}
				if (tdesc.Class == D3D10_SVC_MATRIX_COLUMNS || tdesc.Class == D3D10_SVC_VECTOR || tdesc.Class == D3D10_SVC_SCALAR)
				{
					param.arraySize = tdesc.Elements;
					param.colCount = tdesc.Columns;
					param.rowCount = tdesc.Rows;
					shaderCB.params.push_back (param);
				}
				else
				{
					outErrors.AddShaderError (Format("Unknown parameter type class (%d) for %s", tdesc.Class, vdesc.Name), 0, false, true);
				}
			}
			outCBs.push_back(shaderCB);
		}
		else if (desc.Type == D3D11_CT_RESOURCE_BIND_INFO)
		{
			//@TODO: no idea what to do with them; things like RWStructuredBuffer<float4> Result : register( u0 ) in
			// DX SDK HDRToneMappingCS11/FilterCS.hlsl produce that
		}
		else
		{
			outErrors.AddShaderError (Format("Only scalar constant buffers are supported: %s", desc.Name), 0, false, true);
		}
	}

	// resources, except samplers
	std::vector<std::string> textureResourceNames;
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC desc;
		reflector->GetResourceBindingDesc (i, &desc);
		ComputeShaderResource res;
		res.name.SetName (desc.Name);
		res.bindPoint = desc.BindPoint;
		switch (desc.Type) {
		case D3D10_SIT_TEXTURE:
			res.bindPoint |= 0xFFFF0000; // no sampler by default
			outKernel.textures.push_back (res);
			textureResourceNames.push_back(desc.Name);
			break;
		case D3D10_SIT_SAMPLER: break; // skip samplers for now
		case D3D10_SIT_CBUFFER: outKernel.cbs.push_back (res); break;
		case D3D11_SIT_STRUCTURED: outKernel.inBuffers.push_back(res); break;
		case D3D11_SIT_UAV_RWTYPED:
		case D3D11_SIT_UAV_RWSTRUCTURED:
		case D3D11_SIT_BYTEADDRESS:
		case D3D11_SIT_UAV_RWBYTEADDRESS:
		case D3D11_SIT_UAV_APPEND_STRUCTURED:
		case D3D11_SIT_UAV_CONSUME_STRUCTURED:
		case D3D11_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
			outKernel.outBuffers.push_back(res); break;
		default:
			string msg = Format("Unrecognized resource binding type (%i) for %s", desc.Type, desc.Name);
			outErrors.AddShaderError (msg, 0, false, true);
			continue;
		}
	}

	// sampler resources (to match them up with any previous texture resources)
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC desc;
		reflector->GetResourceBindingDesc (i, &desc);
		if (desc.Type != D3D10_SIT_SAMPLER)
			continue; // only interested in samplers now

		std::string samplerName = desc.Name;

		Assert (outKernel.textures.size() == textureResourceNames.size());
		bool matchedTexture = false;
		for (size_t j = 0; j < outKernel.textures.size(); ++j)
		{
			if (textureResourceNames[j] == samplerName ||
				("sampler"+textureResourceNames[j]) == samplerName || 
				("sampler_"+textureResourceNames[j]) == samplerName)
			{
				outKernel.textures[j].bindPoint &= 0x0000FFFF;
				outKernel.textures[j].bindPoint |= (desc.BindPoint << 16);
				matchedTexture = true;
			}
		}
		if (!matchedTexture)
		{
			samplerName = ToLower(samplerName);
			const bool hasPoint = (samplerName.find("point") != std::string::npos);
			const bool hasLinear = (samplerName.find("linear") != std::string::npos);
			const bool hasClamp = (samplerName.find("clamp") != std::string::npos);
			const bool hasRepeat = (samplerName.find("repeat") != std::string::npos);
			ComputeShaderBuiltinSampler cbSampler;
			cbSampler.sampler = kBuiltinSamplerStateCount;
			cbSampler.bindPoint = desc.BindPoint;
			if (hasPoint && hasLinear)
			{
				string msg = Format("Unrecognized sampler '%s' - can't have both 'point' and 'linear'", desc.Name);
				outErrors.AddShaderError (msg, 0, false, true);
			}
			else if (hasClamp && hasRepeat)
			{
				string msg = Format("Unrecognized sampler '%s' - can't have both 'clamp' and 'repeat'", desc.Name);
				outErrors.AddShaderError (msg, 0, false, true);
			}
			else if (hasPoint && hasClamp)
				cbSampler.sampler = kSamplerPointClamp;
			else if (hasPoint && hasRepeat)
				cbSampler.sampler = kSamplerPointRepeat;
			else if (hasLinear && hasClamp)
				cbSampler.sampler = kSamplerLinearClamp;
			else if (hasLinear && hasRepeat)
				cbSampler.sampler = kSamplerLinearRepeat;
			else
			{
				string msg = Format("Unrecognized sampler '%s' - does not match any texture and is not a built-in name", desc.Name);
				outErrors.AddShaderError (msg, 0, false, true);
			}

			if (cbSampler.sampler != kBuiltinSamplerStateCount)
				outKernel.builtinSamplers.push_back(cbSampler);
		}
	}

	SAFE_RELEASE(reflector);
}

struct ComputeImportDirectives {
	typedef std::pair<string,string> Macro;
	struct Kernel {
		string name;
		std::vector<Macro> macros;
	};

	std::vector<Kernel> kernels;
};

static bool ParseComputeImportDirectives (string& src, ComputeImportDirectives& outParams)
{
	size_t pos;

	// go over all pragma statements
	bool hadAnyKnownPragmas = false;
	const string kPragmaToken("#pragma ");
	pos = FindTokenInText (src, kPragmaToken, 0);
	while (pos != string::npos)
	{
		bool knownPragma = false;

		// extract line
		string line = ExtractRestOfLine(src, pos + kPragmaToken.size());
		string pragmaLine = kPragmaToken + line;

		size_t linePos = 0;
		// skip whitespace
		linePos = SkipWhiteSpace (line, linePos);

		// read pragma name
		string pragmaName = ReadNonWhiteSpace (line, linePos);
		// skip whitespace after pragma name
		linePos = SkipWhiteSpace (line, linePos);

		if (pragmaName == "kernel")
		{
			// compute kernel entry point
			ComputeImportDirectives::Kernel kernel;
			kernel.name = ReadNonWhiteSpace (line, linePos);

			// read optional preprocessor defines following the name
			while(true) {
				linePos = SkipWhiteSpace (line, linePos);
				string macroString = ReadNonWhiteSpace (line, linePos);
				if (macroString.empty())
					break;
				ComputeImportDirectives::Macro macro;
				size_t equalsPos = macroString.find ('=');
				if (equalsPos == string::npos)
				{
					macro.first = macroString;
					macro.second.clear();
				}
				else
				{
					macro.first = macroString.substr (0, equalsPos);
					macro.second = macroString.substr (equalsPos + 1, macroString.size()-equalsPos-1);
				}
				kernel.macros.push_back (macro);
			}


			outParams.kernels.push_back (kernel);
			knownPragma = true;
		}

		if (knownPragma)
		{
			hadAnyKnownPragmas = true;

			// comment it out so that it does not confuse actual compute shader compiler
			src.insert (pos, "//");
			pos += 2;
		}

		// find next pragma
		pos += kPragmaToken.size();
		pos = FindTokenInText(src, kPragmaToken, pos);
	}

	return hadAnyKnownPragmas;
}



static void CompileShaderKernel (const string& source, const string& pathName, const string& fileName, const ComputeImportDirectives::Kernel& kernel, std::vector<ComputeShaderCBs>& outCBs, ComputeShader& shader)
{
	HRESULT hr;
	D3D10Blob *d3dshader = NULL;
	D3D10Blob *d3derrors = NULL;
	D3D10Blob* d3dStrippedShader = NULL;

	ComputeShaderIncludeHandler includeHandler;
	includeHandler.m_AssetFolder = DeleteLastPathNameComponent (pathName);

	D3D10_SHADER_MACRO* macros = new D3D10_SHADER_MACRO[kernel.macros.size()+1];
	for (size_t i = 0; i < kernel.macros.size(); ++i)
	{
		macros[i].Name = kernel.macros[i].first.c_str();
		macros[i].Definition = kernel.macros[i].second.empty() ? "1" : kernel.macros[i].second.c_str();
	}
	macros[kernel.macros.size()].Name = NULL;
	macros[kernel.macros.size()].Definition = NULL;

	const char* targetName = "cs_5_0";
	hr = s_Compiler.D3DCompile (
		source.c_str(),
		source.size(),
		fileName.c_str(),
		macros,
		&includeHandler,
		kernel.name.c_str(),
		targetName,
		D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY, // | D3D10_SHADER_DEBUG | D3D10_SHADER_PREFER_FLOW_CONTROL | D3D10_SHADER_SKIP_OPTIMIZATION,
		0,
		&d3dshader,
		&d3derrors);

	if (d3derrors)
	{
		std::string msg (reinterpret_cast<const char*>(d3derrors->GetBufferPointer()), d3derrors->GetBufferSize());
		ParseHLSLErrorMessages (msg, false, "fatal error ", shader.GetErrors());
		ParseHLSLErrorMessages (msg, false, "error ", shader.GetErrors());
		ParseHLSLErrorMessages (msg, true, "warning ", shader.GetErrors());
	}
	else if (FAILED_IMPL(hr))
	{
		SAFE_RELEASE(d3dshader);
		shader.GetErrors().AddShaderError ("Compilation failed (unknown error)", 0, false, true);
	}

	if (d3dshader)
	{
		// Get compiled shader
		const BYTE* shaderCode = reinterpret_cast<const BYTE*>(d3dshader->GetBufferPointer());
		DWORD shaderSize = d3dshader->GetBufferSize();

		ComputeShaderKernel shaderKernel;
		shaderKernel.name.SetName(kernel.name.c_str());

		// Figure out parameters, buffers etc.
		ComputeShaderCBs cbs;
		ReflectComputeShader (shaderCode, shaderSize, shaderKernel, cbs, shader.GetErrors());

		// Strip reflect/debug information
		hr = s_Compiler.D3DStripShader (shaderCode, shaderSize, D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO | D3DCOMPILER_STRIP_TEST_BLOBS, &d3dStrippedShader);
		if (SUCCEEDED_IMPL(hr))
		{
			shaderCode = reinterpret_cast<const BYTE*>(d3dStrippedShader->GetBufferPointer());
			shaderSize = d3dStrippedShader->GetBufferSize();
		}

		shaderKernel.code.assign (shaderCode, shaderCode+shaderSize);

		shader.GetKernels().push_back (shaderKernel);
		outCBs.push_back (cbs);
	}
	SAFE_RELEASE (d3derrors);
	SAFE_RELEASE (d3dshader);
	SAFE_RELEASE (d3dStrippedShader);
}


template<typename T>
static T& FindOrAddByName (typename std::vector<T>& arr, const T& t)
{
	for (size_t i = 0, n = arr.size(); i < n; ++i)
	{
		if (arr[i].name == t.name)
			return arr[i];
	}
	arr.push_back (t);
	return arr.back();
}


static void CheckAndAssignConstantBuffers (ComputeShader& shader, const std::vector<ComputeShaderCBs>& kernelCBs)
{
	// we need to compute the union of all CBs used by all kernels, and check that parameters in each
	// match up between kernels

	ComputeShaderCBs resultCBs;

	for (size_t k = 0, nk = kernelCBs.size(); k < nk; ++k)
	{
		const ComputeShaderCBs& kernelcb = kernelCBs[k];
		for (size_t cb = 0, ncb = kernelcb.size(); cb < ncb; ++cb)
		{
			// find or add this CB to the result CB set
			const ComputeShaderCB& srcCB = kernelcb[cb];
			ComputeShaderCB& dstCB = FindOrAddByName (resultCBs, srcCB);
			DebugAssert (srcCB.name == dstCB.name);
			dstCB.byteSize = std::max (dstCB.byteSize, srcCB.byteSize);

			// now go over all properties of src CB, add them to destination CB if not there yet;
			// check property data if already there
			for (size_t p = 0, np = srcCB.params.size(); p < np; ++p)
			{
				const ComputeShaderParam& srcP = srcCB.params[p];
				ComputeShaderParam& dstP = FindOrAddByName (dstCB.params, srcP);
				DebugAssert (srcP.name == dstP.name);

				if (srcP.type != dstP.type || srcP.offset != dstP.offset || srcP.arraySize != dstP.arraySize || srcP.rowCount != dstP.rowCount || srcP.colCount != dstP.colCount)
				{
					string msg = Format("All kernels must use same constant buffer layouts; '%s' property found to be different", srcP.name.GetName());
					shader.GetErrors().AddShaderError (msg, 0, false, true);
				}
			}
		}
	}

	// add resulting constant buffer set to the compute shader
	shader.GetConstantBuffers() = resultCBs;
}

void ComputeShaderImporter::GenerateComputeShaderData()
{
	const string pathName = GetAssetPathName ();
	const string fileName = GetLastPathNameComponent (pathName);

	// remove console messages from old shader
	ComputeShader* oldAsset = GetFirstDerivedObjectNamed<ComputeShader> (fileName);
	if (oldAsset)
		RemoveErrorWithIdentifierFromConsole (oldAsset->GetInstanceID ());
	InputString sourceInputStr;
	if (!ReadStringFromFile (&sourceInputStr, pathName))
	{
		LogImportError ("Couldn't read shader script file!");
		return;
	}

	// skip UTF8 BOM if present
	size_t sourceInputSize = sourceInputStr.size();
	size_t sourceInputStart = 0;
	if (sourceInputSize >= 3)
	{
		if (sourceInputStr[0]=='\xEF' && sourceInputStr[1]=='\xBB' && sourceInputStr[2]=='\xBF')
		{
			sourceInputStart = 3;
			sourceInputSize -= 3;
		}
	}

	std::string source (sourceInputStr.c_str() + sourceInputStart, sourceInputSize);

	ComputeImportDirectives importDirectives;
	bool hadImportDirectives = ParseComputeImportDirectives (source, importDirectives);

	// if there were no import directives, treat the file as just an include file
	if (!hadImportDirectives || importDirectives.kernels.empty())
	{
		TextAsset* text = &RecycleExistingAssetObject<TextAsset>();
		if (!text)
			return;
		text->SetScript (source);
		text->AwakeFromLoad (kDefaultAwakeFromLoad);
		return;
	}

	// produce compute shader object
	ComputeShader* shader = &RecycleExistingAssetObject<ComputeShader>();
	if (!shader)
		return;

	shader->GetErrors().Clear();
	shader->GetKernels().clear();
	shader->GetConstantBuffers().clear();


	// do D3D shader compile for each kernel
	std::vector<ComputeShaderCBs> kernelCBs;
	for (size_t i = 0; i < importDirectives.kernels.size(); ++i)
	{
		CompileShaderKernel (source, pathName, fileName, importDirectives.kernels[i], kernelCBs, *shader);
	}

	CheckAndAssignConstantBuffers (*shader, kernelCBs);

	shader->GetErrors().LogErrors (fileName.c_str(), fileName.c_str(), shader, shader->GetInstanceID());

	shader->AwakeFromLoad(kDefaultAwakeFromLoad);
}
#endif // CAN_IMPORT_COMPUTE_SHADERS


// ------------------------------------------------------------------

#if defined(ENABLE_UNIT_TESTS) && CAN_IMPORT_COMPUTE_SHADERS
#include "../../../UnitTest++/src/UnitTest++.h"
SUITE (ComputeShaderImporterTests)
{
TEST(ComputeShaderImporter_HLSLCompilerExists)
{
	s_Compiler.Initialize (GetComputeShaderCompilerPath().c_str());
	CHECK(s_Compiler.IsValid());
	s_Compiler.Shutdown();
}
}
#endif // #if defined(ENABLE_UNIT_TESTS) && CAN_IMPORT_COMPUTE_SHADERS
