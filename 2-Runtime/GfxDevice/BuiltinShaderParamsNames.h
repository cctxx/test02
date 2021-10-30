#pragma once


// Fixed function emulation is done in a high-level language where we need variable names etc.
#define GFX_HIGH_LEVEL_FF (GFX_SUPPORTS_OPENGLES20 || GFX_SUPPORTS_OPENGLES30 || GFX_SUPPORTS_D3D11)


// We need the set of "known" glstate parameters for both non-OpenGL rendering runtimes and
// the CgBatch compiler (so that it can report errors for unsupported ones).
// Hence they are in a separate file here.

const char* GetShaderInstanceMatrixParamName(int paramIndex);
const char* GetShaderInstanceVectorParamName(int paramIndex);

const char* GetBuiltinVectorParamName(int paramIndex);
const char* GetBuiltinMatrixParamName(int paramIndex);
const char* GetBuiltinTexEnvParamName(int paramIndex);

bool IsShaderInstanceMatrixParam(const char* name, int* paramIndex=0);
bool IsShaderInstanceVectorParam(const char* name, int* paramIndex);

bool IsVectorBuiltinParam(const char* name, int* paramIndex=0);
bool IsMatrixBuiltinParam(const char* name, int* paramIndex=0);
bool IsTexEnvBuiltinParam(const char* name, int* paramIndex=0);

void InitializeBuiltinShaderParamNames ();
void CleanupBuiltinShaderParamNames ();

bool IsBuiltinArrayName(const char* name);


// Matrices set by unity as "per-instance" data for each object.
enum ShaderBuiltinInstanceMatrixParam {
	kShaderInstanceMatMVP = 0,
	kShaderInstanceMatMV,
	kShaderInstanceMatM,
	kShaderInstanceMatInvM,
	kShaderInstanceMatTransMV,
	kShaderInstanceMatInvTransMV,
	kShaderInstanceMatTexture0,
	kShaderInstanceMatTexture1,
	kShaderInstanceMatTexture2,
	kShaderInstanceMatTexture3,
	kShaderInstanceMatTexture4,
	kShaderInstanceMatTexture5,
	kShaderInstanceMatTexture6,
	kShaderInstanceMatTexture7,
	
#if GFX_HIGH_LEVEL_FF
	kShaderInstanceMatNormalMatrix,
#endif

	kShaderInstanceMatCount
};

enum ShaderBuiltinInstanceVectorParam {
	kShaderInstanceVecScale,

	kShaderInstanceVecCount
};


enum BuiltinShaderMatrixParam {
	kShaderMatView,
	kShaderMatProj,
	kShaderMatViewProj,
	kShaderMatWorldToCamera,
	kShaderMatCameraToWorld,
	kShaderMatWorldToShadow,
	kShaderMatWorldToShadow1,
	kShaderMatWorldToShadow2,
	kShaderMatWorldToShadow3,
	kShaderMatLightmapMatrix,
	kShaderMatProjector,
	kShaderMatProjectorDistance,
	kShaderMatProjectorClip,
	kShaderMatGUIClip,
	kShaderMatLightMatrix,

	kShaderMatCount
};

