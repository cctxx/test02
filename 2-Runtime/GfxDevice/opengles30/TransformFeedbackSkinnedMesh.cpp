 #include "UnityPrefix.h"

#include "Runtime/GfxDevice/opengles30/TransformFeedbackSkinnedMesh.h"

#include "Runtime/Filters/Mesh/Mesh.h"
#include "Runtime/Filters/Mesh/MeshSkinning.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/GfxDevice/opengles30/VBOGLES30.h"
#include "Runtime/GfxDevice/opengles30/AssertGLES30.h"
#include "Runtime/Shaders/GraphicsCaps.h"

// If 1, uses uniform blocks, otherwise fix bone count to 82
#define USE_UNIFORM_BLOCK_FOR_BONES 0

// 1 to use glVertexAttribIPointer for bone indices, 0 to convert to floats
#define USE_INT_ATTRIBS 1

//! Attribute array indices.
enum { TFATTRLOC_POS = 0, TFATTRLOC_NORM = 1, TFATTRLOC_TAN = 2, TFATTRLOC_BONEIDX=3, TFATTRLOC_BONEWEIGHT = 4,  TFATTRLOC_SIZE = 5 };

// Shader programs
enum { TFSHADER_POS = 0, TFSHADER_POSNORM = 1, TFSHADER_POSNORMTAN = 2, TFSHADER_SIZE = 3 };

struct TFShader
{
	TFShader() : program(0), vertShader(0), bonesLocation(0) {}

	// Not a dtor, we're storing them in a map, so delete manually in CleanupTransformFeedbackShaders
	void Release()
	{
		if(program)
			glDeleteProgram(program);
		if(vertShader)
			glDeleteShader(vertShader);
	}

	GLuint	program;
	GLuint	vertShader;
	GLint	bonesLocation;
	GLint	attribLocations[TFATTRLOC_SIZE];
};

// Swap specialization for TFShader
namespace std
{
	template<> void swap(TFShader &a, TFShader &b)
	{
		swap(a.program, b.program);
		swap(a.vertShader, b.vertShader);
		swap(a.bonesLocation, b.bonesLocation);
		swap(a.attribLocations, b.attribLocations);
	}
}

// Map to store shaders. the key is channelMap + (bonesPerVertex << 16)
typedef std::map<UInt32, TFShader> TFShaderMap;

static TFShaderMap tfShaders;

const char *tfShaderAttribNames[TFATTRLOC_SIZE] = {"in_vertex", "in_normal", "in_tangent", "in_boneIndices", "in_boneWeights" };

//! Fragment shader, common to all programs.
static GLuint tfFragShader = 0; 


#define STRINGIFY(x) #x

#if USE_UNIFORM_BLOCK_FOR_BONES
	#define MATRIX_DECL "uniform MtxBlock { vec4 bones[max_bone_count*3]; } Matrices; \n"
	#define BUILD_MATRIX "Matrices.bones[bidx + 0], Matrices.bones[bidx + 1], Matrices.bones[bidx + 2]"
#else
	#define MATRIX_DECL "uniform vec4 bones[max_bone_count*3];\n"
	#define BUILD_MATRIX "bones[bidx + 0], bones[bidx + 1], bones[bidx + 2]"
#endif

// Macro to build shader source.
#define BUILD_SHADER_2(bonecount, indecl, outdecl, skincalc, outcalc ) \
	"#version 300 es\n" \
	"\n" \
	"const int max_bone_count = " STRINGIFY(bonecount) ";\n" \
	"in vec3 in_vertex;\n" \
	indecl \
	"out vec3 out_pos;\n" \
	outdecl \
	"\n" \
	MATRIX_DECL \
	"\n" \
	"mat4 getMatrix(int idx)\n" \
	"{\n"\
	"	int bidx = idx*3;\n" \
	"	return mat4(" BUILD_MATRIX ", vec4(0.0, 0.0, 0.0, 1.0));\n" \
	"}\n"\
	"void main(void)\n" \
	"{\n" \
	"	vec4 inpos = vec4(in_vertex.xyz, 1.0);\n" \
	"	mat4 localToWorldMatrix = \n" \
	skincalc \
	"	out_pos = (inpos * localToWorldMatrix).xyz;\n" \
	"	gl_Position = vec4(out_pos.xyz, 1.0);\n" \
	outcalc \
	"}"

