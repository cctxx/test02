#include "UnityPrefix.h"
#if GFX_SUPPORTS_OPENGLES30
#include "GpuProgramsGLES30.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/File/ApplicationSpecificPersistentDataPath.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "External/shaderlab/Library/ShaderLabErrors.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/shaderlab/Library/program.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Matrix3x3.h"
#include "Runtime/Math/Vector4.h"


#include "IncludesGLES30.h"
#include "AssertGLES30.h"
#include "GpuPropertiesGLES30.h"
#include "Runtime/Utilities/GLSLUtilities.h"
#include "VBOGLES30.h"
#include "ConstantBuffersGLES30.h"
#include "DebugGLES30.h"

#if UNITY_ANDROID
    #include "PlatformDependent/AndroidPlayer/EntryPoint.h"
    #include "PlatformDependent/AndroidPlayer/AndroidSystemInfo.h"
#endif

#if UNITY_BLACKBERRY
	#include "ctype.h"
#endif


#include <stdio.h>

#define DEBUG_GLSL_BINDINGS 0
#define DEBUG_SHADER_CACHE  0


// from shader_yacc.hpp
extern std::string g_LastParsedShaderName;

void GLSLUseProgramGLES30 (UInt32 programID); // defined in GfxDeviceGLES30.cpp

static bool CompileGLSLVertexShader (const std::string& source, ChannelAssigns& channels, GLShaderID programID, GLShaderID parentProgramID, GLShaderID* outShaderID);
static bool CompileGLSLFragmentShader (const std::string& source, GLShaderID* outShaderID);
static bool BindVProgAttrbutes(const std::string& source, ChannelAssigns& channels, GLShaderID programID);
static bool RebindVProgAttrbutes(GLShaderID programID, GLShaderID parentProgramID);


static void GetCachedBinaryName(const std::string& vprog, const std::string& fshader, char filename[33]);


// --------------------------------------------------------------------------
//  GLSL

// AttributeConversionTable
const static UInt32 kAttribLookupTableSize = 12;

const static char* s_GLSLESAttributes[kAttribLookupTableSize] = {
	"_glesVertex", "_glesColor", "_glesNormal",
	"_gles_unused__", // unused
	"_glesMultiTexCoord0", "_glesMultiTexCoord1", "_glesMultiTexCoord2", "_glesMultiTexCoord3",
	"_glesMultiTexCoord4", "_glesMultiTexCoord5", "_glesMultiTexCoord6", "_glesMultiTexCoord7"
};
const static char* s_UnityAttributes[kAttribLookupTableSize] = {
	"Vertex", "Color", "Normal",
	"", // unused
	"TexCoord", "TexCoord1", "TexCoord2", "TexCoord3",
	"TexCoord4", "TexCoord5", "TexCoord6", "TexCoord7"
};


const VertexComponent s_UnityVertexComponents[kAttribLookupTableSize] = {
	kVertexCompVertex,
	kVertexCompColor,
	kVertexCompNormal,
	kVertexCompTexCoord,
	kVertexCompTexCoord0, kVertexCompTexCoord1, kVertexCompTexCoord2, kVertexCompTexCoord3,
	kVertexCompTexCoord4, kVertexCompTexCoord5, kVertexCompTexCoord6, kVertexCompTexCoord7
};

const int s_GLESVertexComponents[kAttribLookupTableSize] = {
	kGLES3AttribLocationPosition,
	kGLES3AttribLocationColor,
	kGLES3AttribLocationNormal,
	-1 /*kVertexCompTexCoord*/,
	kGLES3AttribLocationTexCoord0,
	kGLES3AttribLocationTexCoord1,
	kGLES3AttribLocationTexCoord2,
	kGLES3AttribLocationTexCoord3,
	kGLES3AttribLocationTexCoord4,
	kGLES3AttribLocationTexCoord5,
	kGLES3AttribLocationTexCoord6,
	kGLES3AttribLocationTexCoord7,
};

GlslGpuProgramGLES30::GlslGpuProgramGLES30 (const std::string& source, CreateGpuProgramOutput& output)
{
	output.SetPerFogModeParamsEnabled(true);
	m_ImplType = kShaderImplBoth;
	for (int i = 0; i < kFogModeCount; ++i)
	{
		m_GLSLVertexShader[i] = 0;
		m_GLSLFragmentShader[i] = 0;
		m_FogColorIndex[i] = -1;
		m_FogParamsIndex[i] = -1;
		m_FogFailed[i] = false;
	}

	// Fragment shaders come out as dummy GLSL text. Just ignore them; the real shader was part of
	// the vertex shader text anyway.
	if (source.empty())
		return;

	if (Create (source, output.CreateChannelAssigns()))
	{
		GpuProgramParameters& params = output.CreateParams ();
		FillParams (m_Programs[kFogDisabled], params, output.GetOutNames());
		if (params.GetTextureParams().size() > gGraphicsCaps.maxTexImageUnits)
			m_NotSupported = true;

		m_UniformCache[kFogDisabled].Create(&params, -1, -1);
	}
	else
	{
		m_NotSupported = true;
	}
}

GlslGpuProgramGLES30::~GlslGpuProgramGLES30 ()
{
	Assert (m_ImplType == kShaderImplBoth);
	for (int i = 0; i < kFogModeCount; ++i)
	{
		if (m_GLSLVertexShader[i]) { GLES_CHK(glDeleteShader(m_GLSLVertexShader[i])); }
		if (m_GLSLFragmentShader[i]) { GLES_CHK(glDeleteShader(m_GLSLFragmentShader[i])); }
		if (m_Programs[i]) { GLES_CHK(glDeleteProgram(m_Programs[i])); }
		m_UniformCache[i].Destroy();
	}
}

