#include "UnityPrefix.h"
#include "ShaderPatchingD3D9.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Utilities/Word.h"

#define DEBUG_FOG_PATCHING 0


static inline bool IsNewline( char c ) { return c == '\n' || c == '\r'; }

static int FindMaxUsedDclIndex (const std::string& src, char registerName)
{
	size_t n = src.size();
	size_t pos = 0;
	int maxDcl = -1;
	while ((pos = src.find("dcl_", pos)) != std::string::npos)
	{
		// skip "dcl_"
		pos += 4;

		// skip until end of dcl_*
		while (pos < n && !isspace(src[pos]))
			++pos;
		// skip space
		while (pos < n && isspace(src[pos]))
			++pos;
		// is this an needed register type?
		if (pos < n && src[pos] == registerName) {
			int number = -1;
			sscanf (src.c_str() + pos + 1, "%d", &number);
			if (number > maxDcl)
				maxDcl = number;
		}
	}
	return maxDcl;
}


static bool InsertFogDcl (std::string& src, const std::string& registerName)
{
	// insert dcl_fog after vs_3_0/ps_3_0 line
	size_t pos = 6;
	while (pos < src.size() && !IsNewline(src[pos])) // skip until newline
		++pos;
	while (pos < src.size() && IsNewline(src[pos])) // skip newlines
		++pos;
	if (pos >= src.size())
		return false;
	src.insert (pos, Format("dcl_fog %s\n", registerName.c_str()));
	return true;
}


bool PatchPixelShaderFogD3D9 (std::string& src, FogMode fog, int fogColorReg, int fogParamsReg)
{
	const bool isPS3 = !strncmp(src.c_str(), "ps_3_0", 6);
	if (!isPS3)
		return true; // nothing to do

	#if DEBUG_FOG_PATCHING
	printf_console ("D3D9 fog patching: original pixel shader:\n%s\n", src.c_str());
	#endif

	// SM3.0 has 10 input registers (v0..v9).

	const int maxDclReg = FindMaxUsedDclIndex (src, 'v');
	if (maxDclReg >= 9)
	{
		// out of registers
		return false;
	}
	const int fogReg = 9;
	if (!InsertFogDcl (src, Format("v%d.x", fogReg)))
	{
		DebugAssert (!"failed to insert fog dcl");
		return false;
	}

	// Remap writes to oC0 with r30
	const int colorReg = 30;
	const int tempReg = 31;
	replace_string (src, "oC0", "r30");

	// make sure source ends with a newline
	if (!IsNewline(src[src.size()-1]))
		src += '\n';

	// inject fog handling code
	if (fog == kFogExp2)
	{
		// fog = exp(-(density*z)^2)
		src += Format("mul r%d.x, c%d.x, v%d.x\n", tempReg, fogParamsReg, fogReg);					// tmp = (density/sqrt(ln(2))) * fog
		src += Format("mul r%d.x, r%d.x, r%d.x\n", tempReg, tempReg, tempReg);						// tmp = tmp * tmp
		src += Format("exp_sat r%d.x, -r%d.x\n", tempReg, tempReg);									// tmp = saturate (exp2 (-tmp))
		src += Format("lrp r%d.rgb, r%d.x, r%d, c%d\n", colorReg, tempReg, colorReg, fogColorReg);	// color.rgb = lerp (color, fogColor, tmp)
	}
	else if (fog == kFogExp)
	{
		// fog = exp(-density*z)
		src += Format("mul r%d.x, c%d.y, v%d.x\n", tempReg, fogParamsReg, fogReg);					// tmp = (density/ln(2)) * fog
		src += Format("exp_sat r%d.x, -r%d.x\n", tempReg, tempReg);									// tmp = saturate (exp2 (-tmp))
		src += Format("lrp r%d.rgb, r%d.x, r%d, c%d\n", colorReg, tempReg, colorReg, fogColorReg);	// color.rgb = lerp (color, fogColor, tmp)
	}
	else if (fog == kFogLinear)
	{
		// fog = (end-z)/(end-start)
		src += Format("mad_sat r%d.x, c%d.z, v%d.x, c%d.w\n", tempReg, fogParamsReg, fogReg, fogParamsReg);	// tmp = (-1/(end-start)) * fog + (end/(end-start))
		src += Format("lrp r%d.rgb, r%d.x, r%d, c%d\n", colorReg, tempReg, colorReg, fogColorReg);			// color.rgb = lerp (color, fogColor, tmp)
	}


	// append final move into oC0
	src += Format("mov oC0, r%d\n", colorReg);

	#if DEBUG_FOG_PATCHING
	printf_console ("D3D9 fog patching: after patching, fog mode %d:\n%s\n", fog, src.c_str());
	#endif

	return true;
}