#if USE_INT_ATTRIBS
#define BONEINDEXTYPE1 "int"
#define BONEINDEXTYPE2 "ivec2"
#define BONEINDEXTYPE4 "ivec4"
#else
#define BONEINDEXTYPE1 "float"
#define BONEINDEXTYPE2 "vec2"
#define BONEINDEXTYPE4 "vec4"
#endif

#if USE_UNIFORM_BLOCK_FOR_BONES
#define BUILD_SHADER( indecl, outdecl, skincalc, outcalc ) \
	{\
		BUILD_SHADER_2(32, indecl, outdecl, skincalc, outcalc), \
		BUILD_SHADER_2(64, indecl, outdecl, skincalc, outcalc), \
		BUILD_SHADER_2(128, indecl, outdecl, skincalc, outcalc), \
		BUILD_SHADER_2(256, indecl, outdecl, skincalc, outcalc), \
		BUILD_SHADER_2(512, indecl, outdecl, skincalc, outcalc), \
		BUILD_SHADER_2(1024, indecl, outdecl, skincalc, outcalc) }
#else
// Just one bonecount, store it in first element
#define BUILD_SHADER( indecl, outdecl, skincalc, outcalc ) \
{\
	BUILD_SHADER_2(82, indecl, outdecl, skincalc, outcalc), "", "", "", "", ""\
}
#endif
// Shaders for each input type, and for various max bone counts (32, 64, 128, 256, 512 and 1024) and bone-per-vertex counts (1, 2, 4 bones per vertex supported, sparse array so third slot is empty).
static const char *tfShaderSource[TFSHADER_SIZE][4][6] = {
	// TFSHADER_POS
	{
#define IN_DECL "\n"
#define OUT_DECL "\n"
#define OUT_CALC "\n"
		// 1 bone
		BUILD_SHADER( "in " BONEINDEXTYPE1 " in_boneIndices;\n" IN_DECL,
			OUT_DECL, 
			"		getMatrix(int(in_boneIndices));\n",
			OUT_CALC
			),

		// 2 bones
		BUILD_SHADER( "in " BONEINDEXTYPE2 " in_boneIndices;\n in vec2 in_boneWeights;\n" IN_DECL,
			OUT_DECL, 
			"		getMatrix(int(in_boneIndices.x)) * in_boneWeights[0]  + \n" \
			"		getMatrix(int(in_boneIndices.y)) * in_boneWeights[1] ;\n ",
			OUT_CALC
			),
			// 3 bones
		{"", "", "", "", "", ""},

		// 4 bones
		BUILD_SHADER( "in " BONEINDEXTYPE4 " in_boneIndices;\n in vec4 in_boneWeights;\n" IN_DECL,
					OUT_DECL, 
					"		getMatrix(int(in_boneIndices.x)) * in_boneWeights[0]  + \n" \
					"		getMatrix(int(in_boneIndices.y)) * in_boneWeights[1]  + \n" \
					"		getMatrix(int(in_boneIndices.z)) * in_boneWeights[2]  + \n" \
					"		getMatrix(int(in_boneIndices.w)) * in_boneWeights[3] ;\n",
					OUT_CALC
				)
	}
	,
	// TFSHADER_POSNORM
	{
#undef IN_DECL
#undef OUT_DECL
#undef OUT_CALC
#define IN_DECL "in vec3 in_normal;\n"
#define OUT_DECL "out vec3 out_normal;\n"
#define OUT_CALC "	out_normal = normalize( (vec4(in_normal.xyz, 0.0) * localToWorldMatrix)).xyz;\n"
		// 1 bone
		BUILD_SHADER( "in " BONEINDEXTYPE1 " in_boneIndices;\n" IN_DECL,
			OUT_DECL, 
			"		getMatrix(int(in_boneIndices));\n",
			OUT_CALC
			),

			// 2 bones
			BUILD_SHADER( "in " BONEINDEXTYPE2 " in_boneIndices;\n in vec2 in_boneWeights;\n" IN_DECL,
			OUT_DECL, 
			"		getMatrix(int(in_boneIndices.x)) * in_boneWeights[0]  + \n" \
			"		getMatrix(int(in_boneIndices.y)) * in_boneWeights[1] ;\n ",
			OUT_CALC
			),
			// 3 bones
		{"", "", "", "", "", ""},

			// 4 bones
			BUILD_SHADER( "in " BONEINDEXTYPE4 " in_boneIndices;\n in vec4 in_boneWeights;\n" IN_DECL,
			OUT_DECL, 
			"		getMatrix(int(in_boneIndices.x)) * in_boneWeights[0]  + \n" \
			"		getMatrix(int(in_boneIndices.y)) * in_boneWeights[1]  + \n" \
			"		getMatrix(int(in_boneIndices.z)) * in_boneWeights[2]  + \n" \
			"		getMatrix(int(in_boneIndices.w)) * in_boneWeights[3] ;\n",
			OUT_CALC
			)
	},
// TFSHADER_POSNORMTAN
	{
#undef IN_DECL
#undef OUT_DECL
#undef OUT_CALC
#define IN_DECL "in vec3 in_normal;\n in vec4 in_tangent;\n"
#define OUT_DECL "out vec3 out_normal;\n out vec4 out_tangent;\n"
#define OUT_CALC "	out_normal = normalize( ( vec4(in_normal.xyz, 0.0) * localToWorldMatrix)).xyz;\n" \
				"	out_tangent = vec4( normalize( ( vec4(in_tangent.xyz, 0.0) * localToWorldMatrix)).xyz, in_tangent.w);\n"
		// 1 bone
		BUILD_SHADER( "in " BONEINDEXTYPE1 " in_boneIndices;\n" IN_DECL,
			OUT_DECL, 
			"		getMatrix(int(in_boneIndices));\n",
			OUT_CALC
			),

			// 2 bones
			BUILD_SHADER( "in " BONEINDEXTYPE2 " in_boneIndices;\n in vec2 in_boneWeights;\n" IN_DECL,
			OUT_DECL, 
			"		getMatrix(int(in_boneIndices.x)) * in_boneWeights[0]  + \n" \
			"		getMatrix(int(in_boneIndices.y)) * in_boneWeights[1] ;\n ",
			OUT_CALC
			),
			// 3 bones
		{"", "", "", "", "", ""},

			// 4 bones
			BUILD_SHADER( "in " BONEINDEXTYPE4 " in_boneIndices;\n in vec4 in_boneWeights;\n" IN_DECL,
			OUT_DECL, 
			"		getMatrix(int(in_boneIndices.x)) * in_boneWeights[0]  + \n" \
			"		getMatrix(int(in_boneIndices.y)) * in_boneWeights[1]  + \n" \
			"		getMatrix(int(in_boneIndices.z)) * in_boneWeights[2]  + \n" \
			"		getMatrix(int(in_boneIndices.w)) * in_boneWeights[3] ;\n",
			OUT_CALC
			)
		}

#undef IN_DECL
#undef OUT_DECL
#undef OUT_CALC

};

