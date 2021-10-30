#include "UnityPrefix.h"
#include "ArbGpuProgamGL.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Math/Vector4.h"
#include "UnityGL.h"
#include "GLAssert.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Utilities/Word.h"

unsigned int GetGLShaderImplTarget( ShaderImplType implType ); // GfxDeviceGL.cpp
void InvalidateActiveShaderStateGL( ShaderType type ); // GfxDeviceGL.cpp


/*
For ARB fragment programs on OS X, Intel, ATI cards, cache some first parameter values and
use Env paramters instead of Local ones.
Otherwise OS X 10.4.10 will insist on recompiling some shaders behind the scenes, or
some other funky stuff - which will make shadows be extremely slow:
 
 From: Aras Pranckevicius <aras@otee.dk>
 To: Michel Castejon <mcast@apple.com>
 Date: Jul 3, 2007 11:35 AM
 <...>
 On my MacBookPro/X1600 it gives 14 FPS with the glProgramLocalParameter4fv call, and 77 FPS
 without the call. That call is the only difference between the two cases - when drawing the
 objects receiving the shadow, the call sets the [1] local parameter (and the parameter is not
 actually used anywhere). The call does not actually change the value, i.e. it always sets it to
 {0.3, 0.7, 0.05, -4}.
 
 So, issuing this glProgramLocalParameter4fv that does not change the value of a local parameter
 that is not actually used, makes each glDrawElements take about 1.3 milliseconds (from GL profiler
 trace). If this call is not made, then each glDrawElements takes about 0.025 milliseconds (after
 the first draw with the shader one which is longer). I can't really tell what's happening from
 the Shark profile, as the difference is somewhere in the driver where I have no symbols.
 <...>
 
BUT do not do this on PPC Macs, Windows or other cards on Intel Macs. On PPC Macs using Env paramters
will make shadows completely randomly disappear. Oh the joy!
*/

const int kFPParamCacheSize = 32; // don't make this larger than 32 (won't fit into s_FPParamCacheValid mask)
UInt32 s_FPParamCacheValid = 0;
Vector4f s_FPParamCache[kFPParamCacheSize];

void InvalidateFPParamCacheGL()
{
	s_FPParamCacheValid = 0;
}


ArbGpuProgram::ArbGpuProgram( const std::string& source, ShaderType type, ShaderImplType implType )
{
	for (int i = 0; i < kFogModeCount; ++i)
		m_FogFailed[i] = false;
	m_ImplType = implType;
	if( !Create(source,type) )
		m_NotSupported = true;
}

ArbGpuProgram::~ArbGpuProgram ()
{
	for (int i = 0; i < kFogModeCount; ++i)
	{
		if (m_Programs[i] != 0)
		{
			glDeleteProgramsARB (1, &m_Programs[i]);
		}
	}
}


static std::string PreprocessARBShader( const std::string& source, ShaderImplType implType )
{
	std::string processed;
	processed = source;
	
	// Remove ARB precision hint if it's broken on this GPU
	if( gGraphicsCaps.gl.buggyArbPrecisionHint )
	{
		std::string::size_type pos;
		while ((pos = processed.find ("OPTION ARB_precision_hint_fastest")) != std::string::npos)
		{
			std::string::size_type end = processed.find ("\n", pos);
			if (end != std::string::npos)
				processed.erase (pos, end - pos);
		}
	}
	
	// In fragment programs on hardware where we have to use&cache Env params instead of Local
	// ones, do replacement in the shader.
	if( implType == kShaderImplFragment && gGraphicsCaps.gl.cacheFPParamsWithEnvs )
	{
		std::string kProgramLocal( "program.local" );
		std::string kProgramEnv( "program.env" );
		size_t pos = processed.find( kProgramLocal );
		while( pos != std::string::npos )
		{
			processed.erase( pos, kProgramLocal.size() );
			processed.insert( pos, kProgramEnv );
			pos = processed.find( kProgramLocal, pos );
		}
	}
	
	return processed;
}


