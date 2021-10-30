#ifndef DEBUGGLES20_H
#define DEBUGGLES20_H

#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Math/Matrix4x4.h"


void DumpVertexArrayStateGLES20();

#if !UNITY_RELEASE
	#define DBG_LOG_GLES20_ACTIVE 0
	#define DBG_TEXTURE_VERBOSE_GLES20_ACTIVE 0
	#define DBG_SHADER_VERBOSE_GLES20_ACTIVE 0
	#define DBG_GLSL_BINDINGS_GLES20_ACTIVE 0
#else
	#define DBG_LOG_GLES20_ACTIVE 0
	#define DBG_TEXTURE_VERBOSE_GLES20_ACTIVE 0
	#define DBG_SHADER_VERBOSE_GLES20_ACTIVE 0
	#define DBG_GLSL_BINDINGS_GLES20_ACTIVE 0
#endif

#if DBG_LOG_GLES20_ACTIVE
	#define DBG_LOG_GLES20(...) {printf_console(__VA_ARGS__);printf_console("\n");}

	inline std::string GetMatrixString(const Matrix4x4f& mtx)
	{
		return Format("%.2f, %.2f, %.2f, %2f,\n"
				"%.2f, %.2f, %.2f, %2f,\n"
				"%.2f, %.2f, %.2f, %2f,\n"
				"%.2f, %.2f, %.2f, %2f",
				mtx[0],	mtx[1],	mtx[2],	mtx[3],
				mtx[4],	mtx[5],	mtx[6],	mtx[7],
				mtx[8],	mtx[9],	mtx[10],mtx[11],
				mtx[12],mtx[13],mtx[14],mtx[15]);
	}

	inline const char*	GetBoolString(bool type)
	{
		return type?"True":"False";
	}

	inline const char * GetBlendModeString(BlendMode type)
	{
		switch(type)
		{
		case kBlendZero:return "kBlendZero";
		case kBlendOne:return "kBlendOne";
		case kBlendDstColor:return "kBlendDstColor";
		case kBlendSrcColor:return "kBlendSrcColor";
		case kBlendOneMinusDstColor:return "kBlendOneMinusDstColor";
		case kBlendSrcAlpha:return "kBlendSrcAlpha";
		case kBlendOneMinusSrcColor:return "kBlendOneMinusSrcColor";
		case kBlendDstAlpha:return "kBlendDstAlpha";
		case kBlendOneMinusDstAlpha:return "kBlendOneMinusDstAlpha";
		case kBlendSrcAlphaSaturate:return "kBlendSrcAlphaSaturate";
		case kBlendOneMinusSrcAlpha:return "kBlendOneMinusSrcAlpha";
		default:return "GetBlendModeString<Undefined>";
		}
	}
	inline const char * GetCullModeString(CullMode type)
	{
		switch(type)
		{
		case kCullUnknown:return "kCullUnknown";
		case kCullOff:return "kCullOff:return";
		case kCullFront:return "kCullFront";
		case kCullBack:return "kCullBack";;
		default:return "GetCullMode<undefined>";
		}
	}

	inline const char * GetCompareFunctionString(CompareFunction type)
	{
		switch(type)
		{
		case kFuncUnknown:return "kFuncUnknown";
		case kFuncDisabled:return "kFuncDisabled";
		case kFuncNever:return "kFuncNever";
		case kFuncLess:return "kFuncLess";
		case kFuncEqual:return "kFuncEqual";
		case kFuncLEqual:return "kFuncLEqual";
		case kFuncGreater:return "kFuncGreater";
		case kFuncNotEqual:return "kFuncNotEqual";
		case kFuncGEqual:return "kFuncGEqual";
		case kFuncAlways:return "kFuncAlways";
		default:return "GetCompareFunctionString<Undefined>";
		}
	}

	inline const char * GetShaderTypeString(ShaderType type)
	{
		switch(type)
		{
		case kShaderNone:return "kShaderNone";
		case kShaderVertex:return "kShaderVertex";
		case kShaderFragment:return "kShaderFragment";
		default:return "GetShaderTypeString<undefined>";
		}
	}

	inline const char * GetShaderImplTypeString(ShaderImplType type)
	{
		switch(type)
		{
		case kShaderImplUndefined: return "kShaderImplUndefined";
		case kShaderImplVertex: return "kShaderImplVertex";
		case kShaderImplFragment: return "kShaderImplFragment";
		case kShaderImplBoth: return "kShaderImplBoth";
		default:return "GetShaderImplTypeString<Undefined>";
		}
	}

#else
	#define DBG_LOG_GLES20(...)
#endif

#if DBG_TEXTURE_VERBOSE_GLES20_ACTIVE
#define DBG_TEXTURE_VERBOSE_GLES20(...) {printf_console(__VA_ARGS__);printf_console("\n");}
#else
#define DBG_TEXTURE_VERBOSE_GLES20(...)
#endif

#if DBG_SHADER_VERBOSE_GLES20_ACTIVE
	#define DBG_SHADER_VERBOSE_GLES20(...) {printf_console(__VA_ARGS__);printf_console("\n");}
	#define DBG_SHADER_VERBOSE_GLES20_DUMP_SHADER(prefix, text) { printf_console("%s\n", prefix);DebugTextLineByLine(text);printf_console("\n---\n");}
#else
	#define DBG_SHADER_VERBOSE_GLES20(...)
	#define DBG_SHADER_VERBOSE_GLES20_DUMP_SHADER(prefix, text)
#endif

#if DBG_GLSL_BINDINGS_GLES20_ACTIVE
	#define DBG_GLSL_BINDINGS_GLES20(...) {printf_console(__VA_ARGS__);printf_console("\n");}
#else
	#define DBG_GLSL_BINDINGS_GLES20(...)
#endif


#endif