#undef BUILD_SHADER
#undef BUILD_SHADER_2
#undef STRINGIFY
#undef MATRIX_DECL
#undef BUILD_MATRIX

static const char skinFS[] = 
	"#version 300 es\n"
	"\n"
	"precision lowp float;\n"
	"out vec4 outcol;\n"
	"void main(void) { outcol = vec4(1.0, 1.0, 1.0, 1.0); }\n";
	
enum TfSkinShaderChannel
{
	kTFC_Position		= VERTEX_FORMAT1(Vertex),
	kTFC_Normal			= VERTEX_FORMAT1(Normal),
	kTFC_Tangent		= VERTEX_FORMAT1(Tangent)
};

static GLuint tfTransformFeedback = 0;
static GLuint GetTransformFeedbackObject(void)
{
	if(!tfTransformFeedback)
		GLES_CHK(glGenTransformFeedbacks(1, &tfTransformFeedback));
	return tfTransformFeedback;
}

// Note: we might not support all formats all the time.
static bool DoesVertexFormatQualifyForTransformFeedback(UInt32 shaderChannelsMap)
{
	// Must have position, and if has tangents, must have normals as well.
	bool qualify = (shaderChannelsMap & kTFC_Position) != 0;
	if ((shaderChannelsMap & kTFC_Tangent) != 0)
		qualify &= (shaderChannelsMap & kTFC_Normal) != 0;

	return qualify;

}

