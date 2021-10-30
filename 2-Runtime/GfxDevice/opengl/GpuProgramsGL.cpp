#include "UnityPrefix.h"
#include "GpuProgramsGL.h"
#include "External/shaderlab/Library/ShaderLabErrors.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Math/Vector4.h"
#include "UnityGL.h"
#include "GLAssert.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/shaderlab/Library/program.h"
#include "Runtime/Utilities/GLSLUtilities.h"


#define DEBUG_GLSL_BINDINGS 0


unsigned int GetGLShaderImplTarget( ShaderImplType implType ); // GfxDeviceGL.cpp
void InvalidateActiveShaderStateGL( ShaderType type ); // GfxDeviceGL.cpp

void InvalidateFPParamCacheGL();


// --------------------------------------------------------------------------
//  GLSL

// AttributeConversionTable
const static int kAttribLookupTableSize = 12;
const static char* s_GLSLAttributes[kAttribLookupTableSize] = {
	"gl_Vertex", "gl_Color", "gl_Normal",
	"gl__unused__", // unused
	"gl_MultiTexCoord0", "gl_MultiTexCoord1", "gl_MultiTexCoord2", "gl_MultiTexCoord3",
	"gl_MultiTexCoord4", "gl_MultiTexCoord5", "gl_MultiTexCoord6", "gl_MultiTexCoord7"
};
const static char* s_UnityAttributes[kAttribLookupTableSize] = {
	"Vertex", "Color", "Normal",
	"", // unused
	"TexCoord", "TexCoord1", "TexCoord2", "TexCoord3", // TODO: all the rest texcoord channels!
	"TexCoord4", "TexCoord5", "TexCoord6", "TexCoord7"
};

GlslGpuProgram::GlslGpuProgram( const std::string& source, CreateGpuProgramOutput& output )
{
	m_ImplType = kShaderImplBoth;
	output.SetPerFogModeParamsEnabled(true);
	m_GpuProgramLevel = kGpuProgramSM2;
	for (int i = 0; i < kFogModeCount; ++i)
	{
		m_GLSLVertexShader[i] = 0;
		m_GLSLFragmentShader[i] = 0;
		m_FogFailed[i] = false;
	}
	// Fragment shaders come out as dummy GLSL text. Just ignore them; the real shader was part of
	// the vertex shader text anyway.
	if (source.empty())
		return;
	if (Create (source, output.CreateShaderErrors()))
	{
		GpuProgramParameters& params = output.CreateParams();
		FillParams (m_Programs[kFogDisabled], params, NULL);
		FillChannels (output.CreateChannelAssigns());
//		GpuProgramParameters& params = parent.GetParams (kFogDisabled);
//		FillParams (m_Programs[kFogDisabled], params, outNames);
//		FillChannels (parent.GetChannels());
		if (params.GetTextureParams().size() > gGraphicsCaps.maxTexImageUnits)
			m_NotSupported = true;
	}
	else
	{
		m_NotSupported = true;
	}
}

GlslGpuProgram::~GlslGpuProgram ()
{
	Assert (m_ImplType == kShaderImplBoth);

	for (int i = 0; i < kFogModeCount; ++i)
	{
		if (m_GLSLVertexShader[i]) glDeleteObjectARB (m_GLSLVertexShader[i]);
		if (m_GLSLFragmentShader[i]) glDeleteObjectARB (m_GLSLFragmentShader[i]);
		if (m_Programs[i]) glDeleteObjectARB (m_Programs[i]);
	}
}




static bool ParseGLSLErrors (GLuint handle , GLSLErrorType errorType, ShaderErrors& outErrors)
{
	bool hadErrors = false;

	int status;
	char log[4096];
	GLsizei infoLogLength = 0;
	switch(errorType)
	{
	case kErrorCompileVertexShader:
	case kErrorCompileFragShader:
		glGetObjectParameterivARB(handle, GL_OBJECT_COMPILE_STATUS_ARB, &status);
		hadErrors = status == 0;
		glGetInfoLogARB(handle, sizeof(log), &infoLogLength, log);
		break;
	case kErrorLinkProgram:
		glGetObjectParameterivARB(handle, GL_OBJECT_LINK_STATUS_ARB, &status);
		hadErrors = status == 0;
		glGetInfoLogARB(handle, sizeof(log), &infoLogLength, log);
		break;
	default:
		FatalErrorMsg("Unknown error type");
		break;
	}

	if(!hadErrors)return false;

	OutputGLSLShaderError(log, errorType, outErrors);

	return hadErrors;
}


