#include "UnityPrefix.h"
#include "GpuProgram.h"
#include "GfxPatchInfo.h"
#include "External/shaderlab/Library/ShaderLabErrors.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "GfxDevice.h"

#if GFX_SUPPORTS_OPENGL
#include "Runtime/GfxDevice/opengl/GpuProgramsGL.h"
#include "Runtime/GfxDevice/opengl/ArbGpuProgamGL.h"
#endif
#if GFX_SUPPORTS_OPENGLES30
#include "Runtime/GfxDevice/opengles30/GpuProgramsGLES30.h"
#endif
#if GFX_SUPPORTS_OPENGLES20
#include "Runtime/GfxDevice/opengles20/GpuProgramsGLES20.h"
#endif
#if GFX_SUPPORTS_D3D9
#include "Runtime/GfxDevice/d3d/GpuProgramsD3D.h"
#endif
#if GFX_SUPPORTS_D3D11
#include "d3d11/GpuProgramsD3D11.h"
#endif
#if GFX_SUPPORTS_GCM
#include "GpuProgramPS3.h"
#endif
#if GFX_SUPPORTS_XENON
#include "GpuProgramsXenon.h"
#endif
#if GFX_SUPPORTS_MOLEHILL
#include "Runtime/GfxDevice/molehill/GpuProgramsMH.h"
#endif


// --------------------------------------------------------------------------
void GpuProgramParameters::AddValueParam (const ValueParameter& param)
{
	m_ValueParams.push_back (param);
	m_ValuesSize += param.m_ArraySize * param.m_RowCount * param.m_ColCount * sizeof(float);
	m_Status = kDirty;
}

void GpuProgramParameters::AddTextureParam (const TextureParameter& param)
{
	m_TextureParams.push_back (param);
	m_ValuesSize += sizeof(TexEnvData);
	m_Status = kDirty;
}

void GpuProgramParameters::AddVectorParam (int index, ShaderParamType type, int dimension, const char* nameStr, int cbIndex, PropertyNamesSet* outNames)
{
	int cbNameID = -1;
	#if GFX_SUPPORTS_CONSTANT_BUFFERS
	if (cbIndex >= 0)
		cbNameID = m_ConstantBuffers[cbIndex].m_Name.index;
	#endif
	if (m_BuiltinParams.CheckVectorParam (nameStr, index, dimension, cbNameID))
		return;

	ValueParameterArray& params = GetValuesArray(cbIndex);
	const FastPropertyName name = ShaderLab::Property(nameStr);
	params.push_back (ValueParameter (name, type, index, 1, 1, dimension));
	const UInt32 size = sizeof(Vector4f);
	m_ValuesSize += size;
	m_Status = kDirty;

	if (outNames && !name.IsBuiltin() && outNames->names.insert(name.index).second)
		outNames->valueSize += size;
}

void GpuProgramParameters::AddMatrixParam (int index, const char* nameStr, int rowCount, int colCount, int cbIndex, PropertyNamesSet* outNames)
{
	Assert(rowCount <= 4);

	int cbNameID = -1;
	#if GFX_SUPPORTS_CONSTANT_BUFFERS
	if (cbIndex >= 0)
		cbNameID = m_ConstantBuffers[cbIndex].m_Name.index;
	#endif

	if (m_BuiltinParams.CheckMatrixParam (nameStr, index, rowCount, colCount, cbNameID))
		return;

	ValueParameterArray& params = GetValuesArray(cbIndex);
	const FastPropertyName name = ShaderLab::Property(nameStr);
	params.push_back (ValueParameter (name, kShaderParamFloat, index, 1, rowCount, colCount));
	const UInt32 size = sizeof(int) + sizeof(Matrix4x4f);
	m_ValuesSize += size;
	m_Status = kDirty;

	if (outNames && !name.IsBuiltin() && outNames->names.insert(name.index).second)
		outNames->valueSize += size;
}