static bool ParseGlslErrors (GLuint type , GLSLErrorType errorType, const char* source = 0)
{
	bool hadErrors = false;
	char compileInfoLog[4096];
	GLsizei infoLogLength = 0;
	switch(errorType)
	{
	case kErrorCompileVertexShader:
	case kErrorCompileFragShader:
		glGetShaderInfoLog(type, 4096, &infoLogLength, compileInfoLog );
		break;
	case kErrorLinkProgram:
		glGetProgramInfoLog(type, 4096, &infoLogLength, compileInfoLog );
		break;
	default:
		FatalErrorMsg("Unknown error type");
		break;
	}

	// Make sure it is null-terminated
	compileInfoLog[std::min<int>(infoLogLength, 4096)] = 0;

	if (strlen (compileInfoLog) > 0)
	{
		hadErrors = true;
		if (source)
		{
			printf_console("-------- GLSL source: \n");
			DebugTextLineByLine(source);
		}

		printf_console("-------- GLSL error:\n%s\n\n", compileInfoLog);
	}

	return hadErrors;
}

static std::string ExtractDefineBock(const std::string& defineName, const std::string& str, std::string* remainderStr)
{
	const std::string beginsWith = "#ifdef " + defineName;
	const std::string endsWith = "#endif";

	size_t b;
	if ((b = str.find(beginsWith))==std::string::npos)
		return "";

	b += beginsWith.size();

	size_t e = b;
	size_t n = 1;
	do
	{
		size_t nextEnd = str.find(endsWith, e);
		size_t nextIf = str.find("#if", e);

		if (nextEnd == std::string::npos)
			return "";

		if (nextIf != std::string::npos && nextIf < nextEnd)
		{
			++n;
			e = nextIf + 1;
		}
		else
		{
			--n;
			e = nextEnd + 1;
		}
	}
	while (n > 0);

	std::string retVal = str.substr(b, e-b-1);
	if (remainderStr)
	{
		*remainderStr = str.substr(0, b - beginsWith.size());
		if (e + endsWith.size() < str.length()) *remainderStr += str.substr(e + endsWith.size());
	}

	return retVal;
}

static void FindProgramStart(const char* source, std::string* header, std::string* prog)
{
	const char* line_start = source;
	while(*line_start)
	{
		while(isspace(*line_start))
			++line_start;
		if(*line_start == '#')
		{
			while(*line_start != '\n' && *line_start != '\r')
				++line_start;
		}
		else
		{
			header->assign(source, line_start - source);
			prog->assign(line_start);
			break;
		}
	}
}

bool GlslGpuProgramGLES30::CompileGlslShader(GLShaderID shader, const char* source)
{
	GLES_CHK(glShaderSource(shader, 1, &source, NULL));
	GLES_CHK(glCompileShader(shader));

	int	compiled = 10;
	GLES_CHK(glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled));

	if (compiled==0)
	{
		ParseGlslErrors(shader, kErrorCompileFragShader, source);
		return false;
	}

	return true;
}

bool GlslGpuProgramGLES30::Create (const std::string &shaderString, ChannelAssigns& channels)
{
	GLESAssert(); // Clear any GL errors

	GLES_CHK(m_Programs[0] = glCreateProgram());

	m_ImplType = kShaderImplBoth;

	// NOTE: pre-pending shader with VERTEX/FRAGMENT defines doesn't work with ES/GLSL compilers for some reason
	// therefore we extract VERTEX/FRAGMENT sections from the shaderString

	std::string remainder = shaderString;
	std::string	vertexShaderSource = ExtractDefineBock("VERTEX", shaderString, &remainder);
	std::string	fragmentShaderSource = ExtractDefineBock("FRAGMENT", remainder, &remainder);

	vertexShaderSource = remainder + vertexShaderSource;
	fragmentShaderSource = remainder + fragmentShaderSource;

	if(!CompileProgram(0, vertexShaderSource, fragmentShaderSource, channels))
	{
		ParseGlslErrors( m_Programs[0], kErrorLinkProgram );

		int charsWritten=0;

		if(m_GLSLVertexShader[0])
		{
			int vertShaderLength = 0;
			GLES_CHK(glGetShaderiv(m_GLSLVertexShader[0], GL_SHADER_SOURCE_LENGTH, &vertShaderLength));
			char* modVertexShader = new char[vertShaderLength];
			GLES_CHK(glGetShaderSource(m_GLSLVertexShader[0], vertShaderLength, &charsWritten, modVertexShader));

		#if UNITY_WIN
			OutputDebugString(Format("Vertex Shader:\n%s\n", modVertexShader).c_str());
		#else
			::printf_console("Vertex Shader:\n");
			DebugTextLineByLine(modVertexShader);
		#endif

			delete[] modVertexShader;
		}

		if(m_GLSLFragmentShader[0])
		{
			int fragShaderLength = 0;
			GLES_CHK(glGetShaderiv(m_GLSLFragmentShader[0], GL_SHADER_SOURCE_LENGTH, &fragShaderLength));
			char* modFragmentShader = new char[fragShaderLength];
			GLES_CHK(glGetShaderSource(m_GLSLFragmentShader[0], fragShaderLength, &charsWritten, modFragmentShader));

		#if UNITY_WIN
			OutputDebugString(Format("Fragment Shader:\n%s\n", modFragmentShader).c_str());
		#else
			::printf_console("Vertex Shader:\n");
			DebugTextLineByLine(modFragmentShader);
		#endif

			delete[] modFragmentShader;
		}

		// TODO: cleanup
		return false;
	}

	m_VertexShaderSourceForFog = vertexShaderSource;
	m_SourceForFog = fragmentShaderSource;

	return true;
}

