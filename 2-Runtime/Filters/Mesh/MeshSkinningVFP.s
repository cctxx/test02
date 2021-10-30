#define UNITY_ASSEMBLER
#include "Configuration/PrefixConfigure.h"
#include "Runtime/Utilities/VFPUtility.h"

#if UNITY_SUPPORTS_VFP

.syntax unified

.set device,0
.set device,__arm__

.if device

//.code32
.globl _s_SkinVertices_VFP
.globl _s_SkinVertices_NoNormals_VFP
.globl _s_SkinVertices_Tangents_VFP

.globl _s_SkinVertices2Bones_VFP
.globl _s_SkinVertices2Bones_NoNormals_VFP
.globl _s_SkinVertices2Bones_Tangents_VFP

.globl _s_SkinVertices4Bones_VFP
.globl _s_SkinVertices4Bones_Copy4Ints_VFP
.globl _s_SkinVertices4Bones_NoNormals_VFP
.globl _s_SkinVertices4Bones_NoNormals_Copy4Ints_VFP
.globl _s_SkinVertices4Bones_Tangents_VFP
.globl _s_SkinVertices4Bones_Tangents_Copy4Ints_VFP

#if UNITY_ANDROID
.hidden _s_SkinVertices_VFP
.hidden _s_SkinVertices_NoNormals_VFP
.hidden _s_SkinVertices_Tangents_VFP

.hidden _s_SkinVertices2Bones_VFP
.hidden _s_SkinVertices2Bones_NoNormals_VFP
.hidden _s_SkinVertices2Bones_Tangents_VFP

.hidden _s_SkinVertices4Bones_VFP
.hidden _s_SkinVertices4Bones_NoNormals_VFP
.hidden _s_SkinVertices4Bones_Tangents_VFP
#endif


//===========================================================================================================================================


#define SKIN_POS				1
#define SKIN_POS_NRM			2
#define SKIN_POS_NRM_TAN		3


#define SKIN_2BONES				0
#define SKIN_4BONES				0

_s_SkinVertices_VFP:

#define SKIN_1BONE				SKIN_POS_NRM
#define VERTEX_SZ				24
#define LOOP_NAME				_s_SkinVertices_VFP_loop

#include "MeshSkinningVFP_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_1BONE

_s_SkinVertices_NoNormals_VFP:

#define SKIN_1BONE				SKIN_POS
#define VERTEX_SZ				12
#define LOOP_NAME				_s_SkinVertices_NoNormals_VFP_loop

#include "MeshSkinningVFP_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_1BONE

_s_SkinVertices_Tangents_VFP:

#define SKIN_1BONE				SKIN_POS_NRM_TAN
#define VERTEX_SZ				40
#define LOOP_NAME				_s_SkinVertices_Tangents_VFP_loop

#include "MeshSkinningVFP_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_1BONE

#undef SKIN_4BONES
#undef SKIN_2BONES


//===========================================================================================================================================

#define SKIN_1BONE				0
#define SKIN_4BONES				0

_s_SkinVertices2Bones_VFP:

#define SKIN_2BONES				SKIN_POS_NRM
#define VERTEX_SZ				24
#define LOOP_NAME				_s_SkinVertices2Bones_VFP_Loop

#include "MeshSkinningVFP_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_2BONES

_s_SkinVertices2Bones_NoNormals_VFP:

#define SKIN_2BONES				SKIN_POS
#define VERTEX_SZ				12
#define LOOP_NAME				_s_SkinVertices2Bones_NoNormals_VFP_Loop

#include "MeshSkinningVFP_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_2BONES

_s_SkinVertices2Bones_Tangents_VFP:

#define SKIN_2BONES				SKIN_POS_NRM_TAN
#define VERTEX_SZ				40
#define LOOP_NAME				_s_SkinVertices2Bones_Tangents_VFP_loop

#include "MeshSkinningVFP_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_2BONES

#undef SKIN_4BONES
#undef SKIN_1BONE

//===========================================================================================================================================

#define SKIN_1BONE				0
#define SKIN_2BONES				0

_s_SkinVertices4Bones_VFP:

#define SKIN_4BONES				SKIN_POS_NRM
#define VERTEX_SZ				24
#define LOOP_NAME				_s_SkinVertices4Bones_VFP_loop

#include "MeshSkinningVFP_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_4BONES

_s_SkinVertices4Bones_NoNormals_VFP:

#define SKIN_4BONES				SKIN_POS
#define VERTEX_SZ				12
#define LOOP_NAME				_s_SkinVertices4Bones_NoNormals_VFP_loop

#include "MeshSkinningVFP_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_4BONES

_s_SkinVertices4Bones_Tangents_VFP:

#define SKIN_4BONES				SKIN_POS_NRM_TAN
#define VERTEX_SZ				40
#define LOOP_NAME				_s_SkinVertices4Bones_Tangents_VFP_loop

#include "MeshSkinningVFP_Loop.h"

#undef LOOP_NAME
#undef VERTEX_SZ
#undef SKIN_4BONES

#undef SKIN_2BONES
#undef SKIN_1BONE

//===========================================================================================================================================

.endif
#endif