void GpuProgramParameters::AddTextureParam (int index, int samplerIndex, const char* nameStr, TextureDimension dim, PropertyNamesSet* outNames)
{
	const FastPropertyName name = ShaderLab::Property(nameStr);
	m_TextureParams.push_back (TextureParameter (name, index, samplerIndex, dim));
	const UInt32 size = sizeof(TexEnvData);
	m_ValuesSize += size;
	m_Status = kDirty;

	if (outNames && !name.IsBuiltin() && outNames->names.insert(name.index).second)
		outNames->valueSize += size;
}

void GpuProgramParameters::AddBufferParam (int index, const char* nameStr, PropertyNamesSet* outNames)
{
	const FastPropertyName name = ShaderLab::Property(nameStr);
	m_BufferParams.push_back (BufferParameter (name, index));
	const UInt32 size = sizeof(ComputeBufferID);
	m_ValuesSize += size;

	if (outNames && !name.IsBuiltin() && outNames->names.insert(name.index).second)
		outNames->valueSize += size;
}

const GpuProgramParameters::ValueParameterArray& GpuProgramParameters::GetValueParams() const
{
	if (IsDirty())
		const_cast<GpuProgramParameters*>(this)->MakeReady();
	return m_ValueParams;
}

const GpuProgramParameters::TextureParameter* GpuProgramParameters::FindTextureParam(const FastPropertyName& name, TextureDimension dim) const
{
	for (TextureParameterList::const_iterator it = m_TextureParams.begin(), itEnd = m_TextureParams.end(); it != itEnd; ++it)
	{
		const TextureParameter& tex = *it;
		if (tex.m_Name == name && tex.m_Dim == dim)
			return &tex;
	}
	return NULL;
}


const GpuProgramParameters::ValueParameter* GpuProgramParameters::FindParam (const FastPropertyName& name, int* outCBIndex) const
{
	DebugAssert(!IsDirty());
	NameToValueIndex key = { name.index, 0, 0 };
	// Binary search using lower_bound should be faster, but seems slower...
	//NameToValueIndexMap::const_iterator pos = std::lower_bound(m_NamedParams.begin(), m_NamedParams.end(), key);
	NameToValueIndexMap::const_iterator pos = std::find(m_NamedParams.begin(), m_NamedParams.end(), key);
	const GpuProgramParameters::ValueParameter* param = NULL;
	if (pos != m_NamedParams.end())
	{
		const ValueParameterArray& params = GetValuesArray(pos->cbIndex);
		param = &params[pos->valueIndex];
		DebugAssert(name.index == param->m_Name.index);

		#if GFX_SUPPORTS_CONSTANT_BUFFERS
		if (outCBIndex)
			*outCBIndex = pos->cbIndex;
		#endif
	}
	return param;
}


template <typename T>
static inline UInt8* PushToBuffer (UInt8* buffer, const T& val)
{
	memcpy (buffer, &val, sizeof(val));
	return buffer + sizeof(val);
}