// \todo [2013-04-17 pyry] Uh...
std::string GlslGpuProgramGLES30::_CachePath;

bool GlslGpuProgramGLES30::InitBinaryShadersSupport()
{
	if (gGraphicsCaps.gles30.useProgramBinary)
	{
		int binFormatCount = 0;
		GLES_CHK(glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS_OES, &binFormatCount));

		if(binFormatCount > 0)
		{
			_CachePath = GetPersistentDataPathApplicationSpecific() + "/shader/";
			if(!IsDirectoryCreated(_CachePath))
				CreateDirectory(_CachePath);

			return true;
		}
	}

	return false;
}

static void Internal_ClearShaderCache()
{
	std::string shaderCache = GetPersistentDataPathApplicationSpecific() + "/shader/";
	DeleteFileOrDirectory(shaderCache);
	CreateDirectory(shaderCache);
}

bool GlslGpuProgramGLES30::CompileProgram(unsigned index, const std::string& vprog, const std::string& fshader, ChannelAssigns& channels)
{
	std::string binaryPath;

	if (gGraphicsCaps.gles30.useProgramBinary)
	{
		char filename[33] = {0};
		GetCachedBinaryName(vprog, fshader, filename);

		binaryPath = _CachePath + filename;
	#if DEBUG_SHADER_CACHE
		::printf_console("Starting compilation \"%s\" variation with md5:%s\n", g_LastParsedShaderName.c_str(), filename);
	#endif
	}

	// first 4 bytes are for binary format
	char*		binaryData		= 0;
	char*		binaryProgram	= 0;
	unsigned	binaryLength	= 0;

	bool loadedBinary = false;
	if (gGraphicsCaps.gles30.useProgramBinary)
	{
		FILE* binaryFile = ::fopen(binaryPath.c_str(), "rb");
		if(binaryFile)
		{
			::fseek(binaryFile, 0, SEEK_END);
			unsigned datasz = (unsigned)::ftell(binaryFile);
			::fseek(binaryFile, 0, SEEK_SET);

			binaryData		= (char*)::malloc(datasz);
			binaryProgram	= binaryData + sizeof(GLenum);
			binaryLength	= datasz - sizeof(GLenum);
			::fread(binaryData, datasz, 1, binaryFile);
			::fclose(binaryFile);

			loadedBinary = true;
		#if DEBUG_SHADER_CACHE
			::printf_console("Loaded from cache");
		#endif
		}
	}

	if(loadedBinary)
	{
		loadedBinary = false;

		bool attrBound = index==0	? BindVProgAttrbutes(vprog, channels, m_Programs[0])
									: RebindVProgAttrbutes(m_Programs[index], m_Programs[0]);
		if(attrBound)
		{
			GLES_CHK(glProgramBinary(m_Programs[index], *((GLenum*)binaryData), binaryProgram, binaryLength));

			int linked = 0;
			GLES_CHK(glGetProgramiv(m_Programs[index], GL_LINK_STATUS, &linked));
			loadedBinary = linked != 0;
		}

		if(!loadedBinary)
		{
		#if DEBUG_SHADER_CACHE
			::printf_console("Bad cached version\n");
		#endif

			::free(binaryData);
			binaryProgram = binaryData = 0;
		}
	}

	// fallback to compiling shaders at runtime
	if(!loadedBinary)
	{
		DBG_SHADER_VERBOSE_GLES30("Compiling shader: %s\n", g_LastParsedShaderName.c_str());
	#if DEBUG_SHADER_CACHE
		::printf_console("Actually compiling");
	#endif

		if(!CompileGLSLVertexShader(vprog, channels, m_Programs[index], m_Programs[0], &m_GLSLVertexShader[index]))
			return false;
		if(!CompileGLSLFragmentShader(fshader, &m_GLSLFragmentShader[index]))
			return false;

		GLES_CHK(glAttachShader(m_Programs[index], m_GLSLVertexShader[index]));
		GLES_CHK(glAttachShader(m_Programs[index], m_GLSLFragmentShader[index]));
		GLES_CHK(glLinkProgram(m_Programs[index]));

		int linked = 0;
		GLES_CHK(glGetProgramiv(m_Programs[index], GL_LINK_STATUS, &linked));
		if(linked == 0)
			return false;

		if (gGraphicsCaps.gles30.useProgramBinary)
		{
			Assert(binaryData == 0 && binaryProgram == 0);

			GLES_CHK(glGetProgramiv(m_Programs[index], GL_PROGRAM_BINARY_LENGTH_OES, (GLint*)&binaryLength));
			binaryData		= (char*)::malloc(binaryLength + sizeof(GLenum));
			binaryProgram	= binaryData + sizeof(GLenum);
			GLES_CHK(glGetProgramBinary(m_Programs[index], binaryLength, 0, (GLenum*)binaryData, binaryProgram));

			FILE* binaryFile = ::fopen(binaryPath.c_str(), "wb");
			if(binaryFile)
			{
				::fwrite(binaryData, binaryLength + sizeof(GLenum), 1, binaryFile);
				::fclose(binaryFile);

			#if DEBUG_SHADER_CACHE
				::printf_console("Saved to cache\n");
			#endif
			}
		}
	}

	if(binaryData)
		::free(binaryData);

	return true;
}