bool PatchVertexShaderFogD3D9 (std::string& src)
{
	const bool isVS3 = !strncmp(src.c_str(), "vs_3_0", 6);
	if (!isVS3)
		return true; // nothing to do

	#if DEBUG_FOG_PATCHING
	printf_console ("D3D9 fog patching: original vertex shader:\n%s\n", src.c_str());
	#endif

	// SM3.0 has 12 output registers (o0..o11), but the pixel shader only has 10 input ones.
	// Play it safe and let's assume we only have 10 here.

	const int maxDclReg = FindMaxUsedDclIndex (src, 'o');
	if (maxDclReg >= 9)
	{
		// out of registers
		return false;
	}
	const int fogReg = 9;
	std::string fogRegName = Format("o%d", fogReg);
	if (!InsertFogDcl (src, fogRegName))
	{
		DebugAssert (!"failed to insert fog dcl");
		return false;
	}

	// find write to o0, and do the same for oFog
	size_t posWrite = src.find ("o0.z,");
	bool writesFullPos = false;
	if (posWrite == std::string::npos)
	{
		posWrite = src.find ("o0,");
		if (posWrite == std::string::npos)
		{
			DebugAssert (!"couldn't find write to o0");
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
		replace_string (instr, "o0", fogRegName, 0);
		instr += ".z";
	}
	else
	{
		replace_string (instr, "o0.z", fogRegName, 0);
	}
	instr += '\n';

	//  insert fog code just after write to position
	src.insert (posWriteEnd+1, instr);

	#if DEBUG_FOG_PATCHING
	printf_console ("D3D9 fog patching: after patching:\n%s\n", src.c_str());
	#endif

	return true;
}


// --------------------------------------------------------------------------

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (ShaderPatchingD3D9Tests)
{

TEST(FindMaxDclIndexNotPresent)
{
	CHECK_EQUAL (-1, FindMaxUsedDclIndex("", 'v'));
	CHECK_EQUAL (-1, FindMaxUsedDclIndex("foobar", 'v'));
	CHECK_EQUAL (-1, FindMaxUsedDclIndex("dcl_", 'v'));
	CHECK_EQUAL (-1, FindMaxUsedDclIndex("dcl_foo", 'v'));
	CHECK_EQUAL (-1, FindMaxUsedDclIndex("dcl_foo ", 'v'));
	CHECK_EQUAL (-1, FindMaxUsedDclIndex("dcl_foo v", 'v'));
}
TEST(FindMaxDclIndexOne)
{
	CHECK_EQUAL (0, FindMaxUsedDclIndex("dcl_foobar v0", 'v'));
	CHECK_EQUAL (1, FindMaxUsedDclIndex("dcl_foobar    v1", 'v'));
	CHECK_EQUAL (2, FindMaxUsedDclIndex("dcl_foobar v2.x", 'v'));
	CHECK_EQUAL (3, FindMaxUsedDclIndex("dcl_foobar v3.rgb", 'v'));
	CHECK_EQUAL (6, FindMaxUsedDclIndex("dcl_foobar v6", 'v'));
	CHECK_EQUAL (10, FindMaxUsedDclIndex("dcl_foobar v10", 'v'));
	CHECK_EQUAL (0, FindMaxUsedDclIndex("ps_3_0\ndcl_foobar v0\nmov oC0, v0", 'v'));
}
TEST(FindMaxDclIndexMultiple)
{
	CHECK_EQUAL (2, FindMaxUsedDclIndex("dcl_foobar v0\ndcl_foobar v2", 'v'));
	CHECK_EQUAL (3, FindMaxUsedDclIndex("dcl_foobar v3\ndcl_foobar v1", 'v'));
}

TEST(PatchVSZWrite)
{
	std::string s;
	s = "vs_3_0\n"
		"dcl_position o0\n"
		"dp4 o0.z, c0, c1\n"
		;
	CHECK (PatchVertexShaderFogD3D9(s));
	CHECK_EQUAL(
		"vs_3_0\n"
		"dcl_fog o9\n"
		"dcl_position o0\n"
		"dp4 o0.z, c0, c1\n"
		"dp4 o9, c0, c1\n"
		, s);
}
TEST(PatchVSFullWrite)
{
	std::string s;
	s = "vs_3_0\n"
		"dcl_position o0\n"
		"mov o0, c0\n"
		;
	CHECK (PatchVertexShaderFogD3D9(s));
	CHECK_EQUAL(
		"vs_3_0\n"
		"dcl_fog o9\n"
		"dcl_position o0\n"
		"mov o0, c0\n"
		"mov o9, c0.z\n"
		, s);
}
TEST(PatchVSWriteNotAtEnd)
{
	std::string s;
	s = "vs_3_0\n"
		"dcl_position o0\n"
		"mov o0, r0\n"
		"mov r0, r1\n"
		;
	CHECK (PatchVertexShaderFogD3D9(s));
	CHECK_EQUAL(
		"vs_3_0\n"
		"dcl_fog o9\n"
		"dcl_position o0\n"
		"mov o0, r0\n"
		"mov o9, r0.z\n"
		"mov r0, r1\n"
		, s);
}
TEST(PatchPSDisjointColorAlphaWrite)
{
	std::string s = 
		"ps_3_0\n"
		"; 31 ALU, 2 TEX\n"
		"dcl_2d s0\n"
		"dcl_2d s1\n"
		"def c5, 0.0, 128.0, 2.0, 0\n"
		"dcl_texcoord0 v0.xy\n"
		"dcl_texcoord1 v1.xyz\n"
		"dcl_texcoord2 v2.xyz\n"
		"dcl_texcoord3 v3.xyz\n"
		"dcl_texcoord4 v4\n"
		"texldp r3.x, v4, s1\n"
		"dp3_pp r0.x, v3, v3\n"
		"rsq_pp r0.x, r0.x\n"
		"mad_pp r0.xyz, r0.x, v3, c0\n"
		"dp3_pp r0.w, r0, r0\n"
		"rsq_pp r0.w, r0.w\n"
		"mul_pp r0.xyz, r0.w, r0\n"
		"mov_pp r0.w, c4.x\n"
		"dp3_pp r0.x, v1, r0\n"
		"dp3_pp r2.x, v1, c0\n"
		"mul_pp r1.y, c5, r0.w\n"
		"max_pp r1.x, r0, c5\n"
		"pow r0, r1.x, r1.y\n"
		"mov r1.x, r0\n"
		"texld r0, v0, s0\n"
		"mul r1.w, r0, r1.x\n"
		"mul_pp r1.xyz, r0, c3\n"
		"mul_pp r0.xyz, r1, c1\n"
		"max_pp r2.x, r2, c5\n"
		"mul_pp r2.xyz, r0, r2.x\n"
		"mov_pp r0.xyz, c1\n"
		"mul_pp r0.xyz, c2, r0\n"
		"mad r0.xyz, r0, r1.w, r2\n"
		"mul_pp r2.w, r3.x, c5.z\n"
		"mul r0.xyz, r0, r2.w\n"
		"mad_pp oC0.xyz, r1, v2, r0\n" // color RGB
		"mov_pp r2.x, c1.w\n"
		"mul_pp r0.x, c2.w, r2\n"
		"mul_pp r0.y, r0.w, c3.w\n"
		"mul r0.x, r1.w, r0\n"
		"mad oC0.w, r3.x, r0.x, r0.y\n"; // color A
	std::string exps = 
		"ps_3_0\n"
		"dcl_fog v9.x\n"
		"; 31 ALU, 2 TEX\n"
		"dcl_2d s0\n"
		"dcl_2d s1\n"
		"def c5, 0.0, 128.0, 2.0, 0\n"
		"dcl_texcoord0 v0.xy\n"
		"dcl_texcoord1 v1.xyz\n"
		"dcl_texcoord2 v2.xyz\n"
		"dcl_texcoord3 v3.xyz\n"
		"dcl_texcoord4 v4\n"
		"texldp r3.x, v4, s1\n"
		"dp3_pp r0.x, v3, v3\n"
		"rsq_pp r0.x, r0.x\n"
		"mad_pp r0.xyz, r0.x, v3, c0\n"
		"dp3_pp r0.w, r0, r0\n"
		"rsq_pp r0.w, r0.w\n"
		"mul_pp r0.xyz, r0.w, r0\n"
		"mov_pp r0.w, c4.x\n"
		"dp3_pp r0.x, v1, r0\n"
		"dp3_pp r2.x, v1, c0\n"
		"mul_pp r1.y, c5, r0.w\n"
		"max_pp r1.x, r0, c5\n"
		"pow r0, r1.x, r1.y\n"
		"mov r1.x, r0\n"
		"texld r0, v0, s0\n"
		"mul r1.w, r0, r1.x\n"
		"mul_pp r1.xyz, r0, c3\n"
		"mul_pp r0.xyz, r1, c1\n"
		"max_pp r2.x, r2, c5\n"
		"mul_pp r2.xyz, r0, r2.x\n"
		"mov_pp r0.xyz, c1\n"
		"mul_pp r0.xyz, c2, r0\n"
		"mad r0.xyz, r0, r1.w, r2\n"
		"mul_pp r2.w, r3.x, c5.z\n"
		"mul r0.xyz, r0, r2.w\n"
		"mad_pp r30.xyz, r1, v2, r0\n"
		"mov_pp r2.x, c1.w\n"
		"mul_pp r0.x, c2.w, r2\n"
		"mul_pp r0.y, r0.w, c3.w\n"
		"mul r0.x, r1.w, r0\n"
		"mad r30.w, r3.x, r0.x, r0.y\n"
		"mul r31.x, c7.x, v9.x\n"
		"mul r31.x, r31.x, r31.x\n"
		"exp_sat r31.x, -r31.x\n"
		"lrp r30.rgb, r31.x, r30, c6\n"
		"mov oC0, r30\n";
	CHECK (PatchPixelShaderFogD3D9(s, kFogExp2, 6, 7));
	CHECK_EQUAL(exps, s);
}

} // SUITE

#endif // ENABLE_UNIT_TESTS