static UInt8* PrepareValueParameters (
	const GpuProgramParameters::ValueParameterArray& valueParams,
	const ShaderLab::PropertySheet* props,
	UInt8* buffer,
	const UInt8* bufferStart,
	GfxPatchInfo* outPatchInfo)
{
	using namespace ShaderLab;	
	using namespace ShaderLab::shaderprops;	
	PropertyLocation location;
	bool missing;

	GpuProgramParameters::ValueParameterArray::const_iterator valueParamsEnd = valueParams.end();
	for (GpuProgramParameters::ValueParameterArray::const_iterator i = valueParams.begin(); i != valueParamsEnd; ++i)
	{
		FastPropertyName name = i->m_Name;
		int rows = i->m_RowCount;
		int cols = i->m_ColCount;
		int arrSize = i->m_ArraySize;
		if (rows == 1 && arrSize <= 1)
		{
			// single floats and vectors
			if (cols == 1) {
				const float& src = GetFloat(props, name, location);
				if (outPatchInfo && IsPatchable(location, missing)) {
					outPatchInfo->AddPatch(GfxPatch::kTypeFloat,
						GfxPatch(name, missing ? NULL : &src, buffer - bufferStart));
				}
				Vector4f val(src,0,0,0);
				buffer = PushToBuffer (buffer, val);
			} else {
				const Vector4f& src = GetVector(props, name, location);
				if (outPatchInfo && IsPatchable(location, missing)) {
					outPatchInfo->AddPatch(GfxPatch::kTypeVector,
						GfxPatch(name, missing ? NULL : &src, buffer - bufferStart));
				}
				buffer = PushToBuffer (buffer, src);
			}
		}
		else
		{
			// matrices, arrays etc.			
			int expectSize = arrSize * rows * cols;
			
			//@TODO: for now, arrays are not really supported yet; and a bunch of
			// other code around us expect matrices to always be 4x4, even if
			// actual shader uses less. Right now this is only a problematic case
			// for Xbox360, since on other platforms the compiler never "strips away"
			// unreferenced matrix constant slots.
			// When we actually add shader parameter array support, this place,
			// and a ton of other places, would have to remove 4x4 assumptions.
			if (expectSize < 16)
				expectSize = 16;
			
			int gotSize;
			const float* src = GetValueProp (props, name, expectSize, &gotSize, location);
			DebugAssert (gotSize <= expectSize);
			memcpy (buffer, &gotSize, sizeof(gotSize)); buffer += sizeof(gotSize); // size
			if (outPatchInfo && IsPatchable(location, missing)) {
				//@TODO: once we have arrays here, it's not necessarily matrix anymore!
				outPatchInfo->AddPatch(GfxPatch::kTypeMatrix,
					GfxPatch(name, missing ? NULL : src, buffer - bufferStart));
			}
			memcpy (buffer, src, gotSize*sizeof(float)); buffer += gotSize*sizeof(float); // value
		}
	}
	return buffer;
}

UInt8* GpuProgramParameters::PrepareValues (
	const ShaderLab::PropertySheet* props,
	UInt8* buffer,
	const UInt8* bufferStart,
	GfxPatchInfo* outPatchInfo,
	bool* outMissingTextures) const
{
	using namespace ShaderLab;	
	using namespace ShaderLab::shaderprops;	

	// Value parameters

	buffer = PrepareValueParameters (GetValueParams(), props, buffer, bufferStart, outPatchInfo);

	#if GFX_SUPPORTS_CONSTANT_BUFFERS
	for (size_t i = 0; i < m_ConstantBuffers.size(); ++i)
	{
		buffer = PrepareValueParameters (m_ConstantBuffers[i].m_ValueParams, props, buffer, bufferStart, outPatchInfo);
	}
	#endif

	// textures
	TextureParameterList::const_iterator textureParamsEnd = m_TextureParams.end();
	for (TextureParameterList::const_iterator i = m_TextureParams.begin(); i != textureParamsEnd; ++i)
	{
		const GpuProgramParameters::TextureParameter& t = *i;
		FastPropertyName name = t.m_Name;
		TextureDimension dim = t.m_Dim;
		TexEnvData* dest = reinterpret_cast<TexEnvData*>(buffer);
		if (outPatchInfo)
		{
			DebugAssert(outMissingTextures);
			if (!outPatchInfo->AddPatchableTexEnv(name, FastPropertyName(), t.m_Dim, dest, bufferStart, props))
				*outMissingTextures = true;
		}
		else
		{
			TexEnv* src = GetTexEnv(props, name, dim);
			src->PrepareData(name.index, FastPropertyName(), props, dest);
		}
		buffer += sizeof(TexEnvData);
	}

	// buffers
	bool missing;
	BufferParameterArray::const_iterator bufferParamsEnd = m_BufferParams.end();
	for (BufferParameterArray::const_iterator i = m_BufferParams.begin(); i != bufferParamsEnd; ++i)
	{
		const BufferParameter& t = *i;

		PropertyLocation location;
		const ComputeBufferID& buf = shaderprops::GetComputeBuffer(props, t.m_Name, location);
		if (outPatchInfo && IsPatchable(location, missing)) {
			outPatchInfo->AddPatch(GfxPatch::kTypeBuffer,
				GfxPatch(t.m_Name, missing ? NULL : &buf, buffer - bufferStart));
		}

		ComputeBufferID* destPtr = reinterpret_cast<ComputeBufferID*>(buffer);
		*destPtr = buf; buffer += sizeof(ComputeBufferID);
	}

	return buffer;
}