static void AddSizedVectorParam (GpuProgramParameters& params, ShaderParamType type, int vectorSize, int uniformNumber, int arraySize, const char* name, char* indexName, PropertyNamesSet* outNames)
{
	if (arraySize <= 1)
	{
		params.AddVectorParam (uniformNumber, type, vectorSize, name, -1, outNames);
	}
	else
	{
		for (int j = 0; j < arraySize; ++j) {
			if (j < 10) {
				indexName[0] = '0' + j;
				indexName[1] = 0;
			} else {
				indexName[0] = '0' + j/10;
				indexName[1] = '0' + j%10;
				indexName[2] = 0;
			}
			params.AddVectorParam (uniformNumber+j, type, vectorSize, name, -1, outNames);
		}
	}
}

static void AddSizedMatrixParam (GpuProgramParameters& params, int rows, int cols, int uniformNumber, int arraySize, const char* name, char* indexName, PropertyNamesSet* outNames)
{
	if (arraySize <= 1)
	{
		params.AddMatrixParam (uniformNumber, name, rows, cols, -1, outNames);
	}
	else
	{
		for (int j = 0; j < arraySize; ++j) {
			if (j < 10) {
				indexName[0] = '0' + j;
				indexName[1] = 0;
			} else {
				indexName[0] = '0' + j/10;
				indexName[1] = '0' + j%10;
				indexName[2] = 0;
			}
			params.AddMatrixParam (uniformNumber+j, name, rows, cols, -1, outNames);
		}
	}
}

static GLShaderID CompileVertexShader	(const std::string& source, ShaderErrors& outErrors)
{
	const char* text = source.c_str();
	GLShaderID vertexId = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	glShaderSourceARB    (vertexId, 1, &text, NULL);
	glCompileShaderARB   (vertexId);

	if (ParseGLSLErrors( vertexId, kErrorCompileVertexShader, outErrors))
	{
		glDeleteObjectARB(vertexId);
		return 0;
	}

	return vertexId;
}

static GLShaderID CompileFragmentShader (const std::string& source, ShaderErrors& outErrors)
{
	const char* text = source.c_str();
	GLShaderID fragId = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB );
	glShaderSourceARB    (fragId, 1, &text, NULL);
	glCompileShaderARB   (fragId);

	if (ParseGLSLErrors( fragId, kErrorCompileFragShader, outErrors))
	{
		glDeleteObjectARB(fragId);
		return 0;
	}

	return fragId;
}



bool GlslGpuProgram::Create (const std::string &shaderString, ShaderErrors& outErrors)
{
	if( !gGraphicsCaps.gl.hasGLSL )
		return false;

	GLAssert(); // Clear any GL errors

	m_Programs[0] = glCreateProgramObjectARB();

	m_ImplType = kShaderImplBoth;
	m_GpuProgramLevel = kGpuProgramSM2;

	std::string vertexShaderSource;
	std::string	fragmentShaderSource;


	vertexShaderSource = "#define VERTEX\n" + shaderString;
	fragmentShaderSource = "#define FRAGMENT\n" + shaderString;

	if( (m_GLSLVertexShader[0] = CompileVertexShader(vertexShaderSource, outErrors)) == 0 )
	{
		return false;
	}

	if( (m_GLSLFragmentShader[0] = CompileFragmentShader(fragmentShaderSource, outErrors)) == 0 )
	{
		return false;
	}

	glAttachObjectARB(m_Programs[0], m_GLSLVertexShader[0]);
	glAttachObjectARB(m_Programs[0], m_GLSLFragmentShader[0]);
	glLinkProgramARB(m_Programs[0]);

	if (ParseGLSLErrors (m_Programs[0], kErrorLinkProgram, outErrors))
	{
		return false;
	}

	m_SourceForFog = shaderString;

	GLAssert();
	return true;
}