bool ArbGpuProgram::Create( const std::string& source, ShaderType type )
{
	// We specially prefix ARB shaders that are compiled for SM3.0-like limits. If a shader is
	// prefixed this way, we can quickly skip loading & compiling it by just checking graphics caps.
	// Saves from weird driver bugs, does not spam the log and is faster.
	bool isShaderModel3 = false;
	if( !strncmp (source.c_str(), "3.0-!!", 6) )
	{
		if( gGraphicsCaps.shaderCaps < kShaderLevel3 )
			return false;
		isShaderModel3 = true;
	}
	
	m_GpuProgramLevel = isShaderModel3 ? kGpuProgramSM3 : kGpuProgramSM2;
	
	GLAssert(); // Clear any GL errors

	GLenum target = GetGLShaderImplTarget( m_ImplType );
	glGenProgramsARB (1, &m_Programs[0]);
	glBindProgramARB (target, m_Programs[0]);
	InvalidateActiveShaderStateGL( type );
	std::string processed = PreprocessARBShader( source, m_ImplType );
	
	glProgramStringARB( target, GL_PROGRAM_FORMAT_ASCII_ARB, processed.size() - (isShaderModel3?4:0), processed.c_str() + (isShaderModel3?4:0) );

	// Check for errors
	GLint errorPos;
	glGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errorPos );
    if( errorPos != -1 )
	{
		printf_console( "ARB shader compile error: %s for %s\n", glGetString (GL_PROGRAM_ERROR_STRING_ARB), processed.c_str() );
		int errorCounter = 0;
		while( errorCounter < 10 && glGetError () != GL_NO_ERROR ) // prevent infinite loops for bad drivers
		{
			++errorCounter;
		}
			
		glDeleteProgramsARB (1, &m_Programs[0]);
		m_Programs[0] = 0;
		return false;
	}

	m_SourceForFog = processed.c_str() + (isShaderModel3?4:0);
	
	return true;
}

void ArbGpuProgram::ApplyGpuProgram (const GpuProgramParameters& params, const UInt8* buffer)
{
	GLenum target = GetGLShaderImplTarget( m_ImplType );

	if( m_ImplType == kShaderImplFragment && gGraphicsCaps.gl.cacheFPParamsWithEnvs )
	{
		// If we are fragment program and have to cache parameters, do that
		// Apply vector and matrix parameters with cache
		const GpuProgramParameters::ValueParameterArray& valueParams = params.GetValueParams();
		GpuProgramParameters::ValueParameterArray::const_iterator valueParamsEnd = valueParams.end();
		for( GpuProgramParameters::ValueParameterArray::const_iterator i = valueParams.begin(); i != valueParamsEnd;  ++i )
		{
			if (i->m_RowCount == 1 && i->m_ArraySize == 1)
			{
				const Vector4f& val = *reinterpret_cast<const Vector4f*>(buffer);
				int idx = i->m_Index;
				UInt32 mask = 1 << idx;
				if( idx >= kFPParamCacheSize )
				{
					OGL_CALL(glProgramEnvParameter4fvARB( target, idx, val.GetPtr() ));
				}
				else
				{
					if( !(s_FPParamCacheValid & mask) || s_FPParamCache[idx] != val )
					{
						OGL_CALL(glProgramEnvParameter4fvARB( target, idx, val.GetPtr() ));
						s_FPParamCache[idx] = val;
					}
					s_FPParamCacheValid |= mask;
					#if GFX_DEVICE_VERIFY_ENABLE
					Vector4f testVal;
					OGL_CALL(glGetProgramEnvParameterfvARB( target, idx, testVal.GetPtr() ));
					if( s_FPParamCache[idx] != testVal )
						ErrorString( Format("FP local cache mismatch at index %i", idx) );
					#endif
				}
				buffer += sizeof(Vector4f);
			}
			else
			{
				// Apply matrix parameters, with cache
				DebugAssert(i->m_RowCount == 4);
				int size = *reinterpret_cast<const int*> (buffer); buffer += sizeof(int);
				DebugAssert (size == 16);
				const Matrix4x4f* val = reinterpret_cast<const Matrix4x4f*>(buffer);
				const float *ptr = val->GetPtr();
				int idx = i->m_Index;
				s_FPParamCacheValid &= ~(15 << idx); // mark 4 consecutive parameters as invalid in the cache
				OGL_CALL(glProgramEnvParameter4fARB( target, idx+0, ptr[0], ptr[4], ptr[8], ptr[12] ));
				OGL_CALL(glProgramEnvParameter4fARB( target, idx+1, ptr[1], ptr[5], ptr[9], ptr[13] ));
				OGL_CALL(glProgramEnvParameter4fARB( target, idx+2, ptr[2], ptr[6], ptr[10], ptr[14] ));
				OGL_CALL(glProgramEnvParameter4fARB( target, idx+3, ptr[3], ptr[7], ptr[11], ptr[15] ));
				buffer += size * sizeof(float);
			}
		}
	}
	else
	{
		// Otherwise we are not FP or we don't have to cache - do the usual code path
		
		// Apply vector and matrix parameters
		const GpuProgramParameters::ValueParameterArray& valueParams = params.GetValueParams();
		GpuProgramParameters::ValueParameterArray::const_iterator valueParamsEnd = valueParams.end();
		for( GpuProgramParameters::ValueParameterArray::const_iterator i = valueParams.begin(); i != valueParamsEnd;  ++i )
		{
			if( i->m_RowCount == 1 )
			{
				const Vector4f& val = *reinterpret_cast<const Vector4f*>(buffer);
				OGL_CALL(glProgramLocalParameter4fvARB( target, i->m_Index, val.GetPtr() ));
				buffer += sizeof(Vector4f);
			}
			else
			{
				DebugAssert(i->m_RowCount == 4);
				int size = *reinterpret_cast<const int*> (buffer); buffer += sizeof(int);
				DebugAssert (size == 16);
				const Matrix4x4f* val = reinterpret_cast<const Matrix4x4f*>(buffer);
				const float *ptr = val->GetPtr();
				OGL_CALL(glProgramLocalParameter4fARB( target, i->m_Index+0, ptr[0], ptr[4], ptr[8], ptr[12] ));
				OGL_CALL(glProgramLocalParameter4fARB( target, i->m_Index+1, ptr[1], ptr[5], ptr[9], ptr[13] ));
				OGL_CALL(glProgramLocalParameter4fARB( target, i->m_Index+2, ptr[2], ptr[6], ptr[10], ptr[14] ));
				OGL_CALL(glProgramLocalParameter4fARB( target, i->m_Index+3, ptr[3], ptr[7], ptr[11], ptr[15] ));
				buffer += size * sizeof(float);
			}
		}
	}
	
	// Apply textures for fragment programs
	if (m_ImplType == kShaderImplFragment)
	{
		const GpuProgramParameters::TextureParameterList& textureParams = params.GetTextureParams();
		const GpuProgramParameters::TextureParameterList::const_iterator textureParamsEnd = textureParams.end();
		for( GpuProgramParameters::TextureParameterList::const_iterator i = textureParams.begin(); i != textureParamsEnd; ++i )
		{
			const GpuProgramParameters::TextureParameter& t = *i;
			const TexEnvData* texdata = reinterpret_cast<const TexEnvData*>(buffer);
			ApplyTexEnvData (t.m_Index, t.m_SamplerIndex, *texdata);
			buffer += sizeof(*texdata);
		}
	}
}



