#define UNITY_ASSEMBLER
#include "Configuration/PrefixConfigure.h"
#include "Runtime/Utilities/VFPUtility.h"

#if UNITY_SUPPORTS_VFP

.syntax unified

.set device,0
.set device,__arm__
 
.if device

//.code32


.globl _s_TransformVertices_Strided_XYZ_0_VFP
.globl _s_TransformVertices_Strided_XYZ_1_VFP
.globl _s_TransformVertices_Strided_XYZ_2_VFP
.globl _s_TransformVertices_Strided_XYZ_3_VFP
.globl _s_TransformVertices_Strided_XYZ_4_VFP
.globl _s_TransformVertices_Strided_XYZ_5_VFP

.globl _s_TransformVertices_Strided_XYZN_0_VFP
.globl _s_TransformVertices_Strided_XYZN_1_VFP
.globl _s_TransformVertices_Strided_XYZN_2_VFP
.globl _s_TransformVertices_Strided_XYZN_3_VFP
.globl _s_TransformVertices_Strided_XYZN_4_VFP
.globl _s_TransformVertices_Strided_XYZN_5_VFP

.globl _s_TransformVertices_Strided_XYZNT_0_VFP
.globl _s_TransformVertices_Strided_XYZNT_1_VFP
.globl _s_TransformVertices_Strided_XYZNT_2_VFP
.globl _s_TransformVertices_Strided_XYZNT_3_VFP
.globl _s_TransformVertices_Strided_XYZNT_4_VFP
.globl _s_TransformVertices_Strided_XYZNT_5_VFP

.globl _s_TransformVertices_Sprite_VFP


#if UNITY_ANDROID
.hidden _s_TransformVertices_Strided_XYZ_0_VFP
.hidden _s_TransformVertices_Strided_XYZ_1_VFP
.hidden _s_TransformVertices_Strided_XYZ_2_VFP
.hidden _s_TransformVertices_Strided_XYZ_3_VFP
.hidden _s_TransformVertices_Strided_XYZ_4_VFP
.hidden _s_TransformVertices_Strided_XYZ_5_VFP

.hidden _s_TransformVertices_Strided_XYZN_0_VFP
.hidden _s_TransformVertices_Strided_XYZN_1_VFP
.hidden _s_TransformVertices_Strided_XYZN_2_VFP
.hidden _s_TransformVertices_Strided_XYZN_3_VFP
.hidden _s_TransformVertices_Strided_XYZN_4_VFP
.hidden _s_TransformVertices_Strided_XYZN_5_VFP

.hidden _s_TransformVertices_Strided_XYZNT_0_VFP
.hidden _s_TransformVertices_Strided_XYZNT_1_VFP
.hidden _s_TransformVertices_Strided_XYZNT_2_VFP
.hidden _s_TransformVertices_Strided_XYZNT_3_VFP
.hidden _s_TransformVertices_Strided_XYZNT_4_VFP
.hidden _s_TransformVertices_Strided_XYZNT_5_VFP

.hidden _s_TransformVertices_Sprite_VFP
#endif

#define STRIDED_INPUT	1


#define LOOP_XYZ		1
#define LOOP_XYZN		0
#define LOOP_XYZNT		0
#define LOOP_SPRITE     0

_s_TransformVertices_Strided_XYZ_0_VFP:
#define COPY_DATA_SZ	0
#define LOOP_NAME		TransformVertices_Strided_XYZ_0_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_1_VFP:
#define COPY_DATA_SZ	1
#define LOOP_NAME		TransformVertices_Strided_XYZ_1_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_2_VFP:
#define COPY_DATA_SZ	2
#define LOOP_NAME		TransformVertices_Strided_XYZ_2_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_3_VFP:
#define COPY_DATA_SZ	3
#define LOOP_NAME		TransformVertices_Strided_XYZ_3_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_4_VFP:
#define COPY_DATA_SZ	4
#define LOOP_NAME		TransformVertices_Strided_XYZ_4_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_5_VFP:
#define COPY_DATA_SZ	5
#define LOOP_NAME		TransformVertices_Strided_XYZ_5_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME


#undef LOOP_XYZ
#undef LOOP_XYZN
#undef LOOP_XYZNT
#undef LOOP_SPRITE


#define LOOP_XYZ		0
#define LOOP_XYZN		1
#define LOOP_XYZNT		0
#define LOOP_SPRITE     0


_s_TransformVertices_Strided_XYZN_0_VFP:
#define COPY_DATA_SZ	0
#define LOOP_NAME		TransformVertices_Strided_XYZN_0_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_1_VFP:
#define COPY_DATA_SZ	1
#define LOOP_NAME		TransformVertices_Strided_XYZN_1_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_2_VFP:
#define COPY_DATA_SZ	2
#define LOOP_NAME		TransformVertices_Strided_XYZN_2_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_3_VFP:
#define COPY_DATA_SZ	3
#define LOOP_NAME		TransformVertices_Strided_XYZN_3_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_4_VFP:
#define COPY_DATA_SZ	4
#define LOOP_NAME		TransformVertices_Strided_XYZN_4_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_5_VFP:
#define COPY_DATA_SZ	5
#define LOOP_NAME		TransformVertices_Strided_XYZN_5_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME


#undef LOOP_XYZ
#undef LOOP_XYZN
#undef LOOP_XYZNT
#undef LOOP_SPRITE


#define LOOP_XYZ		0
#define LOOP_XYZN		0
#define LOOP_XYZNT		1
#define LOOP_SPRITE     0


_s_TransformVertices_Strided_XYZNT_0_VFP:
#define COPY_DATA_SZ	0
#define LOOP_NAME		TransformVertices_Strided_XYZNT_0_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_1_VFP:
#define COPY_DATA_SZ	1
#define LOOP_NAME		TransformVertices_Strided_XYZNT_1_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_2_VFP:
#define COPY_DATA_SZ	2
#define LOOP_NAME		TransformVertices_Strided_XYZNT_2_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_3_VFP:
#define COPY_DATA_SZ	3
#define LOOP_NAME		TransformVertices_Strided_XYZNT_3_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_4_VFP:
#define COPY_DATA_SZ	4
#define LOOP_NAME		TransformVertices_Strided_XYZNT_4_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_5_VFP:
#define COPY_DATA_SZ	5
#define LOOP_NAME		TransformVertices_Strided_XYZNT_5_Loop
#include "TransformVertexVFP_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

#undef LOOP_XYZ
#undef LOOP_XYZN
#undef LOOP_XYZNT
#undef LOOP_SPRITE

#define LOOP_XYZ		0
#define LOOP_XYZN		0
#define LOOP_XYZNT		0
#define LOOP_SPRITE     1

_s_TransformVertices_Sprite_VFP:
#define LOOP_NAME TransformVerties_Sprite_Loop
#include "TransformVertexVFP_Loop.h"
#undef LOOP_NAME

#undef LOOP_XYZ
#undef LOOP_XYZN
#undef LOOP_XYZNT
#undef LOOP_SPRITE

#undef STRIDED_INPUT

.endif

#endif
