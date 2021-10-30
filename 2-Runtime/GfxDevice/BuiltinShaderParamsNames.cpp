#include "UnityPrefix.h"
#include "BuiltinShaderParamsNames.h"
#include "Runtime/Utilities/ArrayUtility.h"

namespace
{
	struct ParamNameIndex
	{
		bool operator < (const ParamNameIndex& rhs) const { return strcmp(name, rhs.name) < 0; }
		bool operator == (const ParamNameIndex& rhs) const { return strcmp(name, rhs.name) == 0; }
		const char* name;
		int index;
	};

	class ParamNameLookup
	{
	public:
		ParamNameLookup() {}

		void AddBuiltinNames(const char** names, int count)
		{
			m_Params.resize(count);
			for (int i = 0; i < count; ++i)
			{
				m_Params[i].name = names[i];
				m_Params[i].index = i;
			}
		}
		void AddSynonyms(ParamNameIndex* params, int count)
		{
			m_Params.reserve(m_Params.size() + count);
			for (int i = 0; i < count; ++i)
				m_Params.push_back(params[i]);
		}
		void Sort()
		{
			std::sort(m_Params.begin(), m_Params.end());
		}
		bool Find(const char* name, int* outIndex) const
		{
			ParamNameIndex key = { name, 0 };
			ParamNameArray::const_iterator found = std::find(m_Params.begin(), m_Params.end(), key);
			if (found != m_Params.end())
			{
				if (outIndex)
					*outIndex = found->index;
				return true;
			}
			return false;
		}

	private:
		typedef std::vector<ParamNameIndex> ParamNameArray;
		ParamNameArray m_Params;
	};

	struct ParamNameLookups
	{
		ParamNameLookup instanceMatrices;
		ParamNameLookup instanceVectors;
		ParamNameLookup vectors;
		ParamNameLookup matrices;
		ParamNameLookup texEnvs;
	};

	ParamNameLookups* s_NameLookups;
}


//==============================================================================


inline static const char** GetInstanceMatrixParams()
{
	static const char* lookup[kShaderInstanceMatCount] = {
		BUILTIN_SHADER_PARAMS_INSTANCE_MATRICES

#if GFX_HIGH_LEVEL_FF
		,
		BUILTIN_SHADER_PARAMS_INSTANCE_MATRICES_FF
#endif
	};
	return lookup;
}

inline static const char** GetInstanceVectorParams()
{
	static const char* lookup[kShaderInstanceVecCount] = {
		BUILTIN_SHADER_PARAMS_INSTANCE_VECTORS
	};
	return lookup;
}

inline static const char** GetBuiltinVectorParams()
{
	static const char* lookup[kShaderVecCount] = {
		BUILTIN_SHADER_PARAMS_VECTORS
#if GFX_HIGH_LEVEL_FF
		,
		BUILTIN_SHADER_PARAMS_VECTORS_FF
#endif
	};
	return lookup;
}

static ParamNameIndex s_VectorSynonymMapping[] = {
	{ "glstate_light0_diffuse", kShaderVecLight0Diffuse },
	{ "glstate_light1_diffuse", kShaderVecLight1Diffuse },
	{ "glstate_light2_diffuse", kShaderVecLight2Diffuse },
	{ "glstate_light3_diffuse", kShaderVecLight3Diffuse },
	{ "glstate_light0_position", kShaderVecLight0Position },
	{ "glstate_light1_position", kShaderVecLight1Position },
	{ "glstate_light2_position", kShaderVecLight2Position },
	{ "glstate_light3_position", kShaderVecLight3Position },
	{ "glstate_light0_attenuation", kShaderVecLight0Atten },
	{ "glstate_light1_attenuation", kShaderVecLight1Atten },
	{ "glstate_light2_attenuation", kShaderVecLight2Atten },
	{ "glstate_light3_attenuation", kShaderVecLight3Atten },
	{ "glstate_light0_spotDirection", kShaderVecLight0SpotDirection },
	{ "glstate_light1_spotDirection", kShaderVecLight1SpotDirection },
	{ "glstate_light2_spotDirection", kShaderVecLight2SpotDirection },
	{ "glstate_light3_spotDirection", kShaderVecLight3SpotDirection }
};

inline static const char** GetBuiltinMatrixParams()
{
	static const char* lookup[kShaderMatCount] = {
		BUILTIN_SHADER_PARAMS_MATRICES
	};
	return lookup;
}

static ParamNameIndex s_MatrixSynonymMapping[] = {
	{ "unity_World2Shadow",  kShaderMatWorldToShadow },
	{ "unity_World2Shadow0", kShaderMatWorldToShadow },
	{ "unity_World2Shadow1", kShaderMatWorldToShadow1 },
	{ "unity_World2Shadow2", kShaderMatWorldToShadow2 },
	{ "unity_World2Shadow3", kShaderMatWorldToShadow3 }
};

