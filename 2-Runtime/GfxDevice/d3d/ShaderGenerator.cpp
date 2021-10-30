#include "UnityPrefix.h"
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <assert.h>
#include "ShaderGenerator.h"
#include "Runtime/Utilities/Word.h"

enum ShaderInputRegister {
	kInputPosition,
	kInputNormal,
	kInputUV0,
	kInputUV1,
	kInputColor,
	kInputCount
};

const char* kShaderInputNames[kInputCount] = {
	"$IPOS",
	"$INOR",
	"$IUV0",
	"$IUV1",
	"$ICOL",
};

const char* kShaderInputDecls[kInputCount] = {
	"dcl_position",
	"dcl_normal",
	"dcl_texcoord0",
	"dcl_texcoord1",
	"dcl_color",
};



enum ShaderFragmentOptions {
	kOptionHasTexMatrix = (1<<0),
};

const int kConstantLocations[kConstCount] = {
	0,	// kConstMatrixMVP
	4,	// kConstMatrixMV
	8,	// kConstMatrixMV_IT
	12, // kConstMatrixTexture
	44, // kConstAmbient
	57, // kConstColorMatAmbient
	45, // kConstLightMisc
	46, // kConstMatDiffuse
	47, // kConstMatSpecular
	48, // kConstLightIndexes
};

enum CommonDependencies {
	kDep_CamSpacePos,
	kDep_CamSpaceN,
	kDep_ViewVector,
	kDep_ReflVector,
	kDep_Normal,
	kDepCount
};


// --------------------------------------------------------------------------

// transform position
const ShaderFragment kVS_Pos = {
	(1<<kInputPosition), // input
	(1<<kConstMatrixMVP), // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	NULL, // outs
	"dp4 oPos.x, $IPOS, c0\n"
	"dp4 oPos.y, $IPOS, c1\n"
	"dp4 oPos.z, $IPOS, c2\n"
	"dp4 oPos.w, $IPOS, c3\n",
};

// --------------------------------------------------------------------------
// temps

// NORM = vertex normal
const ShaderFragment kVS_Load_Normal = {
	(1<<kInputNormal), // input
	0, // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	"NORM", // outs
	"mov $O_NORM, $INOR\n"
};

// NORM = normalized vertex normal
const ShaderFragment kVS_Normalize_Normal = {
	0, // input
	0, // constants
	(1<<kDep_Normal), // deps
	0, // options
	1, // temps
	"NORM", // ins
	"NORM", // outs
	"nrm $TMP0.xyz, $O_NORM\n"
	"mov $O_NORM.xyz, $TMP0\n"
};


// OPOS = input position of the vertex
const ShaderFragment kVS_Temp_ObjSpacePos = {
	(1<<kInputPosition), // input
	0, // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	"OPOS", // outs
	"mov $O_OPOS, $IPOS\n"
};

// CPOS = camera space position of the vertex
const ShaderFragment kVS_Temp_CamSpacePos = {
	(1<<kInputPosition), // input
	(1<<kConstMatrixMV), // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	"CPOS", // outs
	"mul $O_CPOS, $IPOS.y, c5\n"
	"mad $O_CPOS, c4, $IPOS.x, $O_CPOS\n"
	"mad $O_CPOS, c6, $IPOS.z, $O_CPOS\n"
	"mad $O_CPOS, c7, $IPOS.w, $O_CPOS\n",
};

// CNOR = camera space normal of the vertex
const ShaderFragment kVS_Temp_CamSpaceN = {
	0, // input
	(1<<kConstMatrixMV_IT), // constants
	(1<<kDep_Normal), // deps
	0, // options
	0, // temps
	"NORM", // ins
	"CNOR", // outs
	"mul $O_CNOR, $O_NORM.y, c9\n"
	"mad $O_CNOR, c8, $O_NORM.x, $O_CNOR\n"
	"mad $O_CNOR, c10, $O_NORM.z, $O_CNOR\n",
};

// VIEW = normalized vertex-to-eye vector
const ShaderFragment kVS_Temp_ViewVector = {
	0, // input
	0, // constants
	(1<<kDep_CamSpacePos), // deps
	0, // options
	0, // temps
	"CPOS", // ins
	"VIEW", // outs
	"dp3 $O_VIEW.w, $O_CPOS, $O_CPOS\n"
	"rsq $O_VIEW.w, $O_VIEW.w\n"
	"mul $O_VIEW, -$O_CPOS, $O_VIEW.w\n",
};