void GpuProgramParameters::MakeValueParamsReady (ValueParameterArray& values, int cbIndex)
{
	std::sort (values.begin(), values.end());
	int size = values.size();
	for (int i = 0; i < size; i++)
	{
		const ValueParameter& param = values[i];
		NameToValueIndex key = { param.m_Name.index, cbIndex, i };
		m_NamedParams.push_back(key);
	}
}


void GpuProgramParameters::MakeReady()
{
	if (IsDirty())
	{
		m_NamedParams.clear();
		MakeValueParamsReady (m_ValueParams, -1);
		#if GFX_SUPPORTS_CONSTANT_BUFFERS
		for (size_t cb = 0; cb < m_ConstantBuffers.size(); ++cb)
			MakeValueParamsReady (m_ConstantBuffers[cb].m_ValueParams, cb);
		#endif
		std::sort(m_NamedParams.begin(), m_NamedParams.end());
	}
	m_Status = kReady;
}

// --------------------------------------------------------------------------


bool CheckGpuProgramUsable (const char* str)
{
	GfxDeviceRenderer rendererType = GetGfxDevice().GetRenderer();

	// Pretend program is usable if we are building shaders in "nographics" mode
	if( rendererType == kGfxRendererNull )
		return true;

	// determine the kind of program from the start of the string
	if( !strncmp (str, "!!ARBvp1.0", 10) || !strncmp (str, "3.0-!!ARBvp1.0", 14) ) {
		#if GFX_SUPPORTS_OPENGL
		if( rendererType == kGfxRendererOpenGL )
			return true;
		#endif
		return false;
	} else if( !strncmp (str, "!!ARBfp1.0", 10) || !strncmp (str, "3.0-!!ARBfp1.0", 14) ) {
		#if GFX_SUPPORTS_OPENGL
		if( rendererType == kGfxRendererOpenGL )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "!!GLSL", 6)) {
		#if GFX_SUPPORTS_OPENGL
		if( rendererType == kGfxRendererOpenGL )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "!!GLES3", 7)) {
		#if GFX_SUPPORTS_OPENGLES30
		if( rendererType = kGfxRendererOpenGLES30 )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "!!GLES", 6)) {
		#if GFX_SUPPORTS_OPENGLES20
		if( rendererType == kGfxRendererOpenGLES20Mobile || rendererType == kGfxRendererOpenGLES20Desktop )
			return true;
		#endif
		#if GFX_SUPPORTS_OPENGLES30
		if( rendererType = kGfxRendererOpenGLES30 )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "!!ATIfs1.0", 10)) {
		printf_console ("@TODO: found ATIfs1.0 shader; those are not supported anymore\n");
		return false;
	} else if (!strncmp (str, "vs_1_1", 6) || !strncmp (str, "vs_2_0", 6) || !strncmp (str, "vs_3_0", 6)) {
		#if GFX_SUPPORTS_D3D9
		if( rendererType == kGfxRendererD3D9 )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "ps_2_0", 6) || !strncmp (str, "ps_3_0", 6)) {
		#if GFX_SUPPORTS_D3D9
		if( rendererType == kGfxRendererD3D9 )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "vs_4_0_level_9", strlen("vs_4_0_level_9"))) {
		#if GFX_SUPPORTS_D3D11
		if( rendererType == kGfxRendererD3D11 && gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0 )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "ps_4_0_level_9", strlen("ps_4_0_level_9"))) {
		#if GFX_SUPPORTS_D3D11
		if( rendererType == kGfxRendererD3D11 && gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0 )
			return true;
		#endif
		return false;
  } else if (!strncmp (str, "vs_dx11", 7) || !strncmp (str, "vs_4_0", 6) || !strncmp (str, "vs_5_0", 6)) {
		#if GFX_SUPPORTS_D3D11
		if( rendererType == kGfxRendererD3D11 && gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "ps_dx11", 7) || !strncmp (str, "ps_4_0", 6) || !strncmp (str, "ps_5_0", 6)) {
		#if GFX_SUPPORTS_D3D11
		if( rendererType == kGfxRendererD3D11 && gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "gs_4_0", 6) || !strncmp (str, "gs_5_0", 6) || !strncmp (str, "hs_5_0", 6) || !strncmp (str, "ds_5_0", 6)) {
		#if GFX_SUPPORTS_D3D11
		if( rendererType == kGfxRendererD3D11 && gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "vs_360", 6)) {
		#if GFX_SUPPORTS_XENON
		if( rendererType == kGfxRendererXenon )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "ps_360", 6)) {
		#if GFX_SUPPORTS_XENON
		if( rendererType == kGfxRendererXenon )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "sce_vp_rsx", 10)) {
		#if GFX_SUPPORTS_GCM
		if( rendererType == kGfxRendererGCM )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "sce_fp_rsx", 10)) {
		#if GFX_SUPPORTS_GCM
		if( rendererType == kGfxRendererGCM )
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "agal_vs", 7)) {
		#if GFX_SUPPORTS_MOLEHILL
		if (rendererType == kGfxRendererMolehill)
			return true;
		#endif
		return false;
	} else if (!strncmp (str, "agal_ps", 7)) {
		#if GFX_SUPPORTS_MOLEHILL
		if (rendererType == kGfxRendererMolehill)
			return true;
		#endif
		return false;
	}

	// If we got here, it's something unrecognized. Return that it's usable,
	// this will make current SubShader be not supported.
	return true;
}

