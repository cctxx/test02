#define UNITY_ASSEMBLER
#include "Configuration/PrefixConfigure.h"

#if UNITY_SUPPORTS_NEON

.set device,0
.set device,__arm__

.if device

//.code32

.globl _CopyMatrix_NEON
.globl _TransposeMatrix4x4_NEON
.globl _MultiplyMatrices4x4_NEON
.globl _MultiplyMatrixArray4x4_NEON
.globl _MultiplyMatrixArrayWithBase4x4_NEON

#if UNITY_ANDROID
.hidden _CopyMatrix_NEON
.hidden _TransposeMatrix4x4_NEON
.hidden _MultiplyMatrices4x4_NEON
.hidden _MultiplyMatrixArray4x4_NEON
.hidden _MultiplyMatrixArrayWithBase4x4_NEON
#endif


//===========================================================================================================================================

// void CopyMatrix_NEON(const float* __restrict lhs, float* __restrict res)
_CopyMatrix_NEON:
// r0: src
// r1: dst

vld1.32		{q0,q1}, [r0]!
vld1.32		{q2,q3}, [r0]
vst1.32		{q0,q1}, [r1]!
vst1.32		{q2,q3}, [r1]

bx lr


//===========================================================================================================================================

// void TransposeMatrix4x4_NEON(const Matrix4x4f* __restrict lhs, Matrix4x4f* __restrict res)
_TransposeMatrix4x4_NEON:
// r0: src
// r1: dst

vld4.32		{d0,d2,d4,d6}, [r0]!
vld4.32		{d1,d3,d5,d7}, [r0]
vst1.32		{d0,d1,d2,d3}, [r1]!
vst1.32		{d4,d5,d6,d7}, [r1]

bx lr


//===========================================================================================================================================

// void MultiplyMatrices4x4_NEON(const Matrix4x4f* __restrict lhs, const Matrix4x4f* __restrict rhs, Matrix4x4f* __restrict res)
_MultiplyMatrices4x4_NEON:
// r0: A
// r1: B
// r2: dst

vld1.32		{q0,q1}, [r1]!			// load Brow1-2
vld1.32		{q8}, [r0]!				// load Arow1

// R = Arow1 * Bcol1
vmul.f32	q12, q8, d0[0]
vld1.32		{q2},  [r1]!			// load Brow3

vmul.f32	q13, q8, d2[0]
vld1.32		{q3},  [r1]!			// load Brow4

vmul.f32	q14, q8, d4[0]
vld1.32		{q9},  [r0]!			// load Arow2

vmul.f32	q15, q8, d6[0]
vld1.32		{q10}, [r0]!			// load Arow3

// R += Arow2 * Bcolumn2
vmla.f32	q12, q9, d0[1]
vld1.32		{q11}, [r0]!			// load Arow4

vmla.f32	q13, q9, d2[1]
vmla.f32	q14, q9, d4[1]
vmla.f32	q15, q9, d6[1]

// R += Arow3 * Bcolumn3
vmla.f32	q12, q10, d1[0]
vmla.f32	q13, q10, d3[0]
vmla.f32	q14, q10, d5[0]
vmla.f32	q15, q10, d7[0]

// R += Arow4 * Bcolumn4
vmla.f32	q12, q11, d1[1]
vmla.f32	q13, q11, d3[1]
vmla.f32	q14, q11, d5[1]
vmla.f32	q15, q11, d7[1]

vst1.32		{q12,q13}, [r2]!
vst1.32		{q14,q15}, [r2]!

bx			lr


//===========================================================================================================================================

// void MultiplyMatrixArray4x4_NEON(const Matrix4x4f* arrayA, const Matrix4x4f* arrayB, Matrix4x4f* arrayRes, size_t count)
_MultiplyMatrixArray4x4_NEON:
// r0: A
// r1: B
// r2: dst
// r3: A end

vpush		{d8-d15}
add			r3, r0, r3, lsl #6

vld1.32		{q0,q1}, [r1]!
vld1.32		{q2,q3}, [r1]!
vld1.32		{q8},    [r0]!