// REFL = camera space reflection vector: 2*dot(V,N)*N-V
const ShaderFragment kVS_Temp_CamSpaceRefl = {
	0, // input
	0, // constants
	(1<<kDep_CamSpaceN) | (1<<kDep_ViewVector), // deps
	0, // options
	0, // temps
	"CNOR VIEW", // ins
	"REFL", // outs
	"mov $O_REFL.xyz, $O_VIEW\n"
	"dp3 $O_REFL.w, $O_REFL, $O_CNOR\n"
	"add $O_REFL.w, $O_REFL.w, $O_REFL.w\n"
	"mad $O_REFL.xyz, $O_REFL.w, $O_CNOR, -$O_REFL\n"
};

// cheap version
// SPHR = sphere map: N*0.5+0.5
//const ShaderFragment kVS_Temp_SphereMap = {
//	0, // input
//	(1<<kConstLightMisc), // constants
//	(1<<kDep_CamSpaceN), // deps
//	0, // options
//	0, // temps
//	"CNOR", // ins
//	"SPHR", // outs
//	"mad $O_SPHR.xyz, $O_CNOR, c45.w, c45.w"
//};

// SPHR = sphere map. R = reflection vector
// m = 2*sqrt(Rx*Rx + Ry*Ry + (Rz+1)*(Rz+1))
// SPHR = Rx/m + 0.5, Ry/m + 0.5
const ShaderFragment kVS_Temp_SphereMap = {
	0, // input
	(1<<kConstLightMisc), // constants
	(1<<kDep_ReflVector), // deps
	0, // options
	1, // temps
	"REFL", // ins
	"SPHR", // outs
	"mul $TMP0.xy, $O_REFL, $O_REFL\n"	// Rx*Rx, Ry*Ry
	"add $O_SPHR.w, $TMP0.y, $TMP0.x\n"	// Rx*Rx + Ry*Ry
	"add $O_SPHR.z, $O_REFL.z, c45.z\n"	// Rz+1
	"mad $O_SPHR.z, $O_SPHR.z, $O_SPHR.z, $O_SPHR.w\n" // (Rz+1)*(Rz+1) + Rx*Rx + Ry*Ry
	"mul $O_SPHR.z, $O_SPHR.z, c45.y\n"	// * 4
	"rsq $O_SPHR.z, $O_SPHR.z\n" // m
	"mad $O_SPHR.xy, $O_REFL, $O_SPHR.z, c45.w\n" // R/m+0.5
};

// --------------------------------------------------------------------------
// Texture coordinates

const ShaderFragment kVS_Load_UV0 = {
	(1<<kInputUV0), // input
	0, // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	"UV0", // outs
	"mov $O_UV0, $IUV0\n"
};

const ShaderFragment kVS_Load_UV1 = {
	(1<<kInputUV1), // input
	0, // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	"UV1", // outs
	"mov $O_UV1, $IUV1\n"
};

const ShaderFragment kVS_Out_TexCoord = {
	0, // input
	0, // constants
	0, // deps
	0, // options
	0, // temps
	"$0", // ins
	NULL, // outs
	"mov oT$PARAM, $I_0\n"
};


const ShaderFragment kVS_Out_Matrix2 = {
	0, // input
	(1<<kConstMatrixTexture), // constants
	0, // deps
	kOptionHasTexMatrix, // options
	1, // temps
	"$0", // ins
	NULL, // outs
	"mul $TMP0, $I_0.y, $TMPARAM1\n"
	"mad $TMP0, $TMPARAM0, $I_0.x, $TMP0\n"
	"add oT$PARAM, $TMPARAM3, $TMP0\n"
};

const ShaderFragment kVS_Out_Matrix3 = {
	0, // input
	(1<<kConstMatrixTexture), // constants
	0, // deps
	kOptionHasTexMatrix, // options
	1, // temps
	"$0", // ins
	NULL, // outs
	"mul $TMP0, $I_0.y, $TMPARAM1\n"
	"mad $TMP0, $TMPARAM0, $I_0.x, $TMP0\n"
	"mad $TMP0, $TMPARAM2, $I_0.z, $TMP0\n"
	"add oT$PARAM, $TMPARAM3, $TMP0\n"
};

// --------------------------------------------------------------------------
//  Lighting

const ShaderFragment kVS_Out_Diffuse_VertexColor= {
	(1<<kInputColor), // input
	0, // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	NULL, // outs
	"mov oD0, $ICOL\n"
};

const ShaderFragment kVS_Light_Diffuse_Pre = {
	0, // input
	(1<<kConstLightMisc), // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	"DIFF", // outs

	"mov $O_DIFF, c45.xxxz\n" // diffuse = 0
};