void GlslGpuProgram::FillChannels (ChannelAssigns& channels)
{
	if (!m_Programs[0])
		return;

	// Figure out the attributes (vertex inputs)
	const int kBufSize = 1024;
	char name[kBufSize];

	GLAssert();

	GLenum type;
	int arraySize=0, nameLength=0, activeCount=0;
	glGetObjectParameterivARB(m_Programs[0], GL_OBJECT_ACTIVE_ATTRIBUTES_ARB, &activeCount);

	for(int i = 0; i < activeCount; i++)
	{
		glGetActiveAttribARB(m_Programs[0], i, kBufSize, &nameLength, &arraySize, &type, name);
		int attribNumber = 0;
		std::string stringName( name );
		// Find number for gl_ attribs in conversion table
		if (!strncmp (name, "gl_", 3)) {
			for(int j = 0; j < kAttribLookupTableSize; j++)
			{
				if(!strncmp(s_GLSLAttributes[j], stringName.c_str(), strlen(s_GLSLAttributes[j])))
				{
					attribNumber = j+1;
					stringName = s_UnityAttributes[j];
					break;
				}
			}
			if( attribNumber == 0 )
				ErrorString( "Unrecognized vertex attribute: " + stringName ); // TODO: SL error
		}
		else
		{
			// Check in names for non gl_ attribs. if not gl_ name get number:
			attribNumber = glGetAttribLocationARB(m_Programs[0], name) + kVertexCompAttrib0;
		}

		ShaderChannel shaderChannel = GetShaderChannelFromName(stringName);
		if( shaderChannel != kShaderChannelNone )
		{
			channels.Bind (shaderChannel, (VertexComponent)attribNumber);
		}
	}

	GLAssert();
}

void GlslGpuProgram::FillParams (GLShaderID programID, GpuProgramParameters& params, PropertyNamesSet* outNames)
{
	if (!programID)
		return;

	// Figure out the attributes (vertex inputs)
	const int kBufSize = 1024;
	char name[kBufSize];

	GLAssert();

	GLenum type;
	int arraySize=0, nameLength=0, activeCount=0;
	
	// Figure out the uniforms
	glGetObjectParameterivARB (programID, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &activeCount);
	for(int i=0; i < activeCount; i++)
	{
		glGetActiveUniformARB (programID, i, kBufSize, &nameLength, &arraySize, &type, name);
		if (!strncmp (name, "gl_", 3))
			continue;

		if (!strcmp (name, "_unity_FogParams") || !strcmp(name, "_unity_FogColor"))
			continue;

		int uniformNumber = glGetUniformLocationARB (programID, name);
		char* indexName = name;

		bool isElemZero = false;
		bool isArray	= IsShaderParameterArray(name, nameLength, arraySize, &isElemZero);
		if (isArray)
		{
			// for array parameters, transform name a bit: Foo[0] becomes Foo0
			if (arraySize >= 100) {
				ErrorString( "GLSL: array sizes larger than 99 not supported" ); // TODO: SL error
				arraySize = 99;
			}
			if (isElemZero)
			{
				indexName = name+nameLength-3;
				indexName[0] = '0';
				indexName[1] = 0;
			}
			else
			{
				indexName = name+nameLength;
			}
		}

		if (type == GL_FLOAT)
			AddSizedVectorParam (params, kShaderParamFloat, 1, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_FLOAT_VEC2_ARB)
			AddSizedVectorParam (params, kShaderParamFloat, 2, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_FLOAT_VEC3_ARB)
			AddSizedVectorParam (params, kShaderParamFloat, 3, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_FLOAT_VEC4_ARB)
			AddSizedVectorParam (params, kShaderParamFloat, 4, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_INT)
			AddSizedVectorParam (params, kShaderParamInt, 1, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_INT_VEC2_ARB)
			AddSizedVectorParam (params, kShaderParamInt, 2, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_INT_VEC3_ARB)
			AddSizedVectorParam (params, kShaderParamInt, 3, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_INT_VEC4_ARB)
			AddSizedVectorParam (params, kShaderParamInt, 4, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_BOOL)
			AddSizedVectorParam (params, kShaderParamBool, 1, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_BOOL_VEC2_ARB)
			AddSizedVectorParam (params, kShaderParamBool, 2, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_BOOL_VEC3_ARB)
			AddSizedVectorParam (params, kShaderParamBool, 3, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_BOOL_VEC4_ARB)
			AddSizedVectorParam (params, kShaderParamBool, 4, uniformNumber, arraySize, name, indexName, outNames);
		else if (type == GL_FLOAT_MAT4_ARB)
			AddSizedMatrixParam (params, 4, 4, uniformNumber, arraySize, name, indexName, outNames);
		// For all the textures, we could just statically bind the uniform to the sequential texture index,
		// and save all the glUniform1i calls later. However one gfx test on Linux is failing with the change,
		// and I can't figure out why. So instead I'll pack the uniform number & the index :(
		else if (type == GL_SAMPLER_2D_ARB || type == GL_SAMPLER_2D_RECT_ARB || type == GL_SAMPLER_2D_SHADOW_ARB)
		{
			const UInt32 texIndex = params.GetTextureParams().size();
			const UInt32 packedIndex = (texIndex << 24) | (uniformNumber & 0xFFFFFF);
			params.AddTextureParam (packedIndex, -1, name, kTexDim2D, outNames);
		}
		else if (type == GL_SAMPLER_CUBE_ARB)
		{
			const UInt32 texIndex = params.GetTextureParams().size();
			const UInt32 packedIndex = (texIndex << 24) | (uniformNumber & 0xFFFFFF);
			params.AddTextureParam (packedIndex, -1, name, kTexDimCUBE, outNames);
		}
		else if (type == GL_SAMPLER_3D_ARB)
		{
			const UInt32 texIndex = params.GetTextureParams().size();
			const UInt32 packedIndex = (texIndex << 24) | (uniformNumber & 0xFFFFFF);
			params.AddTextureParam (packedIndex, -1, name, kTexDim3D, outNames);
		}
		else {
			AssertString( "Unrecognized GLSL uniform type" );
		}
	}

	GLAssert();
}