static void PrintNumber (char* s, int i, bool brackets)
{
	DebugAssert (i >= 0 && i < 100);
	if (brackets)
		*s++ = '[';

	if (i < 10) {
		*s++ = '0' + i;
	} else {
		*s++ = '0' + i/10;
		*s++ = '0' + i%10;
	}

	if (brackets)
		*s++ = ']';
	*s++ = 0;
}

static void AddSizedVectorParam (GpuProgramParameters& params, ShaderParamType type, GLuint program, int vectorSize, int uniformNumber, int arraySize, const char* unityName, char* glName, int glNameIndexOffset, PropertyNamesSet* outNames)
{
	if (arraySize <= 1)
	{
		params.AddVectorParam (uniformNumber, type, vectorSize, unityName, -1, outNames);
	}
	else
	{
		for (int j = 0; j < arraySize; ++j)
		{
			PrintNumber (glName+glNameIndexOffset, j, true);
			uniformNumber = glGetUniformLocation (program, glName);
			PrintNumber (glName+glNameIndexOffset, j, false);
			params.AddVectorParam (uniformNumber, type, vectorSize, glName, -1, outNames);
		}
	}
}

static void AddSizedMatrixParam (GpuProgramParameters& params, GLuint program, int rows, int cols, int uniformNumber, int arraySize, const char* unityName, char* glName, int glNameIndexOffset, PropertyNamesSet* outNames)
{
	if (arraySize <= 1)
	{
		params.AddMatrixParam (uniformNumber, unityName, rows, cols, -1, outNames);
	}
	else
	{
		for (int j = 0; j < arraySize; ++j)
		{
			PrintNumber (glName+glNameIndexOffset, j, true);
			uniformNumber = glGetUniformLocation (program, glName);
			PrintNumber (glName+glNameIndexOffset, j, false);
			params.AddMatrixParam (uniformNumber, glName, rows, cols, -1, outNames);
		}
	}
}


void GlslGpuProgramGLES30::FillParams (unsigned int programID, GpuProgramParameters& params, PropertyNamesSet* outNames)
{
	if (!programID)
		return;

	int activeUniforms;

	char name[1024];
	GLenum type;
	int arraySize = 0,
		nameLength = 0,
		bufSize = sizeof(name);

	DBG_LOG_GLES30("GLSL: apply params to program id=%i\n", programID);
	GLSLUseProgramGLES30 (programID);

	// Figure out the uniforms
	GLES_CHK(glGetProgramiv (programID, GL_ACTIVE_UNIFORMS, &activeUniforms));
	for(int i=0; i < activeUniforms; i++)
	{
		GLES_CHK(glGetActiveUniform (programID, i, bufSize, &nameLength, &arraySize, &type, name));

		if (!strcmp (name, "_unity_FogParams") || !strcmp(name, "_unity_FogColor"))
			continue;

		// some Unity builtin properties are mapped to GLSL structure fields
		// hijack them here
		const char* glslName = GetGLSLES3PropertyNameRemap(name);
		const char* unityName = glslName ? glslName : name;

		if (!strncmp (name, "gl_", 3)) // skip "gl_" names
			continue;

		int uniformNumber = glGetUniformLocation (programID, name);
		Assert(uniformNumber != -1);

		char* glName = name;
		int glNameIndexOffset = 0;

		bool isElemZero = false;
		bool isArray	= IsShaderParameterArray(name, nameLength, arraySize, &isElemZero);
		if (isArray)
		{
			// for array parameters, transform name a bit: Foo[0] becomes Foo0
			if (arraySize >= 100) {
				ErrorString( "GLSL: array sizes larger than 99 not supported" ); // TODO: SL error
				arraySize = 99;
			}
			// TODO: wrong? what if we use only array[1] for example
			if (isElemZero)
			{
				glNameIndexOffset = nameLength-3;
				if(glNameIndexOffset < 0)
					glNameIndexOffset = 0;
				glName[glNameIndexOffset] = '0';
				glName[glNameIndexOffset+1] = 0;
			}
			else
			{
				glNameIndexOffset = nameLength;
			}
		}

		if(type == GL_FLOAT) {
			AddSizedVectorParam (params, kShaderParamFloat, programID, 1, uniformNumber, arraySize, unityName, glName, glNameIndexOffset, outNames);
		}
		else if(type == GL_FLOAT_VEC2) {
			AddSizedVectorParam (params, kShaderParamFloat, programID, 2, uniformNumber, arraySize, unityName, glName, glNameIndexOffset, outNames);
		}
		else if(type == GL_FLOAT_VEC3) {
			AddSizedVectorParam (params, kShaderParamFloat, programID, 3, uniformNumber, arraySize, unityName, glName, glNameIndexOffset, outNames);
		}
		else if(type == GL_FLOAT_VEC4) {
			AddSizedVectorParam (params, kShaderParamFloat, programID, 4, uniformNumber, arraySize, unityName, glName, glNameIndexOffset, outNames);
		}
		else if(type == GL_FLOAT_MAT4) {
			AddSizedMatrixParam (params, programID, 4, 4, uniformNumber, arraySize, unityName, glName, glNameIndexOffset, outNames);
		}
		else if(type == GL_FLOAT_MAT3) {
			AddSizedMatrixParam (params, programID, 3, 3, uniformNumber, arraySize, unityName, glName, glNameIndexOffset, outNames);
		}

		else if (type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_SHADOW) {
			GLES_CHK(glUniform1i (uniformNumber, params.GetTextureParams().size()));
			params.AddTextureParam (uniformNumber, -1, unityName, kTexDim2D, outNames);
		}
		else if (type == GL_SAMPLER_CUBE || type == GL_SAMPLER_CUBE_SHADOW) {
			GLES_CHK(glUniform1i (uniformNumber, params.GetTextureParams().size()));
			params.AddTextureParam (uniformNumber, -1, unityName, kTexDimCUBE, outNames);
		}
		/*
		else if(type == GL_SAMPLER_3D) {
			GLES_CHK(glUniform1i (uniformNumber, params.GetTextureParams().size()));
			params.AddTextureParam( name, kTexDim3D, uniformNumber );
		}
		else if(type == GL_SAMPLER_2D_SHADOW) {
			GLES_CHK(glUniform1i (uniformNumber, params.GetTextureParams().size()));
			params.AddTextureParam( name, kTexDim2D, uniformNumber );
		}

		*/
		else {
			AssertString( "Unrecognized GLSL uniform type" );
		}
	}

	GLESAssert();
}