const ShaderFragment kVS_Light_Diffuse_Dir = {
	0, // input
	(1<<kConstLightMisc) | (1<<kConstLightIndexes), // constants
	(1<<kDep_CamSpaceN), // deps
	0, // options
	1, // temps
	"CNOR", // ins
	"DIFF", // outs

	"mov $O_CNOR.w, c48.y\n" // CNOR.w is reused as light data index
	"rep i1\n"
	"  mova a0.x, $O_CNOR.w\n"
	"  dp3 $TMP0.x, $O_CNOR, c61[a0.x]\n"				// NdotL
	"  slt $TMP0.w, c45.x, $TMP0.x\n"					// clamp = NdotL > 0
	"  mul $TMP0.xyz, $TMP0.x, c62[a0.x]\n"				// doff = NdotL * lightColor
	"  mad $O_DIFF.xyz, $TMP0.w, $TMP0, $O_DIFF\n"		// diffuse += diff * clamp
	"  add $O_CNOR.w, $O_CNOR.w, c45.y\n"				// index += 4
	"endrep\n"
};

const ShaderFragment kVS_Light_Diffuse_Point = {
	0, // input
	(1<<kConstLightMisc) | (1<<kConstLightIndexes), // constants
	(1<<kDep_CamSpaceN) | (1<<kDep_CamSpacePos), // deps
	0, // options
	3, // temps
	"CNOR CPOS", // ins
	"DIFF", // outs

	"mov $O_CNOR.w, c48.z\n" // CNOR.w is reused as light data index
	"rep i2\n"
	"  mova a0.x, $O_CNOR.w\n"
	"  add $TMP1.xyz, -$O_CPOS, c60[a0.x]\n"			// toLight in view space
	"  dp3 $TMP0.w, $TMP1, $TMP1\n"						// lightDirection = normalize(toLight)
	"  rsq $TMP1.w, $TMP0.w\n"
	"  mul $TMP1.xyz, $TMP1, $TMP1.w\n"
	"  dp3 $TMP1.x, $O_CNOR, $TMP1\n"					// NdotL
	"  slt $TMP1.y, c63[a0.x].z, $TMP0.w\n"				// range = range2 < toLight2
	"  mov $TMP1.z, c45.z\n"							// 1
	"  mad $TMP0.w, c63[a0.x].w, $TMP0.w, $TMP1.z\n"	// 1 + toLight2 * quadAttenuation
	"  rcp $TMP0.w, $TMP0.w\n"							// attenuation
	"  mad $TMP0.w, $TMP1.y, -$TMP0.w, $TMP0.w\n"		// attenuation = 0 if out of range
	"  sge $TMP1.y, $TMP1.x, c45.x\n"					// clamp = NdotL > 0
	"  mul $TMP2, $TMP1.x, c62[a0.x]\n"					// diff = NdotL * lightColor
	"  mul $TMP2, $TMP0.w, $TMP2\n"						// diff *= attenuation
	"  mad $O_DIFF.xyz, $TMP1.y, $TMP2, $O_DIFF\n"		// diffuse += diff * clamp
	"  add $O_CNOR.w, $O_CNOR.w, c45.y\n"				// index += 4
	"endrep\n"
};



const ShaderFragment kVS_Light_Diffuse_Spot = {
	0, // input
	(1<<kConstLightMisc) | (1<<kConstLightIndexes), // constants
	(1<<kDep_CamSpaceN) | (1<<kDep_CamSpacePos), // deps
	0, // options
	3, // temps
	"CNOR CPOS", // ins
	"DIFF", // outs

	"mov $O_CNOR.w, c48.x\n" // CNOR.w is reused as light data index
	"rep i0\n"
	"  mova a0.x, $O_CNOR.w\n"
	"  add $TMP1.xyz, -$O_CPOS, c60[a0.x]\n"			// toLight in view space
	"  dp3 $TMP0.w, $TMP1, $TMP1\n"						// lightDirection = normalize(toLight)
	"  rsq $TMP1.w, $TMP0.w\n"
	"  mul $TMP1.xyz, $TMP1, $TMP1.w\n"
	"  dp3 $TMP1.w, $O_CNOR, $TMP1\n"					// NdotL
	"  dp3 $TMP1.x, $TMP1, c61[a0.x]\n"					// rho = dot(L,lightAxisDirection)
	"  add $TMP1.x, $TMP1.x, -c63[a0.x].y\n"			// rho-cos(phi/2)
	"  mul $TMP1.x, $TMP1.x, c63[a0.x].x\n"				// spotAtten = (rho-cos(phi/2)) / (cos(theta/2)-cos(phi/2))
	"  mov $TMP1.z, c45.z\n"							// 1
	"  mad $TMP1.y, c63[a0.x].w, $TMP0.w, $TMP1.z\n"	// 1 + toLight2 * quadAttenuation
	"  rcp $TMP1.y, $TMP1.y\n"							// attenuation
	"  slt $TMP0.w, c63[a0.x].z, $TMP0.w\n"				// range = range2 < toLight2
	"  mad $TMP0.w, $TMP0.w, -$TMP1.y, $TMP1.y\n"		// attenuation = 0 if out of range
	"  max $TMP1.x, $TMP1.x, c45.x\n"					// spotAtten = saturate(spotAtten)
	"  min $TMP1.x, $TMP1.x, c45.z\n"
	"  mul $TMP0.w, $TMP0.w, $TMP1.x\n"					// attenuation *= spotAtten
	"  sge $TMP1.x, $TMP1.w, c45.x\n"					// clamp = NdotL > 0
	"  mul $TMP2, $TMP1.w, c62[a0.x]\n"					// diff = NdotL * lightColor
	"  mul $TMP2, $TMP0.w, $TMP2\n"						// diff *= attenuation
	"  mad $O_DIFF.xyz, $TMP1.x, $TMP2, $O_DIFF\n"		// diffuse += diff * clamp
	"  add $O_CNOR.w, $O_CNOR.w, c45.y\n"				// index += 4
	"endrep\n"
};


