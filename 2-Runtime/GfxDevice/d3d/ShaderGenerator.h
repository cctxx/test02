#pragma once
#include <string>

enum ShaderConstant {
	kConstMatrixMVP,	// model*view*proj
	kConstMatrixMV,		// model*view
	kConstMatrixMV_IT,	// model*view inverse transpose
	kConstMatrixTexture,// texture matrix
	kConstAmbient,		// materialEmissive + sceneAmbient * materialAmbient
	kConstColorMatAmbient, // various combos of kConstAmbient, based on color material mode
	kConstLightMisc,	// 0, 4, 1, 0.5
	kConstMatDiffuse,	// material diffuse
	kConstMatSpecular,	// material specular
	kConstLightIndexes, // light start indexes * 4
	kConstCount
};

extern const int kConstantLocations[kConstCount];


struct ShaderFragment
{
	unsigned int inputs;
	unsigned int constants;
	unsigned int dependencies;
	unsigned int options;
	int			temps;
	const char* ins;
	const char* outs;
	const char* text;
};


class ShaderGenerator
{
public:
	enum {
		kMaxShaderFragments = 32,
		kMaxTempRegisters = 12,
		kMaxSavedRegisters = 16,
	};

private:
	struct FragmentData {
		FragmentData() : fragment(NULL), inputNames(NULL), param(0) { }
		FragmentData( const ShaderFragment* f, const char* inames, int p ) : fragment(f), inputNames(inames), param(p) { }
		bool operator==( const FragmentData& rhs ) const {
			return
				fragment==rhs.fragment &&
				param==rhs.param &&
				((inputNames==NULL && rhs.inputNames==NULL) || (inputNames && rhs.inputNames && !strcmp(inputNames, rhs.inputNames)));
		}

		const ShaderFragment* fragment;
		const char* inputNames;
		int	param;
	};

public:

	ShaderGenerator() : m_FragmentCount(0)
	{
	}

	void AddFragment( const ShaderFragment* fragment, const char* inputNames = NULL, int param = -1 );
	void GenerateShader( std::string& output, unsigned int& usedConstants );

private:
	int m_FragmentCount;
	FragmentData m_Fragments[kMaxShaderFragments];
};


extern const ShaderFragment kVS_Pos;
extern const ShaderFragment kVS_Light_Diffuse_Pre;
extern const ShaderFragment kVS_Light_Diffuse_Dir;
extern const ShaderFragment kVS_Light_Diffuse_Point;
extern const ShaderFragment kVS_Light_Diffuse_Spot;
extern const ShaderFragment kVS_Light_Specular_Pre;
extern const ShaderFragment kVS_Light_Specular_Dir;
extern const ShaderFragment kVS_Light_Specular_Point;
extern const ShaderFragment kVS_Light_Specular_Spot;
extern const ShaderFragment kVS_Out_Diffuse_Lighting;
extern const ShaderFragment kVS_Out_Specular_Lighting;
extern const ShaderFragment kVS_Out_Diffuse_Lighting_ColorDiffuseAmbient;
extern const ShaderFragment kVS_Out_Diffuse_Lighting_ColorEmission;
extern const ShaderFragment kVS_Out_Diffuse_VertexColor;
extern const ShaderFragment kVS_Out_Diffuse_White;
extern const ShaderFragment kVS_Load_UV0;
extern const ShaderFragment kVS_Load_UV1;
extern const ShaderFragment kVS_Load_Normal;
extern const ShaderFragment kVS_Normalize_Normal;
extern const ShaderFragment kVS_Out_TexCoord;
extern const ShaderFragment kVS_Out_Matrix2;
extern const ShaderFragment kVS_Out_Matrix3;
extern const ShaderFragment kVS_Temp_CamSpacePos;
extern const ShaderFragment kVS_Temp_CamSpaceN;
extern const ShaderFragment kVS_Temp_CamSpaceRefl;
extern const ShaderFragment kVS_Temp_ObjSpacePos;
extern const ShaderFragment kVS_Temp_SphereMap;