enum BuiltinShaderVectorParam {
	kShaderVecLight0Diffuse = 0,
	kShaderVecLight1Diffuse,
	kShaderVecLight2Diffuse,
	kShaderVecLight3Diffuse,
	kShaderVecLight4Diffuse,
	kShaderVecLight5Diffuse,
	kShaderVecLight6Diffuse,
	kShaderVecLight7Diffuse,
	kShaderVecLight0Position,
	kShaderVecLight1Position,
	kShaderVecLight2Position,
	kShaderVecLight3Position,
	kShaderVecLight4Position,
	kShaderVecLight5Position,
	kShaderVecLight6Position,
	kShaderVecLight7Position,
	kShaderVecLight0SpotDirection,
	kShaderVecLight1SpotDirection,
	kShaderVecLight2SpotDirection,
	kShaderVecLight3SpotDirection,
	kShaderVecLight4SpotDirection,
	kShaderVecLight5SpotDirection,
	kShaderVecLight6SpotDirection,
	kShaderVecLight7SpotDirection,
	kShaderVecLight0Atten,
	kShaderVecLight1Atten,
	kShaderVecLight2Atten,
	kShaderVecLight3Atten,
	kShaderVecLight4Atten,
	kShaderVecLight5Atten,
	kShaderVecLight6Atten,
	kShaderVecLight7Atten,
	kShaderVecLightModelAmbient,
	kShaderVecWorldSpaceLightPos0,
	kShaderVecLightColor0,
	kShaderVecWorldSpaceCameraPos,
	kShaderVecProjectionParams,
	kShaderVecScreenParams,
	kShaderVecZBufferParams,
	kShaderVecLightPositionRange,
	kShaderVecUnityAmbient, //@TODO: kill it; replace all uses with LightModelAmbient
	kShaderVecLightmapFade,
	kShaderVecShadowOffset0,
	kShaderVecShadowOffset1,
	kShaderVecShadowOffset2,
	kShaderVecShadowOffset3,
	kShaderVecLightShadowData,
	kShaderVecLightShadowBias,
	kShaderVecLightSplitsNear,
	kShaderVecLightSplitsFar,
	kShaderVecShadowSplitSpheres0,
	kShaderVecShadowSplitSpheres1,
	kShaderVecShadowSplitSpheres2,
	kShaderVecShadowSplitSpheres3,
	kShaderVecShadowSplitSqRadii,
	kShaderVecShadowFadeCenterAndType,
	kShaderVecSHAr,
	kShaderVecSHAg,
	kShaderVecSHAb,
	kShaderVecSHBr,
	kShaderVecSHBg,
	kShaderVecSHBb,
	kShaderVecSHC,
	kShaderVecTime,
	kShaderVecSinTime,
	kShaderVecCosTime,
	kShaderVecPiTime,
	kShaderVecDeltaTime,
	kShaderVecVertexLightPosX0,
	kShaderVecVertexLightPosY0,
	kShaderVecVertexLightPosZ0,
	kShaderVecVertexLightAtten0,
	kShaderVecUnityLightmapST,
	kShaderVecUnityFogStart,
	kShaderVecUnityFogEnd,
	kShaderVecUnityFogDensity,
	kShaderVecUnityFogColor,
	kShaderVecColorSpaceGrey,
	kShaderVecNPOTScale,
	kShaderVecCameraWorldClipPlanes0,
	kShaderVecCameraWorldClipPlanes1,
	kShaderVecCameraWorldClipPlanes2,
	kShaderVecCameraWorldClipPlanes3,
	kShaderVecCameraWorldClipPlanes4,
	kShaderVecCameraWorldClipPlanes5,

	// common textures texel size
	kShaderVecWhiteTexelSize,
	kShaderVecBlackTexelSize,
	kShaderVecRedTexelSize,
	kShaderVecGrayTexelSize,
	kShaderVecGreyTexelSize,
	kShaderVecGrayscaleRampTexelSize,
	kShaderVecGreyscaleRampTexelSize,
	kShaderVecBumpTexelSize,
	kShaderVecLightmapTexelSize,
	kShaderVecUnityLightmapTexelSize,
	kShaderVecUnityLightmapIndTexelSize,
	kShaderVecUnityLightmapThirdTexelSize,
	kShaderVecLightTextureB0TexelSize,
	kShaderVecGUIClipTexelSize,
	kShaderVecDitherMaskLODSize,
	kShaderVecRandomRotationTexelSize,


#if GFX_HIGH_LEVEL_FF
	kShaderVecFFColor,
	kShaderVecFFTextureEnvColor0,
	kShaderVecFFTextureEnvColor1,
	kShaderVecFFTextureEnvColor2,
	kShaderVecFFTextureEnvColor3,
	kShaderVecFFTextureEnvColor4,
	kShaderVecFFTextureEnvColor5,
	kShaderVecFFTextureEnvColor6,
	kShaderVecFFTextureEnvColor7,
	kShaderVecFFMatEmission,
	kShaderVecFFMatAmbient,
	kShaderVecFFMatDiffuse,
	kShaderVecFFMatSpecular,
	kShaderVecFFMatShininess,
	kShaderVecFFAlphaTestRef,
	kShaderVecFFFogColor,
	kShaderVecFFFogParams,
#endif

	kShaderVecCount
};

