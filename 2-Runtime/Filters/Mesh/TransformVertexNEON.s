#define UNITY_ASSEMBLER
#include "Configuration/PrefixConfigure.h"

#if UNITY_SUPPORTS_NEON

.set device,0
.set device,__arm__
 
.if device

//.code32


.globl _s_TransformVertices_Strided_XYZ_0_NEON
.globl _s_TransformVertices_Strided_XYZ_1_NEON
.globl _s_TransformVertices_Strided_XYZ_2_NEON
.globl _s_TransformVertices_Strided_XYZ_3_NEON
.globl _s_TransformVertices_Strided_XYZ_4_NEON
.globl _s_TransformVertices_Strided_XYZ_5_NEON

.globl _s_TransformVertices_Strided_XYZN_0_NEON
.globl _s_TransformVertices_Strided_XYZN_1_NEON
.globl _s_TransformVertices_Strided_XYZN_2_NEON
.globl _s_TransformVertices_Strided_XYZN_3_NEON
.globl _s_TransformVertices_Strided_XYZN_4_NEON
.globl _s_TransformVertices_Strided_XYZN_5_NEON

.globl _s_TransformVertices_Strided_XYZNT_0_NEON
.globl _s_TransformVertices_Strided_XYZNT_1_NEON
.globl _s_TransformVertices_Strided_XYZNT_2_NEON
.globl _s_TransformVertices_Strided_XYZNT_3_NEON
.globl _s_TransformVertices_Strided_XYZNT_4_NEON
.globl _s_TransformVertices_Strided_XYZNT_5_NEON

.globl _s_TransformVertices_Sprite_NEON


#define STRIDED_INPUT	1


#define LOOP_XYZ		1
#define LOOP_XYZN		0
#define LOOP_XYZNT		0
#define LOOP_SPRITE		0


_s_TransformVertices_Strided_XYZ_0_NEON:
#define COPY_DATA_SZ	0
#define LOOP_NAME		TransformVertices_Strided_XYZ_0_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_1_NEON:
#define COPY_DATA_SZ	1
#define LOOP_NAME		TransformVertices_Strided_XYZ_1_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_2_NEON:
#define COPY_DATA_SZ	2
#define LOOP_NAME		TransformVertices_Strided_XYZ_2_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_3_NEON:
#define COPY_DATA_SZ	3
#define LOOP_NAME		TransformVertices_Strided_XYZ_3_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_4_NEON:
#define COPY_DATA_SZ	4
#define LOOP_NAME		TransformVertices_Strided_XYZ_4_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZ_5_NEON:
#define COPY_DATA_SZ	5
#define LOOP_NAME		TransformVertices_Strided_XYZ_5_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME


#undef LOOP_XYZ
#undef LOOP_XYZN
#undef LOOP_XYZNT
#undef LOOP_SPRITE


#define LOOP_XYZ		0
#define LOOP_XYZN		1
#define LOOP_XYZNT		0
#define LOOP_SPRITE		0


_s_TransformVertices_Strided_XYZN_0_NEON:
#define COPY_DATA_SZ	0
#define LOOP_NAME		TransformVertices_Strided_XYZN_0_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_1_NEON:
#define COPY_DATA_SZ	1
#define LOOP_NAME		TransformVertices_Strided_XYZN_1_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_2_NEON:
#define COPY_DATA_SZ	2
#define LOOP_NAME		TransformVertices_Strided_XYZN_2_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_3_NEON:
#define COPY_DATA_SZ	3
#define LOOP_NAME		TransformVertices_Strided_XYZN_3_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_4_NEON:
#define COPY_DATA_SZ	4
#define LOOP_NAME		TransformVertices_Strided_XYZN_4_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZN_5_NEON:
#define COPY_DATA_SZ	5
#define LOOP_NAME		TransformVertices_Strided_XYZN_5_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME


#undef LOOP_XYZ
#undef LOOP_XYZN
#undef LOOP_XYZNT
#undef LOOP_SPRITE


#define LOOP_XYZ		0
#define LOOP_XYZN		0
#define LOOP_XYZNT		1
#define LOOP_SPRITE		0


_s_TransformVertices_Strided_XYZNT_0_NEON:
#define COPY_DATA_SZ	0
#define LOOP_NAME		TransformVertices_Strided_XYZNT_0_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_1_NEON:
#define COPY_DATA_SZ	1
#define LOOP_NAME		TransformVertices_Strided_XYZNT_1_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_2_NEON:
#define COPY_DATA_SZ	2
#define LOOP_NAME		TransformVertices_Strided_XYZNT_2_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_3_NEON:
#define COPY_DATA_SZ	3
#define LOOP_NAME		TransformVertices_Strided_XYZNT_3_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_4_NEON:
#define COPY_DATA_SZ	4
#define LOOP_NAME		TransformVertices_Strided_XYZNT_4_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME

_s_TransformVertices_Strided_XYZNT_5_NEON:
#define COPY_DATA_SZ	5
#define LOOP_NAME		TransformVertices_Strided_XYZNT_5_Loop
#include "TransformVertexNEON_Loop.h"
#undef COPY_DATA_SZ
#undef LOOP_NAME


#undef LOOP_XYZ
#undef LOOP_XYZN
#undef LOOP_XYZNT
#undef LOOP_SPRITE

#define LOOP_XYZ		0
#define LOOP_XYZN		0
#define LOOP_XYZNT		0
#define LOOP_SPRITE		1

_s_TransformVertices_Sprite_NEON:
#define LOOP_NAME		TransformVertices_Sprite_Loop
#include "TransformVertexNEON_Loop.h"
#undef LOOP_NAME

#undef LOOP_XYZ
#undef LOOP_XYZN
#undef LOOP_XYZNT
#undef LOOP_SPRITE

#undef STRIDED_INPUT

.endif

#endif