static bool RebindVProgAttrbutes(GLShaderID programID, GLShaderID parentProgramID)
{
	int attribCount = 0;
	GLES_CHK(glGetProgramiv(parentProgramID, GL_ACTIVE_ATTRIBUTES, &attribCount));

	const int kBufSize = 256;
	char name[kBufSize];
	for(int i = 0 ; i < attribCount ; ++i)
	{
		int nameLength = 0, arraySize = 0;
		GLenum type;
		GLES_CHK(glGetActiveAttrib (parentProgramID, i, kBufSize, &nameLength, &arraySize, &type, name));
		int location = glGetAttribLocation (parentProgramID, name);
		if (location != -1)
			GLES_CHK(glBindAttribLocation(programID, location, name));
	}

	return true;
}

static bool BindVProgAttrbutes(const std::string& source, ChannelAssigns& channels, GLShaderID programID)
{
	// Add necessary attribute tags
	for (UInt32 j = 0; j < kAttribLookupTableSize; j++)
	{
		if (source.find(s_GLSLESAttributes[j]) != std::string::npos)
		{
			if (s_GLESVertexComponents[j] >= gGraphicsCaps.gles30.maxAttributes)
			{
				ErrorString("Shader uses too many vertex attributes for this platform");
				return false;
			}
			GLES_CHK(glBindAttribLocation(programID, s_GLESVertexComponents[j], s_GLSLESAttributes[j]));
			ShaderChannel shaderChannel = GetShaderChannelFromName(s_UnityAttributes[j]);
			if( shaderChannel != kShaderChannelNone )
				channels.Bind (shaderChannel, s_UnityVertexComponents[j]);
		}
	}

	// UGLY HACK:
	// somewhere deep inside shader generation we just put attribute TANGENT
	// without ever using it after
	// look for TANGENT twice to be sure we use it
	size_t firstTangentUsage = source.find("TANGENT");
	if( firstTangentUsage != std::string::npos && source.find("TANGENT", firstTangentUsage+1) != std::string::npos )
	{
		// Find first free slot for tangents and use it.
		// Unity normally supports 2 UV slots (0&1), so start looking from
		// kVertexTexCoord2
		for (int i = kVertexCompTexCoord2; i < kVertexCompTexCoord7; i++)
		{
			if (channels.GetSourceForTarget((VertexComponent)i) == kShaderChannelNone)
			{
				channels.Bind (kShaderChannelTangent, (VertexComponent)i);
				GLES_CHK(glBindAttribLocation(programID, kGLES3AttribLocationTexCoord0 + i - kVertexCompTexCoord0, "_glesTANGENT"));
				break;
			}
		}
	}

	return true;
}

static bool CompileGLSLVertexShader (const std::string& source, ChannelAssigns& channels, GLShaderID programID, GLShaderID parentProgramID, GLShaderID* outShaderID)
{
	GLES_CHK(*outShaderID = glCreateShader(GL_VERTEX_SHADER));

	if(parentProgramID == programID)
		BindVProgAttrbutes(source, channels, programID);
	else
		RebindVProgAttrbutes(programID, parentProgramID);

	const char* text = source.c_str();
	DBG_SHADER_VERBOSE_GLES30_DUMP_SHADER("Compiling VERTEX program:", text);

	if (GlslGpuProgramGLES30::CompileGlslShader(*outShaderID, text))
		return true;

	printf_console ("GLES30: failed to compile vertex shader:\n%s\n", text);

	// TODO: cleanup
	return false;
}

static bool CompileGLSLFragmentShader (const std::string& source, GLShaderID* outShaderID)
{
	*outShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	///@TODO: find any existing precision statement

	std::string modSourceHeader, modSourceProg;
	FindProgramStart(source.c_str(), &modSourceHeader, &modSourceProg);

	// \todo [2013-04-17 pyry] Should we default to highp or mediump?
	std::string modSource =   modSourceHeader
							+ std::string("precision highp float;\n")
							+ modSourceProg;

	const char* text = modSource.c_str();
	DBG_SHADER_VERBOSE_GLES30_DUMP_SHADER("Compiling FRAGMENT program:", text);

	if (GlslGpuProgramGLES30::CompileGlslShader(*outShaderID, text))
		return true;

	printf_console ("GLES30: failed to compile fragment shader:\n%s\n", text);

	// TODO: cleanup
	return false;
}