static UInt32 roundUpToNextPowerOf2(UInt32 in)
{
	// Round up to nearest power of 2
	// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	in--;
	in |= in >> 1;
	in |= in >> 2;
	in |= in >> 4;
	in |= in >> 8;
	in |= in >> 16;
	in++;
	return in;
}
// Get the bones bit index based on bone count. Assumes bonecount is power of 2
static int getBonesBits(UInt32 boneCount)
{
	// Calculate ln2
	// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn

	static const int MultiplyDeBruijnBitPosition2[32] = 
	{
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};
	UInt32 res = MultiplyDeBruijnBitPosition2[(UInt32)(boneCount * 0x077CB531U) >> 27];

	if(res < 5) // Minimum size is 32 (= 0)
		return 0;

	return res-5; // Adjust so that 32 = 0, 64 = 1 etc.
}

static void print_long_string(std::string in)
{
	int offs = 0;
	int len = in.length();
	const int split = 200;
	do 
	{
		printf_console(in.substr(offs, split).c_str());
		offs+=split;
	} while (offs < len);
	
}

// maxBonesBits == Max bone count: 0 = 32, 1 = 64, etc until 5 = 1024
static TFShader * GetTransformFeedbackShaderProgram(UInt32 shaderChannelsMap, UInt32 bonesPerVertex, UInt32 maxBonesBits)
{
	// Check if already created
	TFShaderMap::iterator itr = tfShaders.find(shaderChannelsMap + (bonesPerVertex << 16) + (maxBonesBits << 19));
	if(itr != tfShaders.end())
	{
		return &(itr->second);
	}

	// There are only 3 different combinations, and they are always in order. We'll just cut the array length at the call site.
	const char *varyings[] = {"out_pos", "out_normal", "out_tangent"};
	GLuint varyingCount = 0;
	int shaderIdx = 0;
	if(shaderChannelsMap & kTFC_Tangent)
	{
		shaderIdx = TFSHADER_POSNORMTAN;
		varyingCount = 3;
	}
	else if(shaderChannelsMap & kTFC_Normal)
	{
		shaderIdx = TFSHADER_POSNORM;
		varyingCount = 2;
	}
	else
	{
		shaderIdx = TFSHADER_POS;
		varyingCount = 1;
	}

	TFShader res;

	GLint status = 0;
	GLint shaderLen = 0;
	const char *code;
	int i;
	// Create the fragment shader if it doesn't exist already
	if(tfFragShader == 0)
	{
		tfFragShader = glCreateShader(GL_FRAGMENT_SHADER);
		shaderLen = strlen(skinFS);
		code = &skinFS[0];
		GLES_CHK(glShaderSource(tfFragShader, 1, &code, &shaderLen));
		GLES_CHK(glCompileShader(tfFragShader));
		glGetShaderiv(tfFragShader, GL_COMPILE_STATUS, &status);
		if(status != GL_TRUE)
		{
			char temp[512] = "";
			GLint len = 512;
			glGetShaderInfoLog(tfFragShader, 512, &len, temp );

			printf_console("ERROR: Unable to compile Transform Feedback fragment shader!\n Error log:\n%s", temp);
			return 0;
		}
	}
	res.program = glCreateProgram();
	res.vertShader = glCreateShader(GL_VERTEX_SHADER);
	shaderLen = strlen(tfShaderSource[shaderIdx][bonesPerVertex-1][maxBonesBits]);
	code = &tfShaderSource[shaderIdx][bonesPerVertex-1][maxBonesBits][0];
	GLES_CHK(glShaderSource(res.vertShader, 1, &code, &shaderLen));
	GLES_CHK(glCompileShader(res.vertShader));
	glGetShaderiv(res.vertShader, GL_COMPILE_STATUS, &status);
	if(status != GL_TRUE)
	{
		char temp[512] = "";
		GLint len = 512;
		glGetShaderInfoLog(res.vertShader, 512, &len, temp );

		printf_console("ERROR: Unable to compile Transform Feedback vertex shader!\n Error log:\n%s", temp);
		print_long_string(code);
		return 0;
	}
	GLES_CHK(glAttachShader(res.program, res.vertShader));
	GLES_CHK(glAttachShader(res.program, tfFragShader));

	GLES_CHK(glTransformFeedbackVaryings(res.program, varyingCount, varyings, GL_INTERLEAVED_ATTRIBS));

	GLES_CHK(glLinkProgram(res.program));

	glGetProgramiv(res.program, GL_LINK_STATUS, &status);
	if(status != GL_TRUE)
	{
		char temp[512] = "";
		GLint len = 512;
		glGetProgramInfoLog(res.program, 512, &len, temp );
		printf_console("ERROR: Unable to link Transform Feedback shader! Error: \n%s", temp);
		print_long_string(code);
		return 0;
	}

#if USE_UNIFORM_BLOCK_FOR_BONES
	res.bonesLocation = glGetUniformBlockIndex(res.program, "MtxBlock");
#else
	res.bonesLocation = glGetUniformLocation(res.program, "bones");
#endif

	// Get the attribute locations. Some of these may be missing so clear the glerror afterwards
	for(i = 0; i < TFATTRLOC_SIZE; i++)
	{
		res.attribLocations[i] = glGetAttribLocation(res.program, tfShaderAttribNames[i]);
	}
	// Clear gl error
	glGetError();

	// Insert into map and return
	return &(tfShaders.insert(std::make_pair(shaderChannelsMap + (bonesPerVertex << 16) + (maxBonesBits << 19), res)).first->second);

}