void GlslGpuProgram::ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer)
{
	#if DEBUG_GLSL_BINDINGS
	printf_console("GLSL: apply program id=%i\n", m_ShaderID);
	#endif

	// Fog parameters if needed
	const GfxFogParams& fog = GetRealGfxDevice().GetFogParams();
	if (fog.mode > kFogDisabled && m_Programs[fog.mode])
	{
		OGL_CALL(glUniform4fvARB (m_FogColorIndex[fog.mode], 1, fog.color.GetPtr()));
		float params[4];
		params[0] = fog.density * 1.2011224087f ; // density / sqrt(ln(2))
		params[1] = fog.density * 1.4426950408f; // density / ln(2)
		if (fog.mode == kFogLinear)
		{
			float diff = fog.end - fog.start;
			float invDiff = Abs(diff) > 0.0001f ? 1.0f/diff : 0.0f;
			params[2] = -invDiff;
			params[3] = fog.end * invDiff;
		}
		else
		{
			params[2] = 0.0f;
			params[3] = 0.0f;
		}
		OGL_CALL(glUniform4fvARB (m_FogParamsIndex[fog.mode], 1, params));
	}

	const GpuProgramParameters::ValueParameterArray& valueParams = params.GetValueParams();
	GpuProgramParameters::ValueParameterArray::const_iterator valueParamsEnd = valueParams.end();
	for( GpuProgramParameters::ValueParameterArray::const_iterator i = valueParams.begin(); i != valueParamsEnd;  ++i )
	{
		if(i->m_RowCount == 1 && i->m_ArraySize == 1)
		{
			// Apply float/vector parameters
			const Vector4f& val = *reinterpret_cast<const Vector4f*>(buffer);
			if (i->m_Type == kShaderParamFloat)
			{
				if (i->m_ColCount == 1)
					OGL_CALL(glUniform1fvARB (i->m_Index, 1, val.GetPtr()));
				else if(i->m_ColCount == 2)
					OGL_CALL(glUniform2fvARB (i->m_Index, 1, val.GetPtr()));
				else if(i->m_ColCount == 3)
					OGL_CALL(glUniform3fvARB (i->m_Index, 1, val.GetPtr()));
				else if(i->m_ColCount == 4)
					OGL_CALL(glUniform4fvARB (i->m_Index, 1, val.GetPtr()));
			}
			else
			{
				// In theory Uniform*f can also be used to load bool uniforms, in practice
				// some drivers don't like that. So load both integers and bools via *i functions.
				int ival[4] = {val.x, val.y, val.z, val.w};
				if (i->m_ColCount == 1)
					OGL_CALL(glUniform1ivARB (i->m_Index, 1, ival));
				else if(i->m_ColCount == 2)
					OGL_CALL(glUniform2ivARB (i->m_Index, 1, ival));
				else if(i->m_ColCount == 3)
					OGL_CALL(glUniform3ivARB (i->m_Index, 1, ival));
				else if(i->m_ColCount == 4)
					OGL_CALL(glUniform4ivARB (i->m_Index, 1, ival));
			}
			#if DEBUG_GLSL_BINDINGS
			printf_console("  vector %i dim=%i\n", i->m_Index, i->m_Dim );
			#endif
			buffer += sizeof(Vector4f);
		}
		else
		{
			// Apply matrix parameters
			DebugAssert(i->m_RowCount == 4);
			int size = *reinterpret_cast<const int*> (buffer); buffer += sizeof(int);
			DebugAssert (size == 16);
			const Matrix4x4f* mat = reinterpret_cast<const Matrix4x4f*>(buffer);
			const float *ptr = mat->GetPtr ();
			OGL_CALL(glUniformMatrix4fvARB (i->m_Index, 1, false, ptr));
			#if DEBUG_GLSL_BINDINGS
			printf_console("  matrix %i (%s)\n", i->m_Index, i->m_Name.GetName() );
			#endif
			buffer += size * sizeof(float);
		}
	}

	// Apply textures
	const GpuProgramParameters::TextureParameterList& textureParams = params.GetTextureParams();
	GpuProgramParameters::TextureParameterList::const_iterator textureParamsEnd = textureParams.end();
	for( GpuProgramParameters::TextureParameterList::const_iterator i = textureParams.begin(); i != textureParamsEnd; ++i )
	{
		const GpuProgramParameters::TextureParameter& t = *i;
		const TexEnvData* texdata = reinterpret_cast<const TexEnvData*>(buffer);
		int texIndex = int(UInt32(t.m_Index) >> 24);
		Assert(texIndex < gGraphicsCaps.maxTexImageUnits);
		UInt32 uniformIndex = t.m_Index & 0xFFFFFF;
		OGL_CALL(glUniform1ivARB( uniformIndex, 1, &texIndex ));
		#if DEBUG_GLSL_BINDINGS
		printf_console("  sampler %i to unit %i (%s)\n", uniformIndex, texIndex, t.m_Name.GetName() );
		#endif
		ApplyTexEnvData (texIndex, texIndex, *texdata);
		buffer += sizeof(*texdata);
	}

	GLAssert();
}