// --------------------------------------------------------------------------
// ARB patching for fog

#define DEBUG_FOG_PATCHING 0

static inline bool IsNewline( char c ) { return c == '\n' || c == '\r'; }

bool PatchVertexShaderFogARB (std::string& src)
{
	#if DEBUG_FOG_PATCHING
	printf_console ("ARB fog patching: original vertex shader:\n%s\n", src.c_str());
	#endif

	if (src.find("result.fogcoord") != std::string::npos)
		return false; // already has hardcoded fog mode

	// find write to result.position, and do the same for result.fogcoord
	size_t posWrite = src.find ("result.position.z");
	bool writesFullPos = false;
	if (posWrite == std::string::npos)
	{
		posWrite = src.find ("result.position");
		if (posWrite == std::string::npos)
		{
			DebugAssert (!"couldn't find write to result.position");
			return false;
		}
		writesFullPos = true;
	}

	// get whole line
	size_t n = src.size();
	size_t posWriteStart = posWrite, posWriteEnd = posWrite;
	while (posWriteStart > 0 && !IsNewline(src[posWriteStart])) --posWriteStart;
	++posWriteStart;
	while (posWriteEnd < n && !IsNewline(src[posWriteEnd])) ++posWriteEnd;

	std::string instr = src.substr (posWriteStart, posWriteEnd-posWriteStart);
	if (writesFullPos)
	{
		replace_string (instr, "result.position", "result.fogcoord", 0);
		instr.resize(instr.size()-1);
		instr += ".z;";
	}
	else
	{
		replace_string (instr, "result.position.z", "result.fogcoord", 0);
	}
	instr += '\n';

	// insert fog code just after write to position
	src.insert (posWriteEnd+1, instr);

	#if DEBUG_FOG_PATCHING
	printf_console ("ARB fog patching: after patching:\n%s\n", src.c_str());
	#endif
	return true;
}