static void ReleaseShader(std::pair<UInt32, TFShader> it)
{
	it.second.Release();
}

void TransformFeedbackSkinningInfo::CleanupTransformFeedbackShaders(void)
{
	std::for_each(tfShaders.begin(), tfShaders.end(), ReleaseShader);
	tfShaders.clear();

	if(tfTransformFeedback)
	{
		glDeleteTransformFeedbacks(1, &tfTransformFeedback);
		tfTransformFeedback = NULL;
	}
	if(tfFragShader)
	{
		glDeleteShader(tfFragShader);
		tfFragShader = 0;
	}
}

TransformFeedbackSkinningInfo::~TransformFeedbackSkinningInfo()
{
#define DEL_BUFFER(x) if(x != 0) { GLES_CHK(glDeleteBuffers(1, &x)); x = 0; }
	DEL_BUFFER(m_SourceVBO);
#undef DEL_BUFFER
	if(m_MatrixBuffer)
		m_MatrixBuffer->Release();
}

//! Get Vertex size in floats
UInt32 TransformFeedbackSkinningInfo::GetVertexSize()
{
	// Vertex data size
	UInt32 res = (GetStride() / 4);
	// Add skin info size
	if(GetBonesPerVertex() == 1)
		return res + 1; // Index
	else if(GetBonesPerVertex() == 2)
		return res + 4; // 2 indices, 2 weights
	else
		return res + 8; // 4 indices, 4 weights
}