const ShaderFragment kVS_Light_Specular_Pre = {
	0, // input
	(1<<kConstLightMisc), // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	"DIFF SPEC", // outs
	"mov $O_DIFF, c45.xxxz\n" // diffuse = 0
	"mov $O_SPEC, c45.x\n" // specular = 0
};


const ShaderFragment kVS_Light_Specular_Dir = {
	0, // input
	(1<<kConstLightMisc) | (1<<kConstLightIndexes) | (1<<kConstMatSpecular), // constants
	(1<<kDep_CamSpaceN) | (1<<kDep_ViewVector), // deps
	0, // options
	2, // temps
	"CNOR VIEW", // ins
	"DIFF SPEC", // outs

	"mov $O_CNOR.w, c48.y\n" // CNOR.w is reused as light data index
	"rep i1\n"
	"  mova a0.x, $O_CNOR.w\n"
	"  mov $TMP0.xyz, c61[a0.x]\n"						// L = lightDirection
	// diffuse
	"  dp3 $TMP1.x, $O_CNOR, $TMP0\n"					// NdotL
	"  slt $TMP0.w, c45.x, $TMP1.x\n"					// clamp = NdotL > 0
	"  mul $TMP1, $TMP1.x, c62[a0.x]\n"					// diff = NdotL * lightColor
	"  mad $O_DIFF.xyz, $TMP0.w, $TMP1, $O_DIFF\n"		// diffuse += diff * clamp
	// spec
	"  add $TMP0.xyz, $TMP0, $O_VIEW\n"	// L + V
	"  nrm $TMP1.xyz, $TMP0\n"			// H = normalize(L + V)
	"  dp3 $TMP1.w, $TMP1, $O_CNOR\n"	// H dot N
	"  max $TMP1.w, $TMP1.w, c45.x\n"	// sp = max(H dot N, 0)
	"  pow $TMP1.w, $TMP1.w, c47.w\n"	// sp = pow(sp, exponent)
	"  mul $TMP1.w, $TMP1.w, $TMP0.w\n"	// sp *= clamp
	"  mad $O_SPEC.xyz, $TMP1.w, c62[a0.x], $O_SPEC\n" // spec += sp * lightColor

	"  add $O_CNOR.w, $O_CNOR.w, c45.y\n"				// index += 4
	"endrep\n"
};


const ShaderFragment kVS_Light_Specular_Point = {
	0, // input
	(1<<kConstLightMisc) | (1<<kConstLightIndexes) | (1<<kConstMatSpecular), // constants
	(1<<kDep_CamSpaceN) | (1<<kDep_CamSpacePos) | (1<<kDep_ViewVector), // deps
	0, // options
	3, // temps
	"CNOR CPOS VIEW", // ins
	"DIFF SPEC", // outs

	"mov $O_CNOR.w, c48.z\n" // CNOR.w is reused as light data index
	"rep i2\n"
	"  mova a0.x, $O_CNOR.w\n"
	"  add $TMP1.xyz, -$O_CPOS, c60[a0.x]\n"			// toLight in view space
	"  dp3 $TMP0.w, $TMP1, $TMP1\n"						// L = normalize(toLight)
	"  rsq $TMP1.w, $TMP0.w\n"
	"  mul $TMP1.xyz, $TMP1, $TMP1.w\n"
	// diffuse
	"  dp3 $TMP0.x, $O_CNOR, $TMP1\n"					// NdotL
	"  slt $TMP0.y, c63[a0.x].z, $TMP0.w\n"				// range = range2 < toLight2
	"  mov $TMP0.z, c45.z\n"							// 1
	"  mad $TMP0.w, c63[a0.x].w, $TMP0.w, $TMP0.z\n"	// 1 + toLight2 * quadAttenuation
	"  rcp $TMP0.w, $TMP0.w\n"							// attenuation
	"  mad $TMP0.w, $TMP0.y, -$TMP0.w, $TMP0.w\n"		// attenuation = 0 if out of range
	"  sge $TMP0.y, $TMP0.x, c45.x\n"					// clamp = NdotL > 0
	"  mul $TMP2, $TMP0.x, c62[a0.x]\n"					// diff = NdotL * lightColor
	"  mul $TMP2, $TMP0.w, $TMP2\n"						// diff *= attenuation
	"  mad $O_DIFF.xyz, $TMP0.y, $TMP2, $O_DIFF\n"			// diffuse += diff * clamp
	// spec
	"  add $TMP2.xyz, $TMP1, $O_VIEW\n"	// L + V
	"  nrm $TMP1.xyz, $TMP2\n"			// H = normalize(L + V)
	"  dp3 $TMP1.w, $TMP1, $O_CNOR\n"	// H dot N
	"  max $TMP1.w, $TMP1.w, c45.x\n"	// sp = max(H dot N, 0)
	"  pow $TMP1.w, $TMP1.w, c47.w\n"	// sp = pow(sp, exponent)
	"  mul $TMP1.w, $TMP1.w, $TMP0.w\n"	// sp *= attenuation
	"  mul $TMP1.w, $TMP1.w, $TMP0.y\n"	// sp *= clamp
	"  mad $O_SPEC.xyz, $TMP1.w, c62[a0.x], $O_SPEC\n" // spec += sp * lightColor

	"  add $O_CNOR.w, $O_CNOR.w, c45.y\n"				// index += 4
	"endrep\n"
};