int GlslGpuProgramGLES30::GetGLProgram (FogMode fog, GpuProgramParameters& outParams, ChannelAssigns &channels)
{
	int index = 0;
	if (fog > kFogDisabled && !m_FogFailed[fog] && !m_SourceForFog.empty())
	{
		index = fog;
		Assert (index >= 0 && index < kFogModeCount);

		// create patched fog program if needed
		if(!m_Programs[index])
		{
			std::string srcVS = m_VertexShaderSourceForFog;
			std::string srcPS = m_SourceForFog;

			if(PatchShaderFogGLES (srcVS, srcPS, fog, CanUseOptimizedFogCodeGLES(srcVS)))
			{
				// create program, shaders, link
				GLES_CHK(m_Programs[index] = glCreateProgram());
				ShaderErrors errors;
				if(CompileProgram(index, srcVS, srcPS, channels))
				{
					FillParams (m_Programs[index], outParams, NULL);
					m_FogParamsIndex[index] = glGetUniformLocation(m_Programs[index], "_unity_FogParams");
					m_FogColorIndex[index] = glGetUniformLocation(m_Programs[index], "_unity_FogColor");

					m_UniformCache[index].Create(&outParams, m_FogParamsIndex[index], m_FogColorIndex[index]);
				}
				else
				{
					if(m_GLSLVertexShader[index])
						glDeleteShader(m_GLSLVertexShader[index]);

					if(m_GLSLFragmentShader[index])
						glDeleteShader(m_GLSLFragmentShader[index]);

					m_GLSLVertexShader[index] = 0;
					m_GLSLFragmentShader[index] = 0;

					glDeleteProgram(m_Programs[index]);
					m_Programs[index] = 0;

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
	return index;
}

int GlslGpuProgramGLES30::ApplyGpuProgramES30 (const GpuProgramParameters& params, const UInt8 *buffer)
{
	DBG_LOG_GLES30("GlslGpuProgramGLES30::ApplyGpuProgramES30()");

	// m_Programs[0] == 0, is when Unity tries to build a dummy shader with empty source for fragment shaders, do nothing in this case
	if (m_Programs[0] == 0)
		return 0;

	GfxDevice& device = GetRealGfxDevice();

	const GfxFogParams& fog = device.GetFogParams();
	const int index = (int)fog.mode;

	DBG_LOG_GLES30("GLSL: apply program id=%i\n", m_Programs[index]);
	GLSLUseProgramGLES30 (m_Programs[index]);

	// Apply value parameters
	const GpuProgramParameters::ValueParameterArray& valueParams = params.GetValueParams();
	GpuProgramParameters::ValueParameterArray::const_iterator valueParamsEnd = valueParams.end();
	for( GpuProgramParameters::ValueParameterArray::const_iterator i = valueParams.begin(); i != valueParamsEnd;  ++i )
	{
		if (i->m_RowCount == 1 && i->m_ArraySize == 1)
		{
			UniformCacheGLES30*	cache	= m_UniformCache+index;
//			const float*		val 	= i->m_ColCount == 1 	? &ShaderLab::shaderprops::GetFloat(props, i->m_Name)
//																: ShaderLab::shaderprops::GetVector(props, i->m_Name).GetPtr();
			const float * val = reinterpret_cast<const float*>(buffer);

			switch (i->m_ColCount)
			{
				case 1:	CachedUniform1(cache, i->m_Index, val);	break;
				case 2: CachedUniform2(cache, i->m_Index, val);	break;
				case 3:	CachedUniform3(cache, i->m_Index, val);	break;
				case 4: CachedUniform4(cache, i->m_Index, val);	break;
				default:										break;
			}

			#if DEBUG_GLSL_BINDINGS
			;;printf_console("  vector %i dim=%i\n", i->m_Index, i->m_Dim );
			#endif

			buffer += 4*sizeof(float);
		}
		else
		{
			// Apply matrix parameters
			DebugAssert (i->m_ArraySize == 1);
			int size = *reinterpret_cast<const int*>(buffer); buffer += sizeof(int);
			Assert (size == 16);
			const Matrix4x4f* mat = reinterpret_cast<const Matrix4x4f*>(buffer);
			if (i->m_RowCount == 3 && i->m_ColCount == 3)
			{
				Matrix3x3f m33 = Matrix3x3f(*mat);
				GLES_CHK(glUniformMatrix3fv (i->m_Index, 1, GL_FALSE, m33.GetPtr()));
			}
			else
			{
				const float *ptr = mat->GetPtr ();
				GLES_CHK(glUniformMatrix4fv (i->m_Index, 1, GL_FALSE, ptr));
			}
			#if DEBUG_GLSL_BINDINGS
			;;printf_console("  matrix %i (%s)\n", i->m_Index, i->m_Name.GetName() );
			#endif
			buffer += size * sizeof(float);
		}
	}

	// Apply textures
	int j = 0;
	const GpuProgramParameters::TextureParameterList& textureParams = params.GetTextureParams();
	GpuProgramParameters::TextureParameterList::const_iterator textureParamsEnd = textureParams.end();
	for( GpuProgramParameters::TextureParameterList::const_iterator i = textureParams.begin(); i != textureParamsEnd; ++i )
	{
		const TexEnvData* texdata = reinterpret_cast<const TexEnvData*>(buffer);
		#if DEBUG_GLSL_BINDINGS
		;;printf_console("  sampler %i to unit %i (%s) id=%i dim=%i\n", t.m_Index, j, t.m_Name.GetName(), tex->GetActualTextureID().m_ID, tex->GetTexDim() );
		#endif
		ApplyTexEnvData (j, j, *texdata);
		++j;
		buffer += sizeof(TexEnvData);
	}

	// Fog parameters if needed
	if (index > 0)
	{
		if (m_FogColorIndex[fog.mode] >= 0)
			CachedUniform4(m_UniformCache+index, m_FogColorIndex[fog.mode], fog.color.GetPtr());

		Vector4f params(
			fog.density * 1.2011224087f,// density / sqrt(ln(2))
			fog.density * 1.4426950408f, // density / ln(2)
			0.0f,
			0.0f
		);

		if (fog.mode == kFogLinear)
		{
			float diff = fog.end - fog.start;
			float invDiff = Abs(diff) > 0.0001f ? 1.0f/diff : 0.0f;
			params[2] = -invDiff;
			params[3] = fog.end * invDiff;
		}

		if (m_FogParamsIndex[fog.mode] >= 0)
			CachedUniform4(m_UniformCache+index, m_FogParamsIndex[fog.mode], params.GetPtr());
	}

	GLESAssert();

	return index;
}


// --------------------------------------------------------------------------

const ShaderLab::FastPropertyName FixedFunctionProgramGLES30::kSLPropTransformBlock = ShaderLab::Property("_ffTransform");
const ShaderLab::FastPropertyName FixedFunctionProgramGLES30::kSLPropUVTransformBlock = ShaderLab::Property("_ffUVTransform");


FixedFunctionProgramGLES30::FixedFunctionProgramGLES30(GLShaderID vertexShader, GLShaderID fragmentShader)
:	m_GLSLProgram(0)
,	m_GLSLVertexShader(vertexShader)
,	m_GLSLFragmentShader(fragmentShader)
{
	m_GLSLProgram = Create(m_GLSLVertexShader, m_GLSLFragmentShader);
}

FixedFunctionProgramGLES30::~FixedFunctionProgramGLES30 ()
{
	// NOTE: do not delete vertex/fragment shaders; they can be shared between multiple programs
	// and are deleted in ClearFixedFunctionPrograms
	if (m_GLSLProgram != 0 ) // only delete valid programs
		GLES_CHK(glDeleteProgram(m_GLSLProgram));

	m_UniformCache.Destroy();
}

GLShaderID FixedFunctionProgramGLES30::Create(GLShaderID vertexShader, GLShaderID fragmentShader)
{
	GLESAssert(); // Clear any GL errors

	GLuint program;
	GLES_CHK(program = glCreateProgram());

	for (int i = 0; i < kAttribLookupTableSize; i++)
	{
		if (s_GLESVertexComponents[i] < gGraphicsCaps.gles30.maxAttributes && s_GLESVertexComponents[i] >= 0)
			GLES_CHK(glBindAttribLocation(program, s_GLESVertexComponents[i], s_GLSLESAttributes[i]));
	}

	GLES_CHK(glAttachShader(program, m_GLSLVertexShader));
	GLES_CHK(glAttachShader(program, m_GLSLFragmentShader));

	//We must link only after binding the attributes
	GLES_CHK(glLinkProgram(program));

	int linked = 0;
	GLES_CHK(glGetProgramiv(program, GL_LINK_STATUS, &linked));
	if (linked == 0)
	{
		ParseGlslErrors(program, kErrorLinkProgram);
		GLES_CHK(glDeleteProgram(program));
		return 0;
	}

	// Figure out the uniforms
	int activeUniforms;
	int activeUniformBlocks;

	char name[1024];
	GLenum type;
	int size	= 0,
		length	= 0,
		bufSize = sizeof(name);

	// Fetch texture stage samplers. Bind GLSL uniform to the OpenGL texture unit
	// just once, cause it never changes after that for this program; and could cause
	// internal shader recompiles when changing them.
	DBG_LOG_GLES30("GLSL: apply fixed-function program id=%i\n", program);
	GLSLUseProgramGLES30 (program);

	for (int i = 0; i < kMaxSupportedTextureUnitsGLES; i++)
	{
		std::string samplerName = Format("u_sampler%d", i);
		GLint uniformNumber;
		GLES_CHK(uniformNumber = glGetUniformLocation(program, samplerName.c_str()));

		if (uniformNumber != -1)
		{
			GLES_CHK(glUniform1i (uniformNumber, i));
			DBG_GLSL_BINDINGS_GLES30("  FFsampler: %s nr=%d", samplerName.c_str(), uniformNumber);
		}
	}

	// uniform blocks
	GLES_CHK(glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &activeUniformBlocks));
	m_Params.GetConstantBuffers().clear();
	m_Params.GetConstantBuffers().reserve(activeUniformBlocks);
	for (int i = 0; i < activeUniformBlocks; ++i)
	{
		GLES_CHK(glGetActiveUniformBlockName(program, i, bufSize, &length, name));
		GLES_CHK(glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_DATA_SIZE, &size));

		GpuProgramParameters::ConstantBuffer cb;
		cb.m_Name = ShaderLab::Property(name);
		cb.m_Size = size;
		cb.m_BindIndex = i;

		GLES_CHK(glUniformBlockBinding(program, i, i));

		m_Params.GetConstantBuffers().push_back(cb);
		GetRealGfxDevice().SetConstantBufferInfo(cb.m_Name.index, cb.m_Size);
	}

	// fetch generic params
	GLES_CHK(glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &activeUniforms));
	for(int i=0; i < activeUniforms; i++)
	{
		int uniformNumber;
		int cbIndex = -1;

		GLES_CHK(glGetActiveUniform(program, i, bufSize, &length, &size, &type, name));
		GLES_CHK(uniformNumber = glGetUniformLocation(program, name));

		const char* glslName = GetGLSLES3PropertyNameRemap(name);
		const char* unityName = glslName ? glslName : name;

		if (uniformNumber == GL_INVALID_INDEX)
		{
			// in a uniform block
			GLES_CHK(glGetActiveUniformsiv(program, 1, (GLuint*)&i, GL_UNIFORM_OFFSET, &uniformNumber));
			GLES_CHK(glGetActiveUniformsiv(program, 1, (GLuint*)&i, GL_UNIFORM_BLOCK_INDEX, &cbIndex));
		}

		if(type == GL_FLOAT) {
			m_Params.AddVectorParam(uniformNumber, kShaderParamFloat, 1, unityName, cbIndex, NULL);
		}
		else if(type == GL_FLOAT_VEC2) {
			m_Params.AddVectorParam(uniformNumber, kShaderParamFloat, 2, unityName, cbIndex, NULL);
		}
		else if(type == GL_FLOAT_VEC3) {
			m_Params.AddVectorParam(uniformNumber, kShaderParamFloat, 3, unityName, cbIndex, NULL);
		}
		else if(type == GL_FLOAT_VEC4) {
			m_Params.AddVectorParam(uniformNumber, kShaderParamFloat, 4, unityName, cbIndex, NULL);
		}
		else if(type == GL_FLOAT_MAT4) {
			m_Params.AddMatrixParam(uniformNumber, unityName, 4, 4, cbIndex, NULL);
		}
		else if(type == GL_FLOAT_MAT3) {
			m_Params.AddMatrixParam(uniformNumber, unityName, 3, 3, cbIndex, NULL);
		}
		else if(type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE) {
		}
		else {
			AssertString( "Unrecognized GLSL uniform type" );
		}
		DBG_GLSL_BINDINGS_GLES30("  FFuniform: %s nr=%d type=%d", unityName, uniformNumber, type);
	}

	m_UniformCache.Create(&m_Params, -1,-1);

	GLESAssert();
	return program;
}

void FixedFunctionProgramGLES30::ApplyFFGpuProgram(const BuiltinShaderParamValues& values, ConstantBuffersGLES30& cbs) const
{
	DBG_LOG_GLES30("FixedFunctionProgramGLES30::ApplyFFGpuProgram()");

	DBG_LOG_GLES30("GLSL: apply fixed-function program id=%i\n", m_GLSLProgram);
	GLSLUseProgramGLES30 (m_GLSLProgram);

	cbs.ResetBinds();

	// Apply float/vector parameters
	const GpuProgramParameters::ValueParameterArray& valueParams = m_Params.GetValueParams();
	GpuProgramParameters::ValueParameterArray::const_iterator valueParamsEnd = valueParams.end();
	for( GpuProgramParameters::ValueParameterArray::const_iterator i = valueParams.begin(); i != valueParamsEnd;  ++i )
	{
		if(i->m_RowCount == 1 && i->m_ArraySize == 1)
		{
			UniformCacheGLES30*	cache	= &m_UniformCache;
			const float * val = values.GetVectorParam((BuiltinShaderVectorParam)i->m_Name.BuiltinIndex()).GetPtr();

			switch (i->m_ColCount)
			{
				case 1:	CachedUniform1(cache, i->m_Index, val);	break;
				case 2: CachedUniform2(cache, i->m_Index, val);	break;
				case 3:	CachedUniform3(cache, i->m_Index, val);	break;
				case 4: CachedUniform4(cache, i->m_Index, val);	break;
				default:										break;
			}
		}
		else
		{
			// Apply matrix parameters
			DebugAssert (i->m_ArraySize == 1);
			int matSize;
			const Matrix4x4f& mat = values.GetMatrixParam((BuiltinShaderMatrixParam)i->m_Name.BuiltinIndex());
			if (i->m_RowCount == 3 && i->m_ColCount == 3)
			{
				Matrix3x3f m33 = Matrix3x3f(mat);
				GLES_CHK(glUniformMatrix3fv (i->m_Index, 1, GL_FALSE, m33.GetPtr()));
			}
			else
			{
				const float *ptr = mat.GetPtr ();
				GLES_CHK(glUniformMatrix4fv (i->m_Index, 1, GL_FALSE, ptr));
			}
			DBG_GLSL_BINDINGS_GLES30("  FFmatrix %i (%s)", i->m_Index, i->m_Name.GetName() );
		}
	}

	GLESAssert();
}


#if UNITY_ANDROID || UNITY_BLACKBERRY
//-----------------------------------------------------------------------------
// md5 internals are extracted from External/MurmurHash/md5.cpp
	struct
	md5_context
	{
		unsigned long total[2];
		unsigned long state[4];
		unsigned char buffer[64];
		unsigned char ipad[64];
		unsigned char opad[64];
	};

	extern void md5_starts(md5_context* ctx);
	extern void md5_update(md5_context* ctx, unsigned char* input, int ilen);
	extern void md5_finish(md5_context* ctx, unsigned char output[16]);
#endif


static void GetCachedBinaryName(const std::string& vprog, const std::string& fshader, char filename[33])
{
// we have caching only on android, so no need to worry (at least for now) about md5
#if UNITY_ANDROID || UNITY_BLACKBERRY

	unsigned char hash[16] = {0};

	md5_context ctx;
	md5_starts(&ctx);
	md5_update(&ctx, (unsigned char*)vprog.c_str(), vprog.length());
	md5_update(&ctx, (unsigned char*)fshader.c_str(), fshader.length());
	md5_finish(&ctx, hash);

	BytesToHexString(hash, 16, filename);

#else

	(void)vprog;
	(void)fshader;
	::memset(filename, '1', 33);

#endif
}


#endif // GFX_SUPPORTS_OPENGLES30