enum BuiltinShaderTexEnvParam {
	kShaderTexEnvWhite = 0,
	kShaderTexEnvBlack,
	kShaderTexEnvRed,
	kShaderTexEnvGray,
	kShaderTexEnvGrey, // TODO: synonims
	kShaderTexEnvGrayscaleRamp,
	kShaderTexEnvGreyscaleRamp, // TODO: synonims
	kShaderTexEnvBump,
	kShaderTexEnvLightmap,
	kShaderTexEnvUnityLightmap,
	kShaderTexEnvUnityLightmapInd,
	kShaderTexEnvUnityLightmapThird,
	kShaderTexEnvDitherMaskLOD,
	kShaderTexEnvRandomRotation,

	kShaderTexEnvCount
};



#define BUILTIN_SHADER_PARAMS_INSTANCE_MATRICES	\
	"glstate_matrix_mvp",						\
	"glstate_matrix_modelview0",				\
	"_Object2World",							\
	"_World2Object",							\
	"glstate_matrix_transpose_modelview0",		\
	"glstate_matrix_invtrans_modelview0",		\
	"glstate_matrix_texture0",					\
	"glstate_matrix_texture1",					\
	"glstate_matrix_texture2",					\
	"glstate_matrix_texture3",					\
	"glstate_matrix_texture4",					\
	"glstate_matrix_texture5",					\
	"glstate_matrix_texture6",					\
	"glstate_matrix_texture7"					\

#define BUILTIN_SHADER_PARAMS_INSTANCE_VECTORS	\
	"unity_Scale"	\


#define BUILTIN_SHADER_PARAMS_MATRICES			\
	"unity_MatrixV",							\
	"glstate_matrix_projection",				\
	"unity_MatrixVP",							\
	"_WorldToCamera",							\
	"_CameraToWorld",							\
	"_World2Shadow",							\
	"_World2Shadow1",							\
	"_World2Shadow2",							\
	"_World2Shadow3",							\
	"unity_LightmapMatrix",						\
	"_Projector",								\
	"_ProjectorDistance",						\
	"_ProjectorClip",							\
	"_GUIClipTextureMatrix",					\
	"_LightMatrix0"								\


#define BUILTIN_SHADER_PARAMS_VECTORS		\
	"unity_LightColor0", \
	"unity_LightColor1", \
	"unity_LightColor2", \
	"unity_LightColor3", \
	"unity_LightColor4", \
	"unity_LightColor5", \
	"unity_LightColor6", \
	"unity_LightColor7", \
	"unity_LightPosition0", \
	"unity_LightPosition1", \
	"unity_LightPosition2", \
	"unity_LightPosition3", \
	"unity_LightPosition4", \
	"unity_LightPosition5", \
	"unity_LightPosition6", \
	"unity_LightPosition7", \
	"unity_SpotDirection0", \
	"unity_SpotDirection1", \
	"unity_SpotDirection2", \
	"unity_SpotDirection3", \
	"unity_SpotDirection4", \
	"unity_SpotDirection5", \
	"unity_SpotDirection6", \
	"unity_SpotDirection7", \
	"unity_LightAtten0", \
	"unity_LightAtten1", \
	"unity_LightAtten2", \
	"unity_LightAtten3", \
	"unity_LightAtten4", \
	"unity_LightAtten5", \
	"unity_LightAtten6", \
	"unity_LightAtten7", \
	"glstate_lightmodel_ambient",			\
	"_WorldSpaceLightPos0",					\
	"_LightColor0",							\
	"_WorldSpaceCameraPos",					\
	"_ProjectionParams",					\
	"_ScreenParams",						\
	"_ZBufferParams",						\
	"_LightPositionRange",					\
	"unity_Ambient",						\
	"unity_LightmapFade",					\
	"_ShadowOffsets0",						\
	"_ShadowOffsets1",						\
	"_ShadowOffsets2",						\
	"_ShadowOffsets3",						\
	"_LightShadowData",						\
	"unity_LightShadowBias",				\
	"_LightSplitsNear",						\
	"_LightSplitsFar",						\
	"unity_ShadowSplitSpheres0",			\
	"unity_ShadowSplitSpheres1",			\
	"unity_ShadowSplitSpheres2",			\
	"unity_ShadowSplitSpheres3",			\
	"unity_ShadowSplitSqRadii",				\
	"unity_ShadowFadeCenterAndType",		\
	"unity_SHAr",							\
	"unity_SHAg",							\
	"unity_SHAb",							\
	"unity_SHBr",							\
	"unity_SHBg",							\
	"unity_SHBb",							\
	"unity_SHC",							\
	"_Time",								\
	"_SinTime",								\
	"_CosTime",								\
	"_PiTime",								\
	"unity_DeltaTime",						\
	"unity_4LightPosX0",					\
	"unity_4LightPosY0",					\
	"unity_4LightPosZ0",					\
	"unity_4LightAtten0",					\
	"unity_LightmapST",						\
	"unity_FogStart",						\
	"unity_FogEnd",							\
	"unity_FogDensity",						\
	"unity_FogColor",						\
	"unity_ColorSpaceGrey",					\
	"unity_NPOTScale",						\
	"unity_CameraWorldClipPlanes0",			\
	"unity_CameraWorldClipPlanes1",			\
	"unity_CameraWorldClipPlanes2",			\
	"unity_CameraWorldClipPlanes3",			\
	"unity_CameraWorldClipPlanes4",			\
	"unity_CameraWorldClipPlanes5",			\
											\
	"white_TexelSize",						\
	"black_TexelSize",						\
	"red_TexelSize",						\
	"gray_TexelSize",						\
	"grey_TexelSize",						\
	"grayscaleRamp_TexelSize",				\
	"greyscaleRamp_TexelSize",				\
	"bump_TexelSize",						\
	"lightmap_TexelSize",					\
	"unity_Lightmap_TexelSize",				\
	"unity_LightmapInd_TexelSize",			\
	"unity_LightmapThird_TexelSize",		\
	"_LightTextureB0_TexelSize",			\
	"_GUIClipTexture_TexelSize",			\
	"_DitherMaskLOD_TexelSize",				\
	"unity_RandomRotation16_TexelSize"