const ShaderFragment kVS_Light_Specular_Spot = {
	0, // input
	(1<<kConstLightMisc) | (1<<kConstLightIndexes) | (1<<kConstMatSpecular), // constants
	(1<<kDep_CamSpaceN) | (1<<kDep_CamSpacePos) | (1<<kDep_ViewVector), // deps
	0, // options
	3, // temps
	"CNOR CPOS VIEW", // ins
	"DIFF SPEC", // outs

	"mov $O_CNOR.w, c48.x\n" // CNOR.w is reused as light data index
	"rep i0\n"
	"  mova a0.x, $O_CNOR.w\n"
	"  add $TMP1.xyz, -$O_CPOS, c60[a0.x]\n"			// toLight in view space
	"  dp3 $TMP0.w, $TMP1, $TMP1\n"						// lightDirection = normalize(toLight)
	"  rsq $TMP1.w, $TMP0.w\n"
	"  mul $TMP1.xyz, $TMP1, $TMP1.w\n"
	// diffuse
	"  dp3 $TMP1.w, $O_CNOR, $TMP1\n"					// NdotL
	"  dp3 $TMP0.x, $TMP1, c61[a0.x]\n"					// rho = dot(L,lightAxisDirection)
	"  add $TMP0.x, $TMP0.x, -c63[a0.x].y\n"			// rho-cos(phi/2)
	"  mul $TMP0.x, $TMP0.x, c63[a0.x].x\n"				// spotAtten = (rho-cos(phi/2)) / (cos(theta/2)-cos(phi/2))
	"  mov $TMP0.z, c45.z\n"							// 1
	"  mad $TMP0.y, c63[a0.x].w, $TMP0.w, $TMP0.z\n"	// 1 + toLight2 * quadAttenuation
	"  rcp $TMP0.y, $TMP0.y\n"							// attenuation
	"  slt $TMP0.w, c63[a0.x].z, $TMP0.w\n"				// range = range2 < toLight2
	"  mad $TMP0.w, $TMP0.w, -$TMP0.y, $TMP0.y\n"		// attenuation = 0 if out of range
	"  max $TMP0.x, $TMP0.x, c45.x\n"					// spotAtten = saturate(spotAtten)
	"  min $TMP0.x, $TMP0.x, c45.z\n"
	"  mul $TMP0.w, $TMP0.w, $TMP0.x\n"					// attenuation *= spotAtten
	"  sge $TMP0.x, $TMP1.w, c45.x\n"					// clamp = NdotL > 0
	"  mul $TMP2, $TMP1.w, c62[a0.x]\n"					// diff = NdotL * lightColor
	"  mul $TMP2, $TMP0.w, $TMP2\n"						// diff *= attenuation
	"  mad $O_DIFF.xyz, $TMP0.x, $TMP2, $O_DIFF\n"			// diffuse += diff * clamp
	// spec
	"  add $TMP2.xyz, $TMP1, $O_VIEW\n"	// L + V
	"  nrm $TMP1.xyz, $TMP2\n"			// H = normalize(L + V)
	"  dp3 $TMP1.w, $TMP1, $O_CNOR\n"	// H dot N
	"  max $TMP1.w, $TMP1.w, c45.x\n"	// sp = max(H dot N, 0)
	"  pow $TMP2.x, $TMP1.w, c47.w\n"	// sp = pow(sp, exponent)
	"  mul $TMP2.x, $TMP2.x, $TMP0.w\n"	// sp *= attenuation
	"  mul $TMP2.x, $TMP2.x, $TMP0.x\n"	// sp *= clamp
	"  mad $O_SPEC.xyz, $TMP2.x, c62[a0.x], $O_SPEC\n" // spec += sp * lightColor

	"  add $O_CNOR.w, $O_CNOR.w, c45.y\n"				// index += 4
	"endrep\n"
};


