#define UNITY_ASSEMBLER
#include "Configuration/PrefixConfigure.h"

#if (UNITY_SUPPORTS_NEON && !UNITY_DISABLE_NEON_SKINNING)

.set device,0
.set device,__arm__

.if device

//.code32

.globl _s_SkinVertices_NEON
.globl _s_SkinVertices_NoNormals_NEON
.globl _s_SkinVertices_Tangents_NEON

.globl _s_SkinVertices2Bones_NEON
.globl _s_SkinVertices2Bones_NoNormals_NEON
.globl _s_SkinVertices2Bones_Tangents_NEON

.globl _s_SkinVertices4Bones_NEON
.globl _s_SkinVertices4Bones_NoNormals_NEON
.globl _s_SkinVertices4Bones_Tangents_NEON

#if UNITY_ANDROID
.hidden _s_SkinVertices_NEON
.hidden _s_SkinVertices_NoNormals_NEON
.hidden _s_SkinVertices_Tangents_NEON

.hidden _s_SkinVertices2Bones_NEON
.hidden _s_SkinVertices2Bones_NoNormals_NEON
.hidden _s_SkinVertices2Bones_Tangents_NEON

.hidden _s_SkinVertices4Bones_NEON
.hidden _s_SkinVertices4Bones_NoNormals_NEON
.hidden _s_SkinVertices4Bones_Tangents_NEON
#endif


//===========================================================================================================================================

#define SKIN_POS				1
#define SKIN_POS_NRM			2
#define SKIN_POS_NRM_TAN		3


#define SKIN_2BONES				0
#define SKIN_4BONES				0

_s_SkinVertices_NEON:

#define SKIN_1BONE				SKIN_POS_NRM
#define VERTEX_SZ				24
#define LOOP_NAME				_s_SkinVertices_NEON_loop

#include "MeshSkinningNeon_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_1BONE

_s_SkinVertices_NoNormals_NEON:

#define SKIN_1BONE				SKIN_POS
#define VERTEX_SZ				12
#define LOOP_NAME				_s_SkinVertices_NoNormals_NEON_loop

#include "MeshSkinningNeon_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_1BONE

_s_SkinVertices_Tangents_NEON:

#define SKIN_1BONE				SKIN_POS_NRM_TAN
#define VERTEX_SZ				40
#define LOOP_NAME				_s_SkinVertices_Tangents_NEON_loop

#include "MeshSkinningNeon_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_1BONE

#undef SKIN_4BONES
#undef SKIN_2BONES

//===========================================================================================================================================

#define SKIN_1BONE				0
#define SKIN_4BONES				0

_s_SkinVertices2Bones_NEON:

#define SKIN_2BONES				SKIN_POS_NRM
#define VERTEX_SZ				24
#define LOOP_NAME				_s_SkinVertices2Bones_NEON_loop

#include "MeshSkinningNeon_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_2BONES

_s_SkinVertices2Bones_NoNormals_NEON:

#define SKIN_2BONES				SKIN_POS
#define VERTEX_SZ				12
#define LOOP_NAME				_s_SkinVertices2Bones_NoNormals_NEON_loop

#include "MeshSkinningNeon_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_2BONES

_s_SkinVertices2Bones_Tangents_NEON:

#define SKIN_2BONES				SKIN_POS_NRM_TAN
#define VERTEX_SZ				40
#define LOOP_NAME				_s_SkinVertices2Bones_Tangents_NEON_loop

#include "MeshSkinningNeon_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_2BONES

#undef SKIN_4BONES
#undef SKIN_1BONE


//===========================================================================================================================================

#define SKIN_1BONE				0
#define SKIN_2BONES				0

_s_SkinVertices4Bones_NEON:

#define SKIN_4BONES				SKIN_POS_NRM
#define VERTEX_SZ				24
#define LOOP_NAME				_s_SkinVertices4Bones_NEON_loop

#include "MeshSkinningNeon_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_4BONES

_s_SkinVertices4Bones_NoNormals_NEON:

#define SKIN_4BONES				SKIN_POS
#define VERTEX_SZ				12
#define LOOP_NAME				_s_SkinVertices4Bones_NoNormals_NEON_loop

#include "MeshSkinningNeon_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_4BONES

_s_SkinVertices4Bones_Tangents_NEON:

#define SKIN_4BONES				SKIN_POS_NRM_TAN
#define VERTEX_SZ				40
#define LOOP_NAME				_s_SkinVertices4Bones_Tangents_NEON_loop

#include "MeshSkinningNeon_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_4BONES


#undef SKIN_2BONES
#undef SKIN_1BONE

//===========================================================================================================================================

.endif

#endif