#define BUILTIN_SHADER_PARAMS_VECTORS_SYNONYMS \
	/* aliases for unity_LightColorN */ \
	"glstate_light0_diffuse", \
	"glstate_light1_diffuse", \
	"glstate_light2_diffuse", \
	"glstate_light3_diffuse", \
	/* aliases for unity_LightPositionN */ \
	"glstate_light0_position", \
	"glstate_light1_position", \
	"glstate_light2_position", \
	"glstate_light3_position", \
	/* aliases for unity_LightAttenN */ \
	"glstate_light0_attenuation", \
	"glstate_light1_attenuation", \
	"glstate_light2_attenuation", \
	"glstate_light3_attenuation", \
	/* aliases for unity_SpotDirectionN */ \
	"glstate_light0_spotDirection", \
	"glstate_light1_spotDirection", \
	"glstate_light2_spotDirection", \
	"glstate_light3_spotDirection"




#define BUILTIN_SHADER_PARAMS_TEXENVS	\
	"white",							\
	"black",							\
	"red",								\
	"gray",								\
	"grey",								\
	"grayscaleRamp",					\
	"greyscaleRamp",					\
	"bump",								\
	"lightmap",							\
	"unity_Lightmap",					\
	"unity_LightmapInd", 				\
	"unity_LightmapThird",				\
	"_DitherMaskLOD",					\
	"unity_RandomRotation16"



#if GFX_HIGH_LEVEL_FF

#define BUILTIN_SHADER_PARAMS_INSTANCE_MATRICES_FF	\
	"_glesNormalMatrix"									\


#define BUILTIN_SHADER_PARAMS_VECTORS_FF	\
	"_glesFFColor",								\
	"_glesTextureEnvColor0",					\
	"_glesTextureEnvColor1",					\
	"_glesTextureEnvColor2",					\
	"_glesTextureEnvColor3",					\
	"_glesTextureEnvColor4",					\
	"_glesTextureEnvColor5",					\
	"_glesTextureEnvColor6",					\
	"_glesTextureEnvColor7",					\
	"_glesFrontMaterial.emission",				\
	"_glesFrontMaterial.ambient",				\
	"_glesFrontMaterial.diffuse",				\
	"_glesFrontMaterial.specular",				\
	"_glesFrontMaterial.shininess",				\
	"_glesAlphaTestReference",					\
	"_glesFogColor",							\
	"_glesFogParams"							\

#endif

