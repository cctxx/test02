#include "UnityPrefix.h"
#include "GLSLUtilities.h"
#include "External/shaderlab/Library/ShaderLabErrors.h"

#if UNITY_ANDROID || UNITY_IPHONE
	#include "Runtime/Shaders/GraphicsCaps.h"
#endif

using namespace std;

// --------------------------------------------------------------------------


void	OutputGLSLShaderError(const char* log, GLSLErrorType errorType, ShaderErrors& outErrors)
{

	const char* strErrorIds[kErrorCount] =
	{
		"GLSL Error in Vertex Shader: ",
		"GLSL Error in Fragment Shader: ",
		"GLSL Error while linking: "
	};

	string errorMessage = string(strErrorIds[errorType]) + log;

	int errorLine = 0;
	switch(errorType)
	{
	case kErrorCompileVertexShader:
	case kErrorCompileFragShader:
		{
			size_t b = errorMessage.find('(');
			size_t e = errorMessage.find(')');
			if(b != string::npos && e != string::npos && e>b)
			{
				string errorLineText = errorMessage.substr(b + 1, e - b - 1);
				errorLine = StringToInt(errorLineText);
			}

			outErrors.AddShaderError (errorMessage, errorLine, false);
		}
		break;
	case kErrorLinkProgram:
		errorMessage += log;
		outErrors.AddShaderError (errorMessage, errorLine, false);
		break;
	default:
		break;
	}
}


// --------------------------------------------------------------------------
// GLSL patching for fog

#define DEBUG_FOG_PATCHING 0

static inline bool IsNewline( char c ) { return c == '\n' || c == '\r'; }

static size_t FindStartOfBlock (const std::string& src, const std::string& token)
{
	size_t pos = src.find (token);
	if (pos == std::string::npos)
		return pos;
	const size_t n = src.size();
	while (pos < n && !IsNewline(src[pos])) ++pos; // skip until newline
	while (pos < n && IsNewline(src[pos])) ++pos; // skip newlines
	if (pos >= n)
		return std::string::npos;
	return pos;
}

static bool SkipUntilStatementEnd (const std::string& src, size_t& pos)
{
	const size_t n = src.size();
	while (pos < n && src[pos] != ';') ++pos; if (pos < n) ++pos;
	while (pos < n && IsNewline(src[pos])) ++pos; // skip following newlines
	return pos < n;
}

static void SkipUntilStatementBegin (const std::string& src, size_t& pos)
{
	const size_t n = src.size();
	while (pos > 0 && src[pos] != ';' && src[pos] != '{') --pos; if (pos < n) ++pos;
	while (pos < n && IsSpace(src[pos])) ++pos; // skip following whitespace
}

static size_t SkipGLSLDirectives (const std::string& src, size_t startPos = 0)
{
	size_t pos = startPos;
	const size_t n = src.size();
	while (pos < n)
	{
		// skip whitespace
		while (pos < n && IsSpace(src[pos])) ++pos;
		// have a '#'?
		if (pos >= n || src[pos] != '#')
			break;
		// skip until end of line
		while (pos < n && !IsNewline(src[pos])) ++pos;
	}
	return pos;
}


static int ParseGLSLVersion (const std::string& src, size_t startPos = 0)
{
	size_t pos = startPos;
	const size_t n = src.size();
	// skip whitespace
	while (pos < n && IsSpace(src[pos])) ++pos;
	// have a '#'?
	if (pos >= n || src[pos] != '#')
		return 0;
	++pos;
	// skip whitespace
	while (pos < n && IsSpace(src[pos])) ++pos;
	// have a "version"?
	if (0 != strncmp(src.c_str() + pos, "version", 7))
		return 0;
	pos += 7;
	// skip whitespace
	while (pos < n && IsSpace(src[pos])) ++pos;
	// parse a number
	int v = atoi(src.c_str() + pos);
	return v;
}

static const char* GetGLSLFogData (int version, bool vertex)
{
	if (version >= 150)
	{
		if (vertex)
			return "uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\nout float _unity_FogVar;\n";
		else
			return "uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\nin float _unity_FogVar;\n";
	}
	return "uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\nvarying float _unity_FogVar;\n";
}