.align 4
_MultiplyMatrixArray4x4_NEON_loop:

vmul.f32	q12, q8,   d0[0]
vld1.32		{q9}, [r0]!					// load Arow2

vmul.f32	q13, q8,   d2[0]
vmul.f32	q14, q8,   d4[0]
vmul.f32	q15, q8,   d6[0]


vmla.f32	q12, q9,   d0[1]

vld1.32		{q10},   [r0]!				// load Arow3
vmla.f32	q13, q9,   d2[1]

vld1.32		{q4,q5}, [r1]!				// load B[i+1]
vmla.f32	q14, q9,   d4[1]

vld1.32		{q6,q7}, [r1]!				// load B[i+1]
vmla.f32	q15, q9,   d6[1]

vmla.f32	q12, q10,  d1[0]
vld1.32		{q11},   [r0]!				// load Arow3

vmla.f32	q13, q10,  d3[0]
vmla.f32	q14, q10,  d5[0]
vmla.f32	q15, q10,  d7[0]

vmla.f32	q12, q11,  d1[1]
vld1.32		{q8},    [r0]!				// load A[i+1]row1

vmla.f32	q13, q11,  d3[1]
vmla.f32	q14, q11,  d5[1]
vmla.f32	q15, q11,  d7[1]

vst1.32		{q12,q13}, [r2]!
vst1.32		{q14,q15}, [r2]!

cmp r0, r3
bcs _MultiplyMatrixArray4x4_out


vmul.f32	q12, q8,   d8[0]
vld1.32		{q9},      [r0]!			// load A[i+1]row2

vmul.f32	q13, q8,  d10[0]
vmul.f32	q14, q8,  d12[0]
vmul.f32	q15, q8,  d14[0]

vmla.f32	q12, q9,   d8[1]

vld1.32		{q10},     [r0]!			// load A[i+1]row3
vmla.f32	q13, q9,  d10[1]

vld1.32		{q0,q1},   [r1]!			// load B[i+2]
vmla.f32	q14, q9,  d12[1]

vld1.32		{q2,q3},   [r1]!			// load B[i+2]
vmla.f32	q15, q9,  d14[1]

vmla.f32	q12, q10,  d9[0]
vld1.32		{q11},     [r0]!			// load A[i+1]row4

vmla.f32	q13, q10, d11[0]
vmla.f32	q14, q10, d13[0]
vmla.f32	q15, q10, d15[0]

vmla.f32	q12, q11,  d9[1]
vld1.32		{q8},      [r0]!			// load A[i+2]row1

vmla.f32	q13, q11, d11[1]
vmla.f32	q14, q11, d13[1]
vmla.f32	q15, q11, d15[1]

vst1.32		{q12,q13}, [r2]!
vst1.32		{q14,q15}, [r2]!

cmp r0, r3
bcc _MultiplyMatrixArray4x4_NEON_loop


.align 4
_MultiplyMatrixArray4x4_out:

vpop		{d8-d15}
bx			lr



//===========================================================================================================================================

#define MT_11_1                     \
    vmul.f32    q12, q10,   d8[0] ; \
    vmul.f32    q13, q10,  d10[0] ;

#define MT_11_2                     \
    vmul.f32    q14, q10,  d12[0] ; \
    vmul.f32    q15, q10,  d14[0] ;

#define MT_22_1                     \
    vmla.f32    q12, q11,   d8[1] ; \
    vmla.f32    q13, q11,  d10[1] ;

#define MT_22_2                     \
    vmla.f32    q14, q11,  d12[1] ; \
    vmla.f32    q15, q11,  d14[1] ;

#define MT_33_1                     \
    vmla.f32    q12, q8,    d9[0] ; \
    vmla.f32    q13, q8,   d11[0] ;

#define MT_33_2_44_1                \
    vmla.f32    q14, q8,   d13[0] ; \
    vmla.f32    q12, q9,    d9[1] ; \
    vmla.f32    q13, q9,   d11[1] ; \
    vmla.f32    q15, q8,   d15[0] ;