GpuProgram* CreateGpuProgram( const std::string& source, CreateGpuProgramOutput& output )
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
	GpuProgram* program = NULL;
	
	// determine the kind of program from the start of the string
	GfxDeviceRenderer rendererType = GetRealGfxDevice().GetRenderer();

	if( !strncmp (source.c_str(), "!!ARBvp1.0", 10) || !strncmp (source.c_str(), "3.0-!!ARBvp1.0", 14) ) {
		#if GFX_SUPPORTS_OPENGL
		if( rendererType == kGfxRendererOpenGL )
			program = new ArbGpuProgram( source, kShaderVertex, kShaderImplVertex );
		#endif
	} else if( !strncmp (source.c_str(), "!!ARBfp1.0", 10) || !strncmp (source.c_str(), "3.0-!!ARBfp1.0", 14) ) {
		#if GFX_SUPPORTS_OPENGL
		if( rendererType == kGfxRendererOpenGL )
			program = new ArbGpuProgram( source, kShaderFragment, kShaderImplFragment );
		#endif
	} else if (!strncmp (source.c_str(), "!!GLSL", 6)) {
		#if GFX_SUPPORTS_OPENGL
		if( rendererType == kGfxRendererOpenGL )
			program = new GlslGpuProgram( source.substr(6,source.size()-6), output );
		#endif
	} else if (!strncmp (source.c_str(), "!!GLES3", 7)) {
		#if GFX_SUPPORTS_OPENGLES30
		if( rendererType == kGfxRendererOpenGLES30 )
			program = new GlslGpuProgramGLES30(source.substr(7,source.size()-7), output);
		#endif
	} else if (!strncmp (source.c_str(), "!!GLES", 6)) {
		#if GFX_SUPPORTS_OPENGLES20
		if( rendererType == kGfxRendererOpenGLES20Mobile || rendererType == kGfxRendererOpenGLES20Desktop )
			program = new GlslGpuProgramGLES20( source.substr(6,source.size()-6), output );
		#endif
		#if GFX_SUPPORTS_OPENGLES30
		if( rendererType == kGfxRendererOpenGLES30 )
			program = new GlslGpuProgramGLES30( source.substr(6,source.size()-6), output);
		#endif
	} else if (!strncmp (source.c_str(), "!!ATIfs1.0", 10)) {
		printf_console ("@TODO: found ATIfs1.0 shader; those are not supported anymore\n");
	} else if (!strncmp (source.c_str(), "vs_1_1", 6) || !strncmp (source.c_str(), "vs_2_0", 6) || !strncmp (source.c_str(), "vs_3_0", 6)) {
		#if GFX_SUPPORTS_D3D9
		if( rendererType == kGfxRendererD3D9 )
			program = new D3D9VertexShader( source );
		#endif
	} else if (!strncmp (source.c_str(), "ps_2_0", 6) || !strncmp (source.c_str(), "ps_3_0", 6)) {
		#if GFX_SUPPORTS_D3D9
		if( rendererType == kGfxRendererD3D9 )
			program = new D3D9PixelShader( source );
		#endif
	} else if (!strncmp (source.c_str(), "vs_4_0_level_9", strlen("vs_4_0_level_9"))) {
		#if GFX_SUPPORTS_D3D11
		if( rendererType == kGfxRendererD3D11 && gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0 )
			program = new D3D11VertexShader( source );
		#endif
	} else if (!strncmp (source.c_str(), "ps_4_0_level_9", strlen("ps_4_0_level_9"))) {
		#if GFX_SUPPORTS_D3D11
		if( rendererType == kGfxRendererD3D11 && gGraphicsCaps.d3d11.featureLevel < kDX11Level10_0 )
			program = new D3D11PixelShader( source );
		#endif
	} else if (!strncmp (source.c_str(), "vs_dx11", 7) || !strncmp (source.c_str(), "vs_4_0", 6) || !strncmp (source.c_str(), "vs_5_0", 6)) {
		#if GFX_SUPPORTS_D3D11
		if( rendererType == kGfxRendererD3D11 && gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 )
			program = new D3D11VertexShader( source );
		#endif
	} else if (!strncmp (source.c_str(), "ps_dx11", 7) || !strncmp (source.c_str(), "ps_4_0", 6) || !strncmp (source.c_str(), "ps_5_0", 6)) {
		#if GFX_SUPPORTS_D3D11
		if( rendererType == kGfxRendererD3D11 && gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 )
			program = new D3D11PixelShader( source );
		#endif
	} else if (!strncmp (source.c_str(), "gs_4_0", 6) || !strncmp (source.c_str(), "gs_5_0", 6)) {
		#if GFX_SUPPORTS_D3D11
		if (rendererType == kGfxRendererD3D11)
			program = new D3D11GeometryShader(source);
		#endif
	} else if (!strncmp (source.c_str(), "hs_5_0", 6)) {
		#if GFX_SUPPORTS_D3D11
		if (rendererType == kGfxRendererD3D11)
			program = new D3D11HullShader(source);
		#endif
	} else if (!strncmp (source.c_str(), "ds_5_0", 6)) {
		#if GFX_SUPPORTS_D3D11
		if (rendererType == kGfxRendererD3D11)
			program = new D3D11DomainShader(source);
		#endif
	} else if (!strncmp (source.c_str(), "vs_360", 6)) {
		#if GFX_SUPPORTS_XENON
		if( rendererType == kGfxRendererXenon )
			program = new XenonVertexShader (source);
		#endif
	} else if (!strncmp (source.c_str(), "ps_360", 6)) {
		#if GFX_SUPPORTS_XENON
		if( rendererType == kGfxRendererXenon )
			program = new XenonPixelShader (source);
		#endif
	} else if (!strncmp (source.c_str(), "sce_vp_rsx", 10)) {
		#if GFX_SUPPORTS_GCM
		if( rendererType == kGfxRendererGCM )
			program = new GcmGpuProgram( source );
		#endif
	} else if (!strncmp (source.c_str(), "sce_fp_rsx", 10)) {
		#if GFX_SUPPORTS_GCM
		if( rendererType == kGfxRendererGCM )
			program = new GcmGpuProgram( source );
		#endif
	} else if (!strncmp (source.c_str(), "agal_vs", 7)) {
		#if GFX_SUPPORTS_MOLEHILL
		if (rendererType == kGfxRendererMolehill)
			program = new GpuProgramMH (source, kShaderVertex);
		#endif
	} else if (!strncmp (source.c_str(), "agal_ps", 7)) {
		#if GFX_SUPPORTS_MOLEHILL
		if (rendererType == kGfxRendererMolehill)
			program = new GpuProgramMH (source, kShaderFragment);
		#endif
	} else {
		if( source == "!!error" )
		{
			// we've got a program that had errors when compiling. Silently fail, errors
			// were dumped before
			printf_console( "Shader had programs with errors, disabling subshader\n" );
		}
		else if( source.empty() )
		{
			output.CreateShaderErrors().AddShaderError ("Empty program string", -1, false);
		}
		else
		{
			const int kProgramStartChars = 10;
			int len = source.size() < kProgramStartChars ? source.size() : kProgramStartChars;
			output.CreateShaderErrors().AddShaderError (Format("Unrecognized program string: %s ...", source.substr(0,len).c_str()), -1, false);
		}
	}

	// TODO: immediately check if not supported?
		
	return program;