static bool InsertFogVertexCode (std::string& src)
{
	size_t vertexStart = FindStartOfBlock (src, "#ifdef VERTEX");
	if (vertexStart == std::string::npos)
		return false;
	const int version = ParseGLSLVersion (src, vertexStart);
	vertexStart = SkipGLSLDirectives (src, vertexStart);

	const char* fogData = GetGLSLFogData(version, true);
	src.insert (vertexStart, fogData);

	size_t posWriteStart = src.find ("gl_Position", vertexStart);
	if (posWriteStart == std::string::npos)
		return false;

	size_t pos = posWriteStart;
	if (!SkipUntilStatementEnd (src, pos))
		return false;

	// insert fog code at pos!
	src.insert (pos, "_unity_FogVar = gl_Position.z;\n");

	return true;
}

static bool InsertFogFragmentCode (std::string& src, FogMode fog)
{
	size_t fragmentStart = FindStartOfBlock (src, "#ifdef FRAGMENT");
	if (fragmentStart == std::string::npos)
		return false;
	const int version = ParseGLSLVersion (src, fragmentStart);
	fragmentStart = SkipGLSLDirectives (src, fragmentStart);

	const char* fogData = GetGLSLFogData(version, false);
	src.insert (fragmentStart, fogData);

	bool writesToData = true; // writes to gl_FragData vs. gl_FragColor
	size_t colorWriteStart = src.find ("gl_FragData[0]", fragmentStart);
	if (colorWriteStart == std::string::npos)
	{
		colorWriteStart = src.find ("gl_FragColor", fragmentStart);
		if (colorWriteStart == std::string::npos)
			return false;
		writesToData = false;
	}

	size_t pos = colorWriteStart;
	if (!SkipUntilStatementEnd (src, pos))
		return false;

	// insert fog code at pos!
	std::string color = writesToData ? "gl_FragData[0]" : "gl_FragColor";
	if (fog == kFogExp2)
	{
		// fog = exp(-(density*z)^2)
		src.insert (pos,
			"  float _patchFog = _unity_FogParams.x * _unity_FogVar;\n"
			"  _patchFog = _patchFog * _patchFog;\n"
			"  _patchFog = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"  " + color + ".rgb = mix (_unity_FogColor.rgb, " + color + ".rgb, _patchFog);\n");
	}
	else if (fog == kFogExp)
	{
		// fog = exp(-density*z)
		src.insert (pos,
			"  float _patchFog = _unity_FogParams.y * _unity_FogVar;\n"
			"  _patchFog = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"  " + color + ".rgb = mix (_unity_FogColor.rgb, " + color + ".rgb, _patchFog);\n");
	}
	else if (fog == kFogLinear)
	{
		// fog = (end-z)/(end-start)
		src.insert (pos,
			"  float _patchFog = clamp (_unity_FogParams.z * _unity_FogVar + _unity_FogParams.w, 0.0, 1.0);\n"
			"  " + color + ".rgb = mix (_unity_FogColor.rgb, " + color + ".rgb, _patchFog);\n");
	}

	return true;
}

bool PatchShaderFogGLSL (std::string& src, FogMode fog)
{
	#if DEBUG_FOG_PATCHING
	printf_console ("GLSL fog patching: original shader:\n%s\n", src.c_str());
	#endif

	if (!InsertFogVertexCode (src))
		return false;
	if (!InsertFogFragmentCode (src, fog))
		return false;

	#if DEBUG_FOG_PATCHING
	printf_console ("GLSL fog patching: after patching, fog mode %d:\n%s\n", fog, src.c_str());
	#endif
	return true;
}



static bool InsertFogVertexCodeGLES (std::string& src, FogMode fog, bool canUseOptimizedCode)
{
	size_t posWriteStart = src.find ("gl_Position");
	if (posWriteStart == std::string::npos)
		return false;

	size_t pos = posWriteStart;
	if (!SkipUntilStatementEnd (src, pos))
		return false;

	// TODO: remove duplication between optimized/normal fog code

	// insert fog code at pos!
	if (fog == kFogExp2)
	{
		// fog = exp(-(density*z)^2)
		if(canUseOptimizedCode)
		{
			src.insert (pos,
				"  float _patchFog = _unity_FogParams.x * gl_Position.z;\n"
				"  _patchFog = _patchFog * _patchFog;\n"
				"  _unity_FogVar = vec4(clamp (exp2(-_patchFog), 0.0, 1.0)); _unity_FogVar.a = 1.0;\n"
				"  _unity_FogColorPreMul = _unity_FogColor * (vec4(1.0)-_unity_FogVar);\n"
			);
		}
		else
		{
			src.insert (pos,
				"  float _patchFog = _unity_FogParams.x * gl_Position.z;\n"
				"  _patchFog = _patchFog * _patchFog;\n"
				"  _unity_FogVar = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			);
		}
	}
	else if (fog == kFogExp)
	{
		// fog = exp(-density*z)
		if(canUseOptimizedCode)
		{
			src.insert (pos,
				"  float _patchFog = _unity_FogParams.y * gl_Position.z;\n"
				"  _unity_FogVar = vec4(clamp (exp2(-_patchFog), 0.0, 1.0)); _unity_FogVar.a = 1.0;\n"
				"  _unity_FogColorPreMul = _unity_FogColor * (vec4(1.0)-_unity_FogVar);\n"
			);
		}
		else
		{
			src.insert (pos,
				"  float _patchFog = _unity_FogParams.y * gl_Position.z;\n"
				"  _unity_FogVar = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			);
		}
	}
	else if (fog == kFogLinear)
	{
		// fog = (end-z)/(end-start)
		if(canUseOptimizedCode)
		{
			src.insert (pos,
				"  _unity_FogVar = vec4(clamp (_unity_FogParams.z * gl_Position.z + _unity_FogParams.w, 0.0, 1.0)); _unity_FogVar.a = 1.0;\n"
				"  _unity_FogColorPreMul = _unity_FogColor * (vec4(1.0)-_unity_FogVar);\n"
			);
		}
		else
		{
			src.insert (pos,
				"  _unity_FogVar = clamp (_unity_FogParams.z * gl_Position.z + _unity_FogParams.w, 0.0, 1.0);\n"
			);
		}
	}

	return true;
}

static bool InsertFogFragmentCodeGLES (std::string& src, bool canUseOptimizedCode)
{
	bool writesToData = true; // writes to gl_FragData vs. gl_FragColor
	size_t colorWriteStart = src.find ("gl_FragData[0]");
	if (colorWriteStart == std::string::npos)
	{
		colorWriteStart = src.find ("gl_FragColor");
		if (colorWriteStart == std::string::npos)
			return false;
		writesToData = false;
	}

	// iPad2 crashes with some shaders using fog (not all of them though), case 399638.
	// Seems to avoid the crash if we don't read gl_FragColor after writing it. So instead,
	// introduce a new local variable, replace any previous uses with it, do fog patching
	// on it, and write out to final destination in the end.

	// replace color with new variable name
	const std::string color = writesToData ? "gl_FragData[0]" : "gl_FragColor";
	replace_string (src, color, "_patchFragColor");

	// find color output statement start & end
	size_t posBegin, posEnd;
	posBegin = posEnd = colorWriteStart;
	if (!SkipUntilStatementEnd (src, posEnd))
		return false;
	SkipUntilStatementBegin (src, posBegin);

	// insert new output variable declaration
	src.insert (posBegin, "lowp vec4 _patchFragColor;\n");
	posEnd += strlen("lowp vec4 _patchFragColor;\n");

	// insert fog code
	if(canUseOptimizedCode)
		src.insert (posEnd, "  _patchFragColor = _patchFragColor * _unity_FogVar + _unity_FogColorPreMul; " + color + " = _patchFragColor;\n");
	else
		src.insert (posEnd, "  _patchFragColor.rgb = mix (_unity_FogColor.rgb, _patchFragColor.rgb, _unity_FogVar); " + color + " = _patchFragColor;\n");

	return true;
}

bool PatchShaderFogGLES (std::string& srcVS, std::string& srcPS, FogMode fog, bool useOptimizedFogCode)
{
	#if DEBUG_FOG_PATCHING
	printf_console ("GLES fog patching: original vertex shader:\n%s\n...and pixel shader:\n%s\n", srcVS.c_str(), srcPS.c_str());
	#endif

	const int versionVS = ParseGLSLVersion (srcVS);
	const int versionPS = ParseGLSLVersion (srcPS);
	const size_t startVS = SkipGLSLDirectives(srcVS);
	const size_t startPS = SkipGLSLDirectives(srcPS);
	const char* fogDataVS = NULL;
	const char* fogDataPS = NULL;
	if (useOptimizedFogCode)
	{
		if (versionVS >= 150)
		{
			fogDataVS = "uniform highp vec4 _unity_FogParams;\nuniform lowp vec4 _unity_FogColor;\nout lowp vec4 _unity_FogColorPreMul;\nout lowp vec4 _unity_FogVar;\n";
			fogDataPS = "in lowp vec4 _unity_FogColorPreMul;\nin lowp vec4 _unity_FogVar;\n";
		}
		else
		{
			fogDataVS = "uniform highp vec4 _unity_FogParams;\nuniform lowp vec4 _unity_FogColor;\nvarying lowp vec4 _unity_FogColorPreMul;\nvarying lowp vec4 _unity_FogVar;\n";
			fogDataPS = "varying lowp vec4 _unity_FogColorPreMul;\nvarying lowp vec4 _unity_FogVar;\n";
		}
	}
	else
	{
		if (versionPS >= 150)
		{
			fogDataVS = "uniform highp vec4 _unity_FogParams;\nout lowp float _unity_FogVar;\n";
			fogDataPS = "uniform lowp vec4 _unity_FogColor;\nin lowp float _unity_FogVar;\n";
		}
		else
		{
			fogDataVS = "uniform highp vec4 _unity_FogParams;\nvarying lowp float _unity_FogVar;\n";
			fogDataPS = "uniform lowp vec4 _unity_FogColor;\nvarying lowp float _unity_FogVar;\n";
		}
	}
	srcVS.insert (startVS, fogDataVS);
	srcPS.insert (startPS, fogDataPS);


	if (!InsertFogVertexCodeGLES (srcVS, fog, useOptimizedFogCode))
		return false;
	if (!InsertFogFragmentCodeGLES (srcPS, useOptimizedFogCode))
		return false;

	#if DEBUG_FOG_PATCHING
	printf_console ("GLES fog patching: after patching, fog mode %d:\n%s\n...and pixel shader:\n%s\n", fog, srcVS.c_str(), srcPS.c_str());
	#endif
	return true;
}

static unsigned CalculateVaryingCount(const std::string& src)
{
	unsigned varCount = 0;

	size_t varUsage = src.find("varying ");
	while(varUsage != std::string::npos)
	{
		++varCount;
		varUsage = src.find("varying ", varUsage+7);
	}

	return varCount;
}

bool CanUseOptimizedFogCodeGLES(const std::string& srcVS)
{
#if UNITY_ANDROID || UNITY_IPHONE
	// we add 2 varyings in optimized path
	// in normal path we add one, so would be good to check it too, but we dont take "packing" into account
	// so we just act conservatively and apply optimization only when we are sure it works
	return (CalculateVaryingCount(srcVS) + 2 <= gGraphicsCaps.gles20.maxVaryings);
#else
	(void)srcVS;
	return true;
#endif
}

bool IsShaderParameterArray(const char* name, unsigned nameLen, int arraySize, bool* isZeroElem)
{
	bool zeroElem = nameLen > 3 && strcmp (name+nameLen-3, "[0]") == 0;
	if(isZeroElem)
		*isZeroElem = zeroElem;

	return arraySize > 1 || zeroElem;
}


// --------------------------------------------------------------------------

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (GlslFogPatchingTests)
{
	struct GlslPatchTestFixture
	{
		std::string vs, fs, s;
		std::string expvs, expfs, exps;
		bool DoPatching(FogMode mode, bool checkOptimizedGLESCode)
		{
			s = "#ifdef VERTEX\n" + vs + "\n#endif\n#ifdef FRAGMENT\n" + fs + "\n#endif\n";
			bool ok = true;
			ok &= PatchShaderFogGLSL (s, mode);
			ok &= PatchShaderFogGLES (vs, fs, mode, checkOptimizedGLESCode);
			return ok;
		}
	};

	TEST(SkipGLSLDirectivesWorks)
	{
		const char* s;
		CHECK_EQUAL(0, SkipGLSLDirectives("void")); // no directives
		CHECK_EQUAL(2, SkipGLSLDirectives("  void")); // skipped space
		CHECK_EQUAL(2, SkipGLSLDirectives("\n\nvoid")); // skipped newlines
		CHECK_EQUAL(12, SkipGLSLDirectives("#pragma foo\n")); // skipped whole line
		CHECK_EQUAL(14, SkipGLSLDirectives("  #pragma foo\n")); // skipped whole line
		CHECK_EQUAL(15, SkipGLSLDirectives("#pragma foo\r\n\r\n")); // skipped whole line, including various newlines
		CHECK_EQUAL(24, SkipGLSLDirectives("#pragma foo\n#pragma foo\nvoid")); // skipped two lines
	}

	TEST(ParseGLSLVersionWorks)
	{
		CHECK_EQUAL(0, ParseGLSLVersion(""));
		CHECK_EQUAL(0, ParseGLSLVersion("void"));
		CHECK_EQUAL(0, ParseGLSLVersion("  # pragma"));
		CHECK_EQUAL(0, ParseGLSLVersion("#version"));

		CHECK_EQUAL(120, ParseGLSLVersion("#version 120"));
		CHECK_EQUAL(200, ParseGLSLVersion("  #  version   200"));
	}

	TEST_FIXTURE(GlslPatchTestFixture,PatchSkipsVersionAndExtensionDirectives)
	{
		vs =
			"#version 120\n"
			"#extension bar : enable\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"}";
		fs =
			"#extension bar : enable\n"
			"void main() {\n"
			"  gl_FragColor = vec4(1,2,3,4);\n"
			"}";
		CHECK(DoPatching(kFogExp2,true));
		expvs =
			"#version 120\n"
			"#extension bar : enable\n"
			"uniform highp vec4 _unity_FogParams;\n";
		expfs =
			"#extension bar : enable\n"
			"varying lowp vec4 _unity_FogColorPreMul;\n";
		exps =
			"#ifdef VERTEX\n"
			"#version 120\n"
			"#extension bar : enable\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"_unity_FogVar = gl_Position.z;\n"
			"}\n"
			"#endif\n"
			"#ifdef FRAGMENT\n"
			"#extension bar : enable\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"void main() {\n"
			"  gl_FragColor = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.x * _unity_FogVar;\n"
			"  _patchFog = _patchFog * _patchFog;\n"
			"  _patchFog = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"  gl_FragColor.rgb = mix (_unity_FogColor.rgb, gl_FragColor.rgb, _patchFog);\n"
			"}\n"
			"#endif\n";
		CHECK (BeginsWith(vs, expvs));
		CHECK (BeginsWith(fs, expfs));
		CHECK_EQUAL (exps, s);
	}

	TEST_FIXTURE(GlslPatchTestFixture,PatchTakesVersionIntoAccount)
	{
		vs =
			"#version 300 es\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"}";
		fs =
			"  #version  420\n"
			"void main() {\n"
			"  gl_FragColor = vec4(1,2,3,4);\n"
			"}";
		CHECK(DoPatching(kFogExp2,true));
		expvs =
			"#version 300 es\n"
			"uniform highp vec4 _unity_FogParams;\n"
			"uniform lowp vec4 _unity_FogColor;\n"
			"out lowp vec4 _unity_FogColorPreMul;\n"
			"out lowp vec4 _unity_FogVar;\n";
		expfs =
			"  #version  420\n"
			"in lowp vec4 _unity_FogColorPreMul;\n"
			"in lowp vec4 _unity_FogVar;\n";
		exps =
			"#ifdef VERTEX\n"
			"#version 300 es\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"out float _unity_FogVar;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"_unity_FogVar = gl_Position.z;\n"
			"}\n"
			"#endif\n"
			"#ifdef FRAGMENT\n"
			"  #version  420\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"in float _unity_FogVar;\n"
			"void main() {\n"
			"  gl_FragColor = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.x * _unity_FogVar;\n"
			"  _patchFog = _patchFog * _patchFog;\n"
			"  _patchFog = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"  gl_FragColor.rgb = mix (_unity_FogColor.rgb, gl_FragColor.rgb, _patchFog);\n"
			"}\n"
			"#endif\n";
		CHECK (BeginsWith(vs, expvs));
		CHECK (BeginsWith(fs, expfs));
		CHECK_EQUAL (exps, s);
	}

	TEST_FIXTURE(GlslPatchTestFixture,PatchExp2WriteToColorOptimized)
	{
		vs =
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"}";
		fs =
			"void main() {\n"
			"  gl_FragColor = vec4(1,2,3,4);\n"
			"}";
		CHECK(DoPatching(kFogExp2,true));
		expvs =
			"uniform highp vec4 _unity_FogParams;\n"
			"uniform lowp vec4 _unity_FogColor;\n"
			"varying lowp vec4 _unity_FogColorPreMul;\n"
			"varying lowp vec4 _unity_FogVar;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.x * gl_Position.z;\n"
			"  _patchFog = _patchFog * _patchFog;\n"
			"  _unity_FogVar = vec4(clamp (exp2(-_patchFog), 0.0, 1.0)); _unity_FogVar.a = 1.0;\n"
			"  _unity_FogColorPreMul = _unity_FogColor * (vec4(1.0)-_unity_FogVar);\n"
			"}";
		expfs =
			"varying lowp vec4 _unity_FogColorPreMul;\n"
			"varying lowp vec4 _unity_FogVar;\n"
			"void main() {\n"
			"  lowp vec4 _patchFragColor;\n"
			"_patchFragColor = vec4(1,2,3,4);\n"
			"  _patchFragColor = _patchFragColor * _unity_FogVar + _unity_FogColorPreMul; gl_FragColor = _patchFragColor;\n"
			"}";
		exps =
			"#ifdef VERTEX\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"_unity_FogVar = gl_Position.z;\n"
			"}\n"
			"#endif\n"
			"#ifdef FRAGMENT\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"void main() {\n"
			"  gl_FragColor = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.x * _unity_FogVar;\n"
			"  _patchFog = _patchFog * _patchFog;\n"
			"  _patchFog = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"  gl_FragColor.rgb = mix (_unity_FogColor.rgb, gl_FragColor.rgb, _patchFog);\n"
			"}\n"
			"#endif\n";
		CHECK_EQUAL (expvs, vs);
		CHECK_EQUAL (expfs, fs);
		CHECK_EQUAL (exps, s);
	}

	TEST_FIXTURE(GlslPatchTestFixture,PatchExp2WriteToColor)
	{
		vs =
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"}";
		fs =
			"void main() {\n"
			"  gl_FragColor = vec4(1,2,3,4);\n"
			"}";
		CHECK(DoPatching(kFogExp2,false));
		expvs =
			"uniform highp vec4 _unity_FogParams;\n"
			"varying lowp float _unity_FogVar;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.x * gl_Position.z;\n"
			"  _patchFog = _patchFog * _patchFog;\n"
			"  _unity_FogVar = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"}";
		expfs =
			"uniform lowp vec4 _unity_FogColor;\n"
			"varying lowp float _unity_FogVar;\n"
			"void main() {\n"
			"  lowp vec4 _patchFragColor;\n"
			"_patchFragColor = vec4(1,2,3,4);\n"
			"  _patchFragColor.rgb = mix (_unity_FogColor.rgb, _patchFragColor.rgb, _unity_FogVar); gl_FragColor = _patchFragColor;\n"
			"}";
		exps =
			"#ifdef VERTEX\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"_unity_FogVar = gl_Position.z;\n"
			"}\n"
			"#endif\n"
			"#ifdef FRAGMENT\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"void main() {\n"
			"  gl_FragColor = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.x * _unity_FogVar;\n"
			"  _patchFog = _patchFog * _patchFog;\n"
			"  _patchFog = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"  gl_FragColor.rgb = mix (_unity_FogColor.rgb, gl_FragColor.rgb, _patchFog);\n"
			"}\n"
			"#endif\n";
		CHECK_EQUAL (expvs, vs);
		CHECK_EQUAL (expfs, fs);
		CHECK_EQUAL (exps, s);
	}

	TEST_FIXTURE(GlslPatchTestFixture,PatchExpWriteToDataWithStuffAroundOptimized)
	{
		vs =
			"varying float foo;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"  foo = 1.0;\n"
			"}";
		fs =
			"varying float foo;\n"
			"void main() {\n"
			"  foo = 2.0;\n"
			"  gl_FragData[0] = vec4(1,2,3,4);\n"
			"}";
		CHECK(DoPatching(kFogExp,true));
		expvs =
			"uniform highp vec4 _unity_FogParams;\n"
			"uniform lowp vec4 _unity_FogColor;\n"
			"varying lowp vec4 _unity_FogColorPreMul;\n"
			"varying lowp vec4 _unity_FogVar;\n"
			"varying float foo;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.y * gl_Position.z;\n"
			"  _unity_FogVar = vec4(clamp (exp2(-_patchFog), 0.0, 1.0)); _unity_FogVar.a = 1.0;\n"
			"  _unity_FogColorPreMul = _unity_FogColor * (vec4(1.0)-_unity_FogVar);\n"
			"  foo = 1.0;\n"
			"}";
		expfs =
			"varying lowp vec4 _unity_FogColorPreMul;\n"
			"varying lowp vec4 _unity_FogVar;\n"
			"varying float foo;\n"
			"void main() {\n"
			"  foo = 2.0;\n"
			"  lowp vec4 _patchFragColor;\n"
			"_patchFragColor = vec4(1,2,3,4);\n"
			"  _patchFragColor = _patchFragColor * _unity_FogVar + _unity_FogColorPreMul; gl_FragData[0] = _patchFragColor;\n"
			"}";
		exps =
			"#ifdef VERTEX\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"varying float foo;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"_unity_FogVar = gl_Position.z;\n"
			"  foo = 1.0;\n"
			"}\n"
			"#endif\n"
			"#ifdef FRAGMENT\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"varying float foo;\n"
			"void main() {\n"
			"  foo = 2.0;\n"
			"  gl_FragData[0] = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.y * _unity_FogVar;\n"
			"  _patchFog = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"  gl_FragData[0].rgb = mix (_unity_FogColor.rgb, gl_FragData[0].rgb, _patchFog);\n"
			"}\n"
			"#endif\n";
		CHECK_EQUAL (expvs, vs);
		CHECK_EQUAL (expfs, fs);
		CHECK_EQUAL (exps, s);
	}

	TEST_FIXTURE(GlslPatchTestFixture,PatchExpWriteToDataWithStuffAround)
	{
		vs =
			"varying float foo;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"  foo = 1.0;\n"
			"}";
		fs =
			"varying float foo;\n"
			"void main() {\n"
			"  foo = 2.0;\n"
			"  gl_FragData[0] = vec4(1,2,3,4);\n"
			"}";
		CHECK(DoPatching(kFogExp,false));
		expvs =
			"uniform highp vec4 _unity_FogParams;\n"
			"varying lowp float _unity_FogVar;\n"
			"varying float foo;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.y * gl_Position.z;\n"
			"  _unity_FogVar = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"  foo = 1.0;\n"
			"}";
		expfs =
			"uniform lowp vec4 _unity_FogColor;\n"
			"varying lowp float _unity_FogVar;\n"
			"varying float foo;\n"
			"void main() {\n"
			"  foo = 2.0;\n"
			"  lowp vec4 _patchFragColor;\n"
			"_patchFragColor = vec4(1,2,3,4);\n"
			"  _patchFragColor.rgb = mix (_unity_FogColor.rgb, _patchFragColor.rgb, _unity_FogVar); gl_FragData[0] = _patchFragColor;\n"
			"}";
		exps =
			"#ifdef VERTEX\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"varying float foo;\n"
			"void main() {\n"
			"  gl_Position = vec4(1,2,3,4);\n"
			"_unity_FogVar = gl_Position.z;\n"
			"  foo = 1.0;\n"
			"}\n"
			"#endif\n"
			"#ifdef FRAGMENT\n"
			"uniform vec4 _unity_FogColor; uniform vec4 _unity_FogParams;\n"
			"varying float _unity_FogVar;\n"
			"varying float foo;\n"
			"void main() {\n"
			"  foo = 2.0;\n"
			"  gl_FragData[0] = vec4(1,2,3,4);\n"
			"  float _patchFog = _unity_FogParams.y * _unity_FogVar;\n"
			"  _patchFog = clamp (exp2(-_patchFog), 0.0, 1.0);\n"
			"  gl_FragData[0].rgb = mix (_unity_FogColor.rgb, gl_FragData[0].rgb, _patchFog);\n"
			"}\n"
			"#endif\n";
		CHECK_EQUAL (expvs, vs);
		CHECK_EQUAL (expfs, fs);
		CHECK_EQUAL (exps, s);
	}

	TEST(CalculateVaryingCount)
	{
		// TODO: should we handle packing? for now we dont need it, but point for the future
		std::string test = "varying float foo; varying float foo2;";
		CHECK_EQUAL(CalculateVaryingCount(test), 2);
	}
} // SUITE

#endif // ENABLE_UNIT_TESTS