const ShaderFragment kVS_Out_Diffuse_Lighting = {
	0, // input
	(1<<kConstAmbient) | (1<<kConstMatDiffuse) | (1<<kConstLightMisc), // constants
	0, // deps
	0, // options
	0, // temps
	"DIFF", // ins
	NULL, // outs
	"mul $O_DIFF, $O_DIFF, c46\n"		// diffuse *= materialDiffuse
	"add $O_DIFF.xyz, $O_DIFF, c44\n"	// diffuse += ambient
	"min oD0, $O_DIFF, c45.z\n"			// diffuse = max(diffuse,1)
};

const ShaderFragment kVS_Out_Specular_Lighting = {
	0, // input
	(1<<kConstMatSpecular) | (1<<kConstLightMisc), // constants
	0, // deps
	0, // options
	0, // temps
	"SPEC", // ins
	NULL, // outs
	"mul $O_SPEC, $O_SPEC, c47\n"	// specular *= materialSpecular
	"min oD1, $O_SPEC, c45.z\n"		// specular = max(specular,1)
};

const ShaderFragment kVS_Out_Diffuse_Lighting_ColorDiffuseAmbient = {
	(1<<kInputColor), // input
	(1<<kConstColorMatAmbient) | (1<<kConstAmbient) | (1<<kConstLightMisc), // constants
	0, // deps
	0, // options
	0, // temps
	"DIFF", // ins
	NULL, // outs
	"mul $O_DIFF, $O_DIFF, $ICOL\n"				// diffuse *= vertexColor
	"mad $O_DIFF.xyz, $ICOL, c57, $O_DIFF\n"	// diffuse += ambient * vertexColor
	"add $O_DIFF.xyz, $O_DIFF, c44\n"			// diffuse += emissive
	"min oD0, $O_DIFF, c45.z\n"					// diffuse = max(diffuse,1)
};

const ShaderFragment kVS_Out_Diffuse_Lighting_ColorEmission = {
	(1<<kInputColor), // input
	(1<<kConstAmbient) | (1<<kConstMatDiffuse) | (1<<kConstLightMisc), // constants
	0, // deps
	0, // options
	0, // temps
	"DIFF", // ins
	NULL, // outs
	"mul $O_DIFF, $O_DIFF, c46\n"		// diffuse *= materialDiffuse
	"add $O_DIFF.xyz, c44, $O_DIFF\n"   // diffuse += ambient
	"add $O_DIFF, $O_DIFF, $ICOL\n"		// diffuse += vertex color
	"min oD0, $O_DIFF, c45.z\n"			// diffuse = max(diffuse,1)
};


const ShaderFragment kVS_Out_Diffuse_White = {
	0, // input
	(1<<kConstLightMisc), // constants
	0, // deps
	0, // options
	0, // temps
	NULL, // ins
	NULL, // outs
	"mov oD0, c45.z\n"
};


// --------------------------------------------------------------------------


static const ShaderFragment* kCommonDependencies[kDepCount] = {
	&kVS_Temp_CamSpacePos,
	&kVS_Temp_CamSpaceN,
	&kVS_Temp_ViewVector,
	&kVS_Temp_CamSpaceRefl,
	&kVS_Load_Normal,
};

static bool IsAlNum( char c ) {
	return c=='$' || c>='A' && c<='Z' || c>='0' && c<='9';
}

static const char* SkipTokens( const char* p, int count ) {
	while( count-- ) {
		while( IsAlNum(*p++) ) ;
		if( *p == 0 )
			return p;
		++p;
	}
	return p;
}

static std::string ExtractToken( const char** text ) {
	const char* ptr = *text;
	char c = *ptr;
	while( IsAlNum(c) ) {
		++ptr;
		c = *ptr;
	}

	if( ptr == *text )
		return std::string();

	// result
	std::string res(*text, ptr);

	// skip space after result
	++ptr;
	*text = ptr;

	return res;
}

void ShaderGenerator::AddFragment( const ShaderFragment* fragment, const char* inputNames, int param )
{
	// is already added?
	FragmentData data(fragment, inputNames, param);
	for( int i = 0; i < m_FragmentCount; ++i ) {
		if( m_Fragments[i] == data )
			return;
	}

	// add it's dependencies first
	if( fragment->dependencies ) {
		for( int i = 0; i < kDepCount; ++i ) {
			// has this dependency?
			if( !(fragment->dependencies & (1<<i)) )
				continue;
			AddFragment( kCommonDependencies[i] );
		}
	}

	// add itself
	m_Fragments[m_FragmentCount] = data;
	m_FragmentCount++;
	assert( m_FragmentCount < kMaxShaderFragments );
}