#else
	return NULL;
#endif
}


// --------------------------------------------------------------------------

GpuProgram::GpuProgram()
{
	m_ImplType = kShaderImplUndefined;
	m_GpuProgramLevel = kGpuProgramNone;
	m_NotSupported = false;
	m_WasDestroyed = false;
}

GpuProgram::~GpuProgram ()
{
}

bool GpuProgram::IsSupported () const
{
	if (m_NotSupported)
		return false;
	
	// OpenGL specific caps check
	#if GFX_SUPPORTS_OPENGL
	if( GetGfxDevice().GetRenderer() == kGfxRendererOpenGL )
	{
		switch (m_ImplType) {
		case kShaderImplBoth:
			if( !gGraphicsCaps.gl.hasGLSL ) return false;
			break;
		default:
			break;
		}
	}
	#endif

	return true;
}

// --------------------------------------------------------------------------

CreateGpuProgramOutput::CreateGpuProgramOutput()
:	m_PerFogModeParamsEnabled(false)
,	m_Params(NULL)
,	m_ShaderErrors(NULL)
,	m_ChannelAssigns(NULL)
{
}

CreateGpuProgramOutput::~CreateGpuProgramOutput()
{
	delete m_Params;
	delete m_ShaderErrors;
	delete m_ChannelAssigns;
}

GpuProgramParameters& CreateGpuProgramOutput::CreateParams()
{
	Assert(!m_Params);
	m_Params = new GpuProgramParameters;
	return *m_Params;
}

ShaderErrors& CreateGpuProgramOutput::CreateShaderErrors()
{
	Assert(!m_ShaderErrors);
	m_ShaderErrors = new ShaderErrors;
	return *m_ShaderErrors;
}

ChannelAssigns& CreateGpuProgramOutput::CreateChannelAssigns()
{
	Assert(!m_ChannelAssigns);
	m_ChannelAssigns = new ChannelAssigns;
	return *m_ChannelAssigns;
}


#if GFX_SUPPORTS_OPENGL || GFX_SUPPORTS_OPENGLES20

GpuProgramGL::GpuProgramGL()
:	GpuProgram()
{
	for (int i = 0; i < kFogModeCount; ++i)
		m_Programs[i] = 0;
}

GpuProgramGL::~GpuProgramGL()
{
}

#endif
