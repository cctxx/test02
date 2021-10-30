#define UNITY_ASSEMBLER
#include "Configuration/PrefixConfigure.h"
#include "Runtime/Utilities/VFPUtility.h"

#if UNITY_SUPPORTS_VFP

.syntax unified

.set device,0
.set device,__arm__
 
.if device

//.code32

.globl _MultiplyMatrices4x4_VFP
.globl _MultiplyMatrixArray4x4_VFP

#if UNITY_ANDROID

.hidden _MultiplyMatrices4x4_VFP
.hidden _MultiplyMatrixArray4x4_VFP

#endif


//===========================================================================================================================================


// void MultiplyMatrices4x4_VFP(const Matrix4x4f* __restrict lhs, const Matrix4x4f* __restrict rhs, Matrix4x4f* __restrict res)
_MultiplyMatrices4x4_VFP:
// r0: A
// r1: B
// r2: dst

vpush		{d8-d15}

mov			ip, r0

// VFP_VECTOR_LENGTH(3)

mov			r0, ip

vldmia.32	r0,  {s8-s23}
vldmia.32	r1!, {s0-s7}

FMULS4		(24,25,26,27,	8,9,10,11,		0,0,0,0)
FMULS4		(28,29,30,31,	8,9,10,11,		4,4,4,4)

FMACS4		(24,25,26,27,	12,13,14,15,	1,1,1,1)
FMACS4		(28,29,30,31,	12,13,14,15,	5,5,5,5)

FMACS4		(24,25,26,27,	16,17,18,19,	2,2,2,2)
FMACS4		(28,29,30,31,	16,17,18,19,	6,6,6,6)

FMACS4		(24,25,26,27,	20,21,22,23,	3,3,3,3)
FMACS4		(28,29,30,31,	20,21,22,23,	7,7,7,7)


vstmia.32	r2!, {s24-s31}
vldmia.32	r1,  {s0-s7}
                
FMULS4		(24,25,26,27,	8,9,10,11,		0,0,0,0)
FMULS4		(28,29,30,31,	8,9,10,11,		4,4,4,4)

FMACS4		(24,25,26,27,	12,13,14,15,	1,1,1,1)
FMACS4		(28,29,30,31,	12,13,14,15,	5,5,5,5)

FMACS4		(24,25,26,27,	16,17,18,19,	2,2,2,2)
FMACS4		(28,29,30,31,	16,17,18,19,	6,6,6,6)

FMACS4		(24,25,26,27,	20,21,22,23,	3,3,3,3)
FMACS4		(28,29,30,31,	20,21,22,23,	7,7,7,7)

vstmia.32	r2,  {s24-s31}

// VFP_VECTOR_LENGTH_ZERO

vpop		{d8-d15}
bx			lr


//===========================================================================================================================================

// void MultiplyMatrixArray4x4_VFP(const Matrix4x4f* arrayA, const Matrix4x4f* arrayB, Matrix4x4f* arrayRes, size_t count)
_MultiplyMatrixArray4x4_VFP:
// r0: A
// r1: B
// r2: dst
// r3: A end

vpush		{d8-d15}

mov			ip, r0

// VFP_VECTOR_LENGTH(3)

mov			r0, ip
add			r3, r0, r3, lsl #6

	 
.align 4
_MultiplyMatrixArray4x4_VFP_loop:
	 
vldmia.32	r0!, {s16-s31}
vldmia.32	r1!, {s0-s7}

FMULS4		(8,9,10,11,		16,17,18,19,	0,0,0,0)
FMULS4		(12,13,14,15,	16,17,18,19,	4,4,4,4)

FMACS4		(8,9,10,11,		20,21,22,23,	1,1,1,1)
FMACS4		(12,13,14,15,	20,21,22,23,	5,5,5,5)

FMACS4		(8,9,10,11,		24,25,26,27,	2,2,2,2)
FMACS4		(12,13,14,15,	24,25,26,27,	6,6,6,6)

FMACS4		(8,9,10,11,		28,29,30,31,	3,3,3,3)
FMACS4		(12,13,14,15,	28,29,30,31,	7,7,7,7)


vldmia.32	r1!, {s0-s7}
vstmia.32	r2!, {s8-s15}
	 
FMULS4		(8,9,10,11,		16,17,18,19,	0,0,0,0)
FMULS4		(12,13,14,15,	16,17,18,19,	4,4,4,4)

FMACS4		(8,9,10,11,		20,21,22,23,	1,1,1,1)
FMACS4		(12,13,14,15,	20,21,22,23,	5,5,5,5)

FMACS4		(8,9,10,11,		24,25,26,27,	2,2,2,2)
FMACS4		(12,13,14,15,	24,25,26,27,	6,6,6,6)

FMACS4		(8,9,10,11,		28,29,30,31,	3,3,3,3)
FMACS4		(12,13,14,15,	28,29,30,31,	7,7,7,7)

vstmia.32	r2!, {s8-s15}

cmp			r0, r3
bcc			_MultiplyMatrixArray4x4_VFP_loop
	 
// VFP_VECTOR_LENGTH_ZERO

vpop		{d8-d15}
bx			lr


.endif

#endif