// Register plus it's living range - first and last shader fragment indices
// on where it can be used.
struct SavedRegister {
	std::string name;
	int	firstUse;
	int lastUse;
	int regIndex;
};
typedef std::vector<SavedRegister> SavedRegisters;

static inline int FindSavedRegister( const SavedRegisters& regs, const std::string& name )
{
	int n = regs.size();
	for( int i = 0; i < n; ++i )
		if( regs[i].name == name )
			return i;
	return -1;
}

void ShaderGenerator::GenerateShader( std::string& output, unsigned int& usedConstants )
{
	unsigned int usedConstantsMask = 0;

	output.clear();
	output.reserve(1024);
	//debug.clear();

	// shader input mappings
	int inputMapping[kInputCount];
	for( int i = 0; i < kInputCount; ++i )
		inputMapping[i] = -1;
	int usedInputStack[kInputCount];
	int usedInputs = 0;

	// saved registers across fragments
	SavedRegisters savedRegisters;

	// go over fragments and figure out inputs, saved registers and used constants
	int maxTemps = 0;
	for( int fi = 0; fi < m_FragmentCount; ++fi ) {
		const ShaderFragment& frag = *m_Fragments[fi].fragment;

		// fragment vertex inputs
		for( int i = 0; i < kInputCount; ++i ) {
			// does fragment use this input?
			if( frag.inputs & (1<<i) ) {
				// add to inputs list of in there yet
				if( inputMapping[i] == -1 ) {
					usedInputStack[usedInputs] = i;
					inputMapping[i] = usedInputs;
					++usedInputs;
				}
			}
		}

		// remember output registers
		if( frag.outs ) {
			const char* outputs = frag.outs;
			std::string token;
			while( !(token = ExtractToken(&outputs)).empty() ) {
				token = "$O_" + token;
				//TODO: check that text has that token.
				//TODO: check that text has no $O_ tokens that are not in the output
				// add to list if not there yet
				int savedIndex = FindSavedRegister( savedRegisters, token );
				if( savedIndex == -1 )
				{
					SavedRegister r;
					r.name = token;
					r.firstUse = fi;
					r.lastUse = fi;
					r.regIndex = -1;
					savedRegisters.push_back( r );
				}
				else
				{
					savedRegisters[savedIndex].lastUse = fi;
					assert(savedRegisters[savedIndex].firstUse <= savedRegisters[savedIndex].lastUse);
				}
			}
		}

		// from fragment input registers, determine last use of saved registers
		if( frag.ins ) {
			const char* inputs = frag.ins;
			std::string token;
			while( !(token = ExtractToken(&inputs)).empty() ) {
				// a parametrized token?
				if( token[0] == '$' ) {
					assert(token.size()==2);
					assert(token[1]>='0' && token[1]<='9');
					int index = token[1]-'0';
					const char* inputNames = m_Fragments[fi].inputNames;
					inputNames = SkipTokens( inputNames, index );
					token = ExtractToken(&inputNames);
				}
				token = "$O_" + token;

				//TODO: check that text has that token.
				//TODO: check that text has no $O_ tokens that are not in the input
				int savedIndex = FindSavedRegister( savedRegisters, token );
				assert(savedIndex != -1);
				assert(savedRegisters[savedIndex].lastUse <= fi);
				savedRegisters[savedIndex].lastUse = fi;
			}
		}

		maxTemps = std::max(maxTemps, frag.temps);

		// used constants
		usedConstantsMask |= frag.constants;
	}

	assert( savedRegisters.size() <= kMaxSavedRegisters );

	// assign register indices to saved registers
	int mapFragmentRegister[kMaxShaderFragments][kMaxTempRegisters]; // [fragment][index] = used or not?
	memset(mapFragmentRegister, 0, sizeof(mapFragmentRegister));
	for( size_t i = 0; i < savedRegisters.size(); ++i ) {
		// find unused register over whole lifetime, and assign it
		SavedRegister& sr = savedRegisters[i];
		assert(sr.regIndex == -1);
		for( int regIndex = 0; regIndex < kMaxTempRegisters; ++regIndex ) {
			bool unused = true;
			for( int fi = sr.firstUse; fi <= sr.lastUse; ++fi ) {
				if( mapFragmentRegister[fi][regIndex] != 0 ) {
					unused = false;
					break;
				}
			}
			if( unused ) {
				for( int fi = sr.firstUse; fi <= sr.lastUse; ++fi )
					mapFragmentRegister[fi][regIndex] = 1;
				sr.regIndex = regIndex;
				break;
			}
		}
		assert(sr.regIndex != -1);
	}

	// generate prolog with declarations
	output += "vs_2_0\n";
	for( int i = 0; i < usedInputs; ++i ) {
		output += kShaderInputDecls[usedInputStack[i]];
		output += " v";
		assert(i<=9);
		output += ('0' + i);
		output += '\n';
	}

	// go over fragments, transform register names and output
	for( int fi = 0; fi < m_FragmentCount; ++fi ) {
		const ShaderFragment& frag = *m_Fragments[fi].fragment;
		int param = m_Fragments[fi].param;

		output += '\n';
		std::string text = frag.text;

		std::string regname("r0");
		std::string regname2("r00");

		// input registers
		regname[0] = 'v';
		for( int i = 0; i < usedInputs; ++i ) {
			int inputIndex = usedInputStack[i];
			assert(i<=9);
			regname[1] = '0' + i;
			replace_string(text, kShaderInputNames[inputIndex], regname);
		}

		// fragment inputs
		if( frag.ins ) {
			const char* inputs = frag.ins;
			std::string token;
			while( !(token = ExtractToken(&inputs)).empty() ) {
				std::string searchName;
				std::string savedName;
				// a parametrized token?
				if( token[0] == '$' ) {
					assert(token.size()==2);
					assert(token[1]>='0' && token[1]<='9');
					int index = token[1]-'0';
					const char* inputNames = m_Fragments[fi].inputNames;
					inputNames = SkipTokens( inputNames, index );
					token = ExtractToken(&inputNames);
					searchName = std::string("$I_") + char('0'+index);
				} else {
					searchName = "$O_" + token;
				}
				savedName = "$O_" + token;

				// Assign register index to this saved reg
				SavedRegisters::iterator it, itEnd = savedRegisters.end();
				for( it = savedRegisters.begin(); it != itEnd; ++it ) {
					const SavedRegister& sr = *it;
					if( sr.name == savedName )
					{
						// replace with register value
						regname[0] = 'r';
						assert(sr.regIndex<=9);
						regname[1] = '0' + sr.regIndex;
						replace_string(text, searchName, regname);
						break;
					}
				}
				assert( it != itEnd );
			}
		}

		// saved registers
		if( frag.outs ) {
			regname[0] = 'r';
			SavedRegisters::iterator it, itEnd = savedRegisters.end();
			for( it = savedRegisters.begin(); it != itEnd; ++it ) {
				const SavedRegister& sr = *it;
				assert(sr.regIndex<=9);
				regname[1] = '0' + sr.regIndex;
				replace_string(text, sr.name, regname);
			}
		}

		// fragment-private temporary registers
		regname[0] = 'r';
		regname2[0] = 'r';
		std::string tmpname("$TMP0");
		int regIndex = 0;
		for( int i = 0; i < frag.temps; ++i ) {
			assert(i<=9);
			tmpname[4] = '0' + i;
			// find unused register at this fragment
			while( regIndex < kMaxTempRegisters && mapFragmentRegister[fi][regIndex] != 0 )
				++regIndex;
			assert(regIndex < kMaxTempRegisters);
			if( regIndex > 9 ) {
				regname2[1] = '1';
				regname2[2] = '0' + (regIndex-10);
				replace_string(text, tmpname, regname2);
			} else {
				regname[1] = '0' + regIndex;
				replace_string(text, tmpname, regname);
			}
			++regIndex;
		}

		// parameter
		if( param >= 0 ) {
			std::string paramString("0");
			assert(param<=9);
			paramString[0] = '0'+param;
			replace_string(text, "$PARAM", paramString);
		}

		// texture matrix parameters
		if( frag.options & kOptionHasTexMatrix ) {
			std::string tmpstring("$TMPARAM0");
			std::string paramString("c00");
			for( int i = 0; i < 4; ++i ) {
				assert(i<=9);
				tmpstring[8] = '0' + i;
				int constant = kConstantLocations[kConstMatrixTexture] + param*4 + i;
				paramString[1] = '0' + constant/10;
				paramString[2] = '0' + constant%10;
				replace_string(text, tmpstring, paramString);
			}
		}

		output += text;
	}


	usedConstants = usedConstantsMask;

	// checks

	// should be no '$' left
	assert( output.find('$') == std::string::npos );

	// debug info
	//char buffer[1000];
	//_snprintf_s( buffer, 1000, "Fragments: %i SavedRegs: %i\n", m_FragmentCount, maxTemps );
	//debug += buffer;
	//for( size_t i = 0; i < savedRegisters.size(); ++i ) {
	//	_snprintf_s( buffer, 1000, "  saved %s [%i..%i] r%i\n", savedRegisters[i].name.c_str(), savedRegisters[i].firstUse, savedRegisters[i].lastUse, savedRegisters[i].regIndex );
	//	debug += buffer;
	//}
}