GLShaderID GlslGpuProgram::GetGLProgram (FogMode fog, GpuProgramParameters& outParams)
{
	int index = 0;
	if (fog > kFogDisabled && !m_SourceForFog.empty() && !m_FogFailed[fog])
	{
		index = fog;
		Assert (index >= 0 && index < kFogModeCount);

		// create patched fog program if needed
		if (!m_Programs[index])
		{
			std::string src = m_SourceForFog;
			if (PatchShaderFogGLSL (src, fog))
			{
				// create program, fragment shader, link
				m_Programs[index] = glCreateProgramObjectARB();
				ShaderErrors errors;
				if(
					(m_GLSLVertexShader[index] = CompileVertexShader("#define VERTEX\n" + src, errors)) != 0 &&
					(m_GLSLFragmentShader[index] = CompileFragmentShader("#define FRAGMENT\n" + src, errors)) != 0 )
				{
					glAttachObjectARB(m_Programs[index], m_GLSLVertexShader[index]);
					glAttachObjectARB(m_Programs[index], m_GLSLFragmentShader[index]);
					glLinkProgramARB(m_Programs[index]);
					if (!ParseGLSLErrors (m_Programs[index], kErrorLinkProgram, errors))
					{
						FillParams (m_Programs[index], outParams, NULL);
						m_FogParamsIndex[index] = glGetUniformLocationARB(m_Programs[index], "_unity_FogParams");
						m_FogColorIndex[index] = glGetUniformLocationARB(m_Programs[index], "_unity_FogColor");
					}
					else
					{
						glDeleteObjectARB(m_GLSLVertexShader[index]); m_GLSLVertexShader[index] = 0;
						glDeleteObjectARB(m_GLSLFragmentShader[index]); m_GLSLFragmentShader[index] = 0;
						glDeleteObjectARB(m_Programs[index]); m_Programs[index] = 0;
						m_FogFailed[index] = true;
						index = 0;
					}
				}
				else
				{
					glDeleteObjectARB(m_GLSLVertexShader[index]); m_GLSLVertexShader[index] = 0;
					glDeleteObjectARB(m_GLSLFragmentShader[index]); m_GLSLFragmentShader[index] = 0;
					glDeleteObjectARB(m_Programs[index]); m_Programs[index] = 0;
					m_FogFailed[index] = true;
					index = 0;
				}
			}
			else
			{
				m_FogFailed[index] = true;
				index = 0;
			}
		}
	}
	return m_Programs[index];
}