bool TransformFeedbackSkinningInfo::EnsureBuffer()
{
	bool dirty = false;
	if(m_SourceVBO == 0)
	{
		GLES_CHK(glGenBuffers(1, &m_SourceVBO));
		dirty = true;
	}
	GLsizei size = GetVertexSize() * GetVertexCount() * sizeof(float);
	if(m_SourceVBOSize < size)
	{
		GLES_CHK(glBindBuffer(GL_UNIFORM_BUFFER, m_SourceVBO));
		GLES_CHK(glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW));
		m_SourceVBOSize = size;
		dirty = true;
		GLES_CHK(glBindBuffer(GL_UNIFORM_BUFFER, 0));
	}
	return dirty;
}


void TransformFeedbackSkinningInfo::UpdateSourceData(const void *vertData, const BoneInfluence *skinData, bool dirty)
{
	dirty |= EnsureBuffer();

	if(!dirty)
		return;

	std::vector<float> vboData;
	vboData.resize(GetVertexSize() * GetVertexCount());

	float *dest = &vboData[0];
	float *vertsrc = (float *)vertData;
	int vertsize = GetStride() / sizeof(float);
	const BoneInfluence *bonesrc4 = skinData;
	const BoneInfluence2 *bonesrc2 = (BoneInfluence2 *)skinData;
	const int *bonesrc1 = (int *)skinData;

	for(int i = 0; i < GetVertexCount(); i++)
	{
		std::copy(vertsrc, vertsrc+vertsize, dest);
		dest += vertsize;
		vertsrc += vertsize;
		switch(GetBonesPerVertex())
		{
		default:
		case 1:
#if USE_INT_ATTRIBS
			memcpy(dest, bonesrc1, sizeof(int));
			dest++;
			bonesrc1++;
#else
			*(dest++) = (float) *(bonesrc1++);
#endif
			break;
		case 2:
			// Copy weights
			std::copy(&bonesrc2->weight[0], (&bonesrc2->weight[0])+2, dest);
			dest += 2;
#if USE_INT_ATTRIBS
			memcpy(dest, &bonesrc2->boneIndex[0], sizeof(int)*2);
			dest+= 2;
#else
			*(dest++) = (float) bonesrc2->boneIndex[0];
			*(dest++) = (float) bonesrc2->boneIndex[1];
#endif
			bonesrc2++;

			break;
		case 4:
			// Copy weights
			std::copy(&bonesrc4->weight[0], (&bonesrc4->weight[0])+4, dest);
			dest += 4;
#if USE_INT_ATTRIBS
			memcpy(dest, &bonesrc4->boneIndex[0], sizeof(int)*4);
			dest+= 4;
#else
			*(dest++) = (float) bonesrc4->boneIndex[0];
			*(dest++) = (float) bonesrc4->boneIndex[1];
			*(dest++) = (float) bonesrc4->boneIndex[2];
			*(dest++) = (float) bonesrc4->boneIndex[3];
#endif
			bonesrc4++;

			break;
		}
	}
	GLES_CHK(glBindBuffer(GL_UNIFORM_BUFFER, m_SourceVBO));
	GLES_CHK(glBufferSubData(GL_UNIFORM_BUFFER, 0, vboData.size() * sizeof(float), &vboData[0]));
	GLES_CHK(glBindBuffer(GL_UNIFORM_BUFFER, 0));

}