inline static const char** GetBuiltinTexEnvParams()
{
	static const char* lookup[kShaderTexEnvCount] = {
		BUILTIN_SHADER_PARAMS_TEXENVS
	};
	return lookup;
}


//------------------------------------------------------------------------------

const char* GetShaderInstanceMatrixParamName(int paramIndex)
{
	Assert(paramIndex >= 0 && paramIndex < kShaderInstanceMatCount);
	return GetInstanceMatrixParams()[paramIndex];
}
const char* GetShaderInstanceVectorParamName(int paramIndex)
{
	Assert(paramIndex >= 0 && paramIndex < kShaderInstanceVecCount);
	return GetInstanceVectorParams()[paramIndex];
}
const char* GetBuiltinMatrixParamName(int paramIndex)
{
	Assert(paramIndex >= 0 && paramIndex < kShaderMatCount);
	return GetBuiltinMatrixParams()[paramIndex];
}
const char* GetBuiltinVectorParamName(int paramIndex)
{
	Assert(paramIndex >= 0 && paramIndex < kShaderVecCount);
	return GetBuiltinVectorParams()[paramIndex];
}
const char* GetBuiltinTexEnvParamName(int paramIndex)
{
	Assert(paramIndex >= 0 && paramIndex < kShaderTexEnvCount);
	return GetBuiltinTexEnvParams()[paramIndex];
}

//------------------------------------------------------------------------------

void InitializeBuiltinShaderParamNames ()
{
	Assert (!s_NameLookups);
	// needs to dynamically allocate this due to static initialization order (FastPropertyNames
	// might be initialized before this is etc.)
	s_NameLookups = new ParamNameLookups ();

	s_NameLookups->instanceMatrices.AddBuiltinNames(GetInstanceMatrixParams(), kShaderInstanceMatCount);
	s_NameLookups->instanceMatrices.Sort();

	s_NameLookups->instanceVectors.AddBuiltinNames(GetInstanceVectorParams(), kShaderInstanceVecCount);
	s_NameLookups->instanceVectors.Sort();

	s_NameLookups->vectors.AddBuiltinNames(GetBuiltinVectorParams(), kShaderVecCount);
	s_NameLookups->vectors.AddSynonyms(s_VectorSynonymMapping, ARRAY_SIZE(s_VectorSynonymMapping));
	s_NameLookups->vectors.Sort();

	s_NameLookups->matrices.AddBuiltinNames(GetBuiltinMatrixParams(), kShaderMatCount);
	s_NameLookups->matrices.AddSynonyms(s_MatrixSynonymMapping, ARRAY_SIZE(s_MatrixSynonymMapping));
	s_NameLookups->matrices.Sort();

	s_NameLookups->texEnvs.AddBuiltinNames(GetBuiltinTexEnvParams(), kShaderTexEnvCount);
	s_NameLookups->texEnvs.Sort();
}

void CleanupBuiltinShaderParamNames ()
{
	delete s_NameLookups;
	s_NameLookups = NULL;
}

bool IsShaderInstanceMatrixParam(const char* name, int* paramIndex)
{
	return s_NameLookups->instanceMatrices.Find(name, paramIndex);
}

bool IsShaderInstanceVectorParam(const char* name, int* paramIndex)
{
	return s_NameLookups->instanceVectors.Find(name, paramIndex);
}

bool IsVectorBuiltinParam(const char* name, int* paramIndex)
{
	return s_NameLookups->vectors.Find(name, paramIndex);
}

bool IsMatrixBuiltinParam(const char* name, int* paramIndex)
{
	return s_NameLookups->matrices.Find(name, paramIndex);
}

bool IsTexEnvBuiltinParam(const char* name, int* paramIndex)
{
	return s_NameLookups->texEnvs.Find(name, paramIndex);
}

bool IsBuiltinArrayName(const char* name)
{
	static const char* _BuiltinArrayName[] =
	{
		"glstate_matrix_texture",
		"unity_LightColor",
		"unity_LightPosition",
		"unity_LightAtten",
		"unity_World2Shadow",
		"unity_ShadowSplitSpheres",
		"_ShadowOffsets",
		"unity_CameraWorldClipPlanes"
	};

	static const unsigned _BuiltinArrayCount = sizeof(_BuiltinArrayName) / sizeof(const char*);

	for( unsigned i = 0 ; i < _BuiltinArrayCount ; ++i )
	{
		if( ::strcmp(name, _BuiltinArrayName[i]) == 0 )
			return true;
	}

	return false;
}