// void MultiplyMatrixArrayWithBase4x4_NEON( const Matrix4x4f* base, const Matrix4x4f* arrayA, const Matrix4x4f* arrayB, Matrix4x4f* arrayRes, size_t count )
_MultiplyMatrixArrayWithBase4x4_NEON:
// r0: base
// r1: A
// r2: B
// r3: dst
// r4: A end

mov			ip, sp

vpush		{d8-d15}
stmfd		sp!, {r4-r5}


ldr			r4, [ip, #0]
add			r4, r1, r4, lsl #6


vld1.32		{q8},		[r1]!				// load Arow1
vld1.32		{q0,q1},	[r2]!				// load Brow1-2
vld1.32		{q2,q3},	[r2]!				// load Brow3-4
vld1.32		{q10,q11},	[r0]!				// load Mrow1-2

add			r5, r0, #16

// T = Arow1 * Bcol1

vmul.f32	q4, q8,		d0[0]
vmul.f32	q5, q8,		d2[0]


.align 4
_MultiplyMatrixArrayWithBase4x4_NEON_loop:

// T = Arow1 * Bcol1

vld1.32		{q9}, [r1]!						// load Arow2
vmul.f32	q6, q8,		d4[0]
vmul.f32	q7, q8,		d6[0]

// T += Arow2 * Bcol2

vmla.f32	q4, q9,   d0[1]
vmla.f32	q5, q9,   d2[1]

 vld1.32	{q8}, [r1]!						// load Arow3
vmla.f32	q6, q9,   d4[1]
vmla.f32	q7, q9,   d6[1]

// T += Arow3 * Bcol3

vmla.f32	q4, q8,  d1[0]
vmla.f32	q5, q8,  d3[0]

vld1.32		{q9}, [r1]!						// load Arow4

vmla.f32	q6, q8,  d5[0]
vmla.f32	q7, q8,  d7[0]

cmp			r1, r4

// T += Arow4 * Bcol4

vmla.f32	q4, q9,  d1[1]
vmla.f32	q5, q9,  d3[1]

vld1.32		{q8}, [r0]						// load Mrow3
vmla.f32	q6, q9,  d5[1]
vmla.f32	q7, q9,  d7[1]

// R = M * T

vld1.32		{q9}, [r5]						// load Mrow4
MT_11_1

bge _MultiplyMatrixArrayWithBase4x4_NEON_epilogue

vld1.32		{q0}, [r2]!						// load B[i+1]row1
MT_11_2
MT_22_1

vld1.32		{q1}, [r2]!						// load B[i+1]row2
MT_22_2

MT_33_1
vld1.32		{q2}, [r2]!						// load B[i+1]row3

MT_33_2_44_1
vld1.32		{q3}, [r2]!						// load B[i+1]row4

vmla.f32	q14, q9,  d13[1]
vld1.32		{q8}, [r1]!						// load A[i+1]row1

vmla.f32	q15, q9,  d15[1]
vst1.32		{q12,q13}, [r3]!

// interleave T = Arow1 * Bcol1
vmul.f32	q4, q8,		d0[0]
vmul.f32	q5, q8,		d2[0]
vst1.32		{q14,q15}, [r3]!

bcc _MultiplyMatrixArrayWithBase4x4_NEON_loop

.align 4
_MultiplyMatrixArrayWithBase4x4_NEON_epilogue:

MT_11_2
MT_22_1
MT_22_2
MT_33_1
MT_33_2_44_1
vmla.f32    q14, q9,  d13[1]
vmla.f32    q15, q9,  d15[1]
vst1.32     {q12,q13}, [r3]!
vst1.32     {q14,q15}, [r3]!


ldmfd		sp!, {r4-r5}
vpop		{d8-d15}
bx			lr

.endif

#undef MT_11_1
#undef MT_11_2
#undef MT_22_1
#undef MT_22_2
#undef MT_33_1
#undef MT_33_2_44_1

#endif