void TransformFeedbackSkinningInfo::UpdateSourceBones( const int boneCount, const Matrix4x4f* cachedPose )
{
	int i;
	int inputSize = boneCount * 4 * 3 * sizeof(float);

#if USE_UNIFORM_BLOCK_FOR_BONES
	m_BoneCount = roundUpToNextPowerOf2(boneCount);
#else
	m_BoneCount = 82;
#endif

	UInt32 realBufSize = m_BoneCount * 4 * 3 * sizeof(float);

	// This basically shouldn't happen but just in case (should be released in SkinMesh)
	if(m_MatrixBuffer)
	{
		m_MatrixBuffer->Release();
	}

	float *dest = NULL;

#if USE_UNIFORM_BLOCK_FOR_BONES
	m_MatrixBuffer = GetBufferManagerGLES30()->AcquireBuffer(realBufSize, GL_DYNAMIC_DRAW);

	if(gGraphicsCaps.gles30.useMapBuffer)
	{
		m_MatrixBuffer->RecreateStorage(realBufSize, GL_DYNAMIC_DRAW);
		dest = (float *)m_MatrixBuffer->Map(0, realBufSize, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
	}
	else
#endif
	{
		m_CachedPose.resize(realBufSize / sizeof(float));
		dest = &m_CachedPose[0];
	}
	
	int realBoneCount = boneCount;
	if(boneCount > m_BoneCount)
		realBoneCount = m_BoneCount;

	for(i = 0; i < realBoneCount; i++)
	{
		Matrix4x4f mat = cachedPose[i];
		mat.Transpose();
		float *src = mat.GetPtr();
		std::copy(src, src+12, dest);
		dest+=12;
	}

#if USE_UNIFORM_BLOCK_FOR_BONES
	if(gGraphicsCaps.gles30.useMapBuffer)
	{
		m_MatrixBuffer->Unmap();
	}
	else
	{
		m_MatrixBuffer->RecreateWithData(realBufSize, GL_DYNAMIC_DRAW, (void *)&m_CachedPose[0]);
	}
	m_MatrixBuffer->RecordUpdate();
#endif
}

// In GfxDeviceGLES30.cpp
void GLSLUseProgramGLES30(UInt32 programID);

void TransformFeedbackSkinningInfo::SkinMesh( bool last )
{

	static GLuint s_WorkaroundTFBuf = 0;

	// Qualcomm, srsly?
	if(s_WorkaroundTFBuf == 0)
	{
		glGenBuffers(1, &s_WorkaroundTFBuf);
		glBindBuffer(GL_COPY_WRITE_BUFFER, s_WorkaroundTFBuf);
		glBufferData(GL_COPY_WRITE_BUFFER, 1024, NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	}

#if USE_UNIFORM_BLOCK_FOR_BONES
	TFShader *shd = GetTransformFeedbackShaderProgram(GetChannelMap(), GetBonesPerVertex(), getBonesBits(m_BoneCount));
#else
	TFShader *shd = GetTransformFeedbackShaderProgram(GetChannelMap(), GetBonesPerVertex(), 0);
#endif

	Assert(shd);

	GLuint tf = GetTransformFeedbackObject();
	GLES3VBO *vbo = static_cast<GLES3VBO *>(GetDestVBO());
	GLuint glvbo = vbo->GetSkinningTargetVBO();

	GLES_CHK(glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, glvbo));


	GLSLUseProgramGLES30(shd->program);

#if USE_UNIFORM_BLOCK_FOR_BONES
	GLES_CHK(glUniformBlockBinding(shd->program, shd->bonesLocation, 0));

	if(m_MatrixBuffer)
		GLES_CHK(glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_MatrixBuffer->GetBuffer()));
#else

	GLES_CHK(glUniform4fv(shd->bonesLocation, m_CachedPose.size() / 4, &m_CachedPose[0]));
#endif

	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, m_SourceVBO));
	GLuint stride = GetVertexSize() * sizeof(float);
	GLES_CHK(glVertexAttribPointer(shd->attribLocations[TFATTRLOC_POS], 3, GL_FLOAT, GL_FALSE, stride, 0));
	GLES_CHK(glEnableVertexAttribArray(shd->attribLocations[TFATTRLOC_POS]));

	GLuint nextoffs = 12;

	if(GetChannelMap() & kTFC_Normal)
	{
		GLES_CHK(glVertexAttribPointer(shd->attribLocations[TFATTRLOC_NORM], 3, GL_FLOAT, GL_FALSE, stride, (void *)nextoffs));
		GLES_CHK(glEnableVertexAttribArray(shd->attribLocations[TFATTRLOC_NORM]));
		nextoffs += 12;
	}
	if(GetChannelMap() & kTFC_Tangent)
	{
		GLES_CHK(glVertexAttribPointer(shd->attribLocations[TFATTRLOC_TAN], 4, GL_FLOAT, GL_FALSE, stride, (void *)nextoffs));
		GLES_CHK(glEnableVertexAttribArray(shd->attribLocations[TFATTRLOC_TAN]));
		nextoffs += 16;
	}

	switch(GetBonesPerVertex())
	{
	default:
	case 1:
#if USE_INT_ATTRIBS
		GLES_CHK(glVertexAttribIPointer(shd->attribLocations[TFATTRLOC_BONEIDX], 1, GL_INT, stride, (void *)nextoffs));	
#else
		GLES_CHK(glVertexAttribPointer(shd->attribLocations[TFATTRLOC_BONEIDX], 1, GL_FLOAT, GL_FALSE, stride, (void *)nextoffs));	
#endif
		GLES_CHK(glEnableVertexAttribArray(shd->attribLocations[TFATTRLOC_BONEIDX]));

		break;
	case 2:
		GLES_CHK(glVertexAttribPointer(shd->attribLocations[TFATTRLOC_BONEWEIGHT], 2, GL_FLOAT, GL_FALSE, stride,(void *)nextoffs));
		GLES_CHK(glEnableVertexAttribArray(shd->attribLocations[TFATTRLOC_BONEWEIGHT]));
		nextoffs += 8;
#if USE_INT_ATTRIBS
		GLES_CHK(glVertexAttribIPointer(shd->attribLocations[TFATTRLOC_BONEIDX], 2, GL_INT, stride, (void *)nextoffs));	
#else
		GLES_CHK(glVertexAttribPointer(shd->attribLocations[TFATTRLOC_BONEIDX], 2, GL_FLOAT, GL_FALSE, stride, (void *)nextoffs));	
#endif
		GLES_CHK(glEnableVertexAttribArray(shd->attribLocations[TFATTRLOC_BONEIDX]));
		break;

	case 4:
		GLES_CHK(glVertexAttribPointer(shd->attribLocations[TFATTRLOC_BONEWEIGHT], 4, GL_FLOAT, GL_FALSE, stride,(void *)nextoffs));
		GLES_CHK(glEnableVertexAttribArray(shd->attribLocations[TFATTRLOC_BONEWEIGHT]));
		nextoffs += 16;
#if USE_INT_ATTRIBS
		GLES_CHK(glVertexAttribIPointer(shd->attribLocations[TFATTRLOC_BONEIDX], 4, GL_INT, stride, (void *)nextoffs));	
#else
		GLES_CHK(glVertexAttribPointer(shd->attribLocations[TFATTRLOC_BONEIDX], 4, GL_FLOAT,GL_FALSE,  stride, (void *)nextoffs));	
#endif
		GLES_CHK(glEnableVertexAttribArray(shd->attribLocations[TFATTRLOC_BONEIDX]));

		break;
	}

	GLES_CHK(glBeginTransformFeedback(GL_POINTS));

	GLES_CHK(glEnable(GL_RASTERIZER_DISCARD));
	GLES_CHK(glDrawArrays(GL_POINTS, 0, GetVertexCount()));
	GLES_CHK(glDisable(GL_RASTERIZER_DISCARD));

	GLES_CHK(glEndTransformFeedback());
	

	GLES_CHK(glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, s_WorkaroundTFBuf));
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));

#if USE_UNIFORM_BLOCK_FOR_BONES

	GLES_CHK(glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0));
	if(m_MatrixBuffer)
	{
		m_MatrixBuffer->RecordRender();
		m_MatrixBuffer->Release();
		m_MatrixBuffer = NULL;
	}
#endif
	InvalidateVertexInputCacheGLES30();
}