bool PatchPixelShaderFogARB (std::string& src, FogMode fog)
{
	#if DEBUG_FOG_PATCHING
	printf_console ("ARB fog patching: original pixel shader:\n%s\n", src.c_str());
	#endif

	if (src.find("OPTION ARB_fog_") != std::string::npos)
		return false; // already has hardcoded fog mode

	// skip until next line after !!ARBfp1.0
	size_t pos = src.find("!!ARBfp1.0");
	if (pos == std::string::npos)
		return false; // something is wrong
	pos += 10; // skip !!ARBfp1.0

	size_t n = src.size();
	while (pos < n && !IsNewline(src[pos])) ++pos; // skip until newline
	while (pos < n && IsNewline(src[pos])) ++pos; // skip newlines
	if (pos >= n)
		return false;

	// insert fog option
	static const char* kFogOptions[kFogModeCount] = {"", "OPTION ARB_fog_linear;\n", "OPTION ARB_fog_exp;\n", "OPTION ARB_fog_exp2;\n"};
	src.insert (pos, kFogOptions[fog]);

	#if DEBUG_FOG_PATCHING
	printf_console ("ARB fog patching: after patching, fog mode %d:\n%s\n", fog, src.c_str());
	#endif
	return true;
}

GLShaderID ArbGpuProgram::GetGLProgram (FogMode fog)
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
			if (m_ImplType == kShaderImplFragment && PatchPixelShaderFogARB (src, fog))
			{
				// create program
				glGenProgramsARB (1, &m_Programs[index]);
				glBindProgramARB (GL_FRAGMENT_PROGRAM_ARB, m_Programs[index]);
				InvalidateActiveShaderStateGL (kShaderFragment);
				glProgramStringARB (GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, src.size(), src.c_str());
				GLint errorPos;
				glGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errorPos );
				if (errorPos != -1)
				{
					glDeleteProgramsARB (1, &m_Programs[index]);
					m_Programs[index] = 0;
					m_FogFailed[index] = true;
					index = 0;
				}
			}
			else if (m_ImplType == kShaderImplVertex && PatchVertexShaderFogARB (src))
			{
				// create program
				glGenProgramsARB (1, &m_Programs[index]);
				glBindProgramARB (GL_VERTEX_PROGRAM_ARB, m_Programs[index]);
				InvalidateActiveShaderStateGL (kShaderVertex);
				glProgramStringARB (GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, src.size(), src.c_str());
				GLint errorPos;
				glGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errorPos );
				if (errorPos != -1)
				{
					glDeleteProgramsARB (1, &m_Programs[index]);
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
	return m_Programs[index];
}

// --------------------------------------------------------------------------

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (ShaderPatchingARBTests)
{
	TEST(PatchVSZWrite)
	{
		std::string s;
		s = "!!ARBvp1.0\n"
			"DP4 result.position.z, vertex.position, c[0];\n"
			"END";
		CHECK (PatchVertexShaderFogARB(s));
		CHECK_EQUAL(
			"!!ARBvp1.0\n"
			"DP4 result.position.z, vertex.position, c[0];\n"
			"DP4 result.fogcoord, vertex.position, c[0];\n"
			"END", s);
	}
	TEST(PatchVSFullWrite)
	{
		std::string s;
		s = "!!ARBvp1.0\n"
			"MOV result.position, vertex.position;\n"
			"END";
		CHECK (PatchVertexShaderFogARB(s));
		CHECK_EQUAL(
			"!!ARBvp1.0\n"
			"MOV result.position, vertex.position;\n"
			"MOV result.fogcoord, vertex.position.z;\n"
			"END", s);
	}
	TEST(PatchVSWriteNotAtEnd)
	{
		std::string s;
		s = "!!ARBvp1.0\n"
			"MOV result.position, R0;\n"
			"MOV R0, R1;\n"
			"END";
		CHECK (PatchVertexShaderFogARB(s));
		CHECK_EQUAL(
			"!!ARBvp1.0\n"
			"MOV result.position, R0;\n"
			"MOV result.fogcoord, R0.z;\n"
			"MOV R0, R1;\n"
			"END", s);
	}

} // SUITE

#endif // ENABLE_UNIT_TESTS
