
// defines
// SKIN_1BONE
// SKIN_2BONES
// SKIN_4BONES
// LOOP_NAME
// VERTEX_SZ

// skin types
// SKIN_POS
// SKIN_POS_NRM
// SKIN_POS_NRM_TAN



//r0: const void* bones4x4
//r1: const void* srcVertData
//r2: const void* srcVertDataEnd
//r3: const BoneInfluence4* srcBoneInfluence4
//[sp+0] -> r4: const void* dstVertData

// r5, r6:	index
// r7:		matrix address
// r8:		12 (offset for vector3)

// q0 <- output: pos
// q1 <- output: nrm
// q2 <- output: tan
// q3 <- input: pos
// q4 <- input: nrm
// q5 <- input: tan
// d11,d12 <- weights
// q12-q15 (blended matrix)
// q8-q11 (cur matrix)


// input:
// d6[0], d6[1], d7[0] 			<- pos
// d7[1], d8[0], d8[1] 			<- nrm
// d9[0], d9[1], d10[0], d10[1] <- tan
// q3 <- pos.x, pos.y, pos.z, nrm.x
// q4 <- nrm.y, nrm.z, tan.x, tan.y
// q5 <- tan.z, tan.w, w0, w1


//===========================================================================================================================================
//
// Common

#define CALC_POS_1 		vmul.f32	q0, q12, d6[0]
#define CALC_POS_2 		vmla.f32	q0, q13, d6[1]
#define CALC_POS_3 		vmla.f32	q0, q14, d7[0]
#define CALC_POS_4 		vadd.f32	q0, q15

#define STORE_POS		vst1.32		{d0, d1}, [r4], r8

#if		(SKIN_1BONE == SKIN_POS_NRM) || (SKIN_1BONE == SKIN_POS_NRM_TAN)		\
	||	(SKIN_2BONES == SKIN_POS_NRM) || (SKIN_2BONES == SKIN_POS_NRM_TAN)		\
	||	(SKIN_4BONES == SKIN_POS_NRM) || (SKIN_4BONES == SKIN_POS_NRM_TAN)

	#define LOAD_POS_NRM 	vld1.32 	{d6, d7, d8}, [r1, :64]!
	#define STORE_NRM		vst1.32		{d2, d3}, [r4], r8
	#define CALC_NRM_1		vmul.f32	q1, q12, d7[1]
	#define CALC_NRM_2 		vmla.f32	q1, q13, d8[0]
	#define CALC_NRM_3 		vmla.f32	q1, q14, d8[1]
#else
	#define LOAD_POS_NRM 	vld1.32 	{d6, d7}, [r1], r8
	#define STORE_NRM
	#define CALC_NRM_1
	#define CALC_NRM_2
	#define CALC_NRM_3
#endif

#if	(SKIN_1BONE == SKIN_POS_NRM_TAN) || (SKIN_2BONES == SKIN_POS_NRM_TAN) || (SKIN_4BONES == SKIN_POS_NRM_TAN)
	#define LOAD_TAN		vld1.32		{d9, d10}, [r1, :64]!
	#define STORE_TAN		vst1.32		{d4, d5}, [r4]!
	#define CALC_TAN_1 		vmul.f32	q2, q12, d9[0]
	#define CALC_TAN_2 		vmla.f32	q2, q13, d9[1]
	#define CALC_TAN_3 		vmla.f32	q2, q14, d10[0]
	#define CALC_TAN_4 		vmov.f32	s11, s21
#else
	#define LOAD_TAN
	#define STORE_TAN
	#define CALC_TAN_1
	#define CALC_TAN_2
	#define CALC_TAN_3
	#define CALC_TAN_4
#endif

// right after vertex-data will be copy-data stream, so be careful to not overwrite anything
#if	(SKIN_1BONE == SKIN_POS) || (SKIN_2BONES == SKIN_POS) || (SKIN_4BONES == SKIN_POS)
#define STORE_POS_LAST1		vst1.32		{d0}, [r4]!
#define STORE_POS_LAST2		vst1.32		{d1[0]}, [r4]!
#else
#define STORE_POS_LAST1		STORE_POS
#define STORE_POS_LAST2
#endif

#if	(SKIN_1BONE == SKIN_POS_NRM) || (SKIN_2BONES == SKIN_POS_NRM) || (SKIN_4BONES == SKIN_POS_NRM)
#define STORE_NRM_LAST1		vst1.32		{d2}, [r4]!
#define STORE_NRM_LAST2		vst1.32		{d3[0]}, [r4]!
#else
#define STORE_NRM_LAST1		STORE_NRM
#define STORE_NRM_LAST2
#endif

#define __NAME_EPILOGUE(x) x ## EPILOGUE
#define _NAME_EPILOGUE(x) __NAME_EPILOGUE(x)
#define LOOP_EPILOGUE _NAME_EPILOGUE(LOOP_NAME)



#if (SKIN_1BONE == SKIN_POS) || (SKIN_1BONE == SKIN_POS_NRM) || (SKIN_1BONE == SKIN_POS_NRM_TAN)
	#define LOAD_M_12		vld1.32		{q12,q13}, [r7,:128]!
	#define LOAD_M_34		vld1.32		{q14,q15}, [r7,:128]
#else
	#define LOAD_M_12		vld1.32		{q8,q9}, [r7,:128]!
	#define LOAD_M_34		vld1.32		{q10,q11}, [r7,:128]
#endif

#define WEIGHT_MATRIX_1(op,r)	op.f32 q12, q8, r
#define WEIGHT_MATRIX_2(op,r)	op.f32 q13, q9, r
#define WEIGHT_MATRIX_3(op,r)	op.f32 q14, q10, r
#define WEIGHT_MATRIX_4(op,r)	op.f32 q15, q11, r

#define WEIGHT_M0_1 WEIGHT_MATRIX_1(vmul, d11[0])
#define WEIGHT_M0_2 WEIGHT_MATRIX_2(vmul, d11[0])
#define WEIGHT_M0_3 WEIGHT_MATRIX_3(vmul, d11[0])
#define WEIGHT_M0_4 WEIGHT_MATRIX_4(vmul, d11[0])

#define WEIGHT_M1_1 WEIGHT_MATRIX_1(vmla, d11[1])
#define WEIGHT_M1_2 WEIGHT_MATRIX_2(vmla, d11[1])
#define WEIGHT_M1_3 WEIGHT_MATRIX_3(vmla, d11[1])
#define WEIGHT_M1_4 WEIGHT_MATRIX_4(vmla, d11[1])

#define WEIGHT_M2_1 WEIGHT_MATRIX_1(vmla, d12[0])
#define WEIGHT_M2_2 WEIGHT_MATRIX_2(vmla, d12[0])
#define WEIGHT_M2_3 WEIGHT_MATRIX_3(vmla, d12[0])
#define WEIGHT_M2_4 WEIGHT_MATRIX_4(vmla, d12[0])

#define WEIGHT_M3_1 WEIGHT_MATRIX_1(vmla, d12[1])
#define WEIGHT_M3_2 WEIGHT_MATRIX_2(vmla, d12[1])
#define WEIGHT_M3_3 WEIGHT_MATRIX_3(vmla, d12[1])
#define WEIGHT_M3_4 WEIGHT_MATRIX_4(vmla, d12[1])


//===========================================================================================================================================
//
// 1 bone skinning

#if (SKIN_1BONE == SKIN_POS) || (SKIN_1BONE == SKIN_POS_NRM) || (SKIN_1BONE == SKIN_POS_NRM_TAN)

mov			ip, sp

vpush		{d8-d10}
stmfd		sp!, {r4-r8}

ldr			r4, [ip, #0]
mov			r8, #12

									ldr		r5, [r3], #4
									add		r7, r0, r5, lsl #6

LOOP_NAME:



LOAD_M_12
LOAD_M_34


LOAD_POS_NRM
LOAD_TAN

CALC_POS_1
CALC_NRM_1
CALC_TAN_1

									cmp 	r1, r2
									pld		[r1, #256]

CALC_POS_2
CALC_NRM_2
CALC_TAN_2

									ldrcc	r5, [r3], #4
									add		r7, r0, r5, lsl #6

CALC_POS_3
CALC_NRM_3
CALC_TAN_3

									pld		[r7]

CALC_POS_4
CALC_TAN_4

beq LOOP_EPILOGUE

STORE_POS
STORE_NRM
STORE_TAN

bcc LOOP_NAME

LOOP_EPILOGUE:
STORE_POS_LAST1
STORE_POS_LAST2
STORE_NRM_LAST1
STORE_NRM_LAST2
STORE_TAN


ldmfd		sp!, {r4-r8}
vpop		{d8-d10}

bx			lr


//===========================================================================================================================================
//
// 2 bones skinning

#elif (SKIN_2BONES == SKIN_POS || SKIN_2BONES == SKIN_POS_NRM || SKIN_2BONES == SKIN_POS_NRM_TAN)

mov			ip, sp

vpush		{d8-d11}
stmfd		sp!, {r4,r5,r6,r7,r8,r10}

ldr			r4, [ip, #0]

vld1.32		{d11}, [r3,:64]!			// wgt ->
ldmia		r3!, {r5-r6}				// idx ->

add			r7, r0, r5, lsl #6			// M0 ..
LOAD_M_12								// M0
WEIGHT_M0_1
WEIGHT_M0_2

LOAD_M_34								// M0
add			r7, r0, r6, lsl #6			// M1 ..
WEIGHT_M0_3
WEIGHT_M0_4

LOAD_M_12								// M1
WEIGHT_M1_1
WEIGHT_M1_2

ldr			r5, [r3, #8]				// idx0

mov			r8,  #12
sub			r10, r2, #VERTEX_SZ

LOAD_M_34								// M1

WEIGHT_M1_3

.align 4
LOOP_NAME:

																cmp			r1, r10

																add			r7, r0, r5, lsl #6			// M0 ..
																ldrcc		r6, [r3, #12]				// idx1
LOAD_POS_NRM

WEIGHT_M1_4

LOAD_TAN

CALC_POS_1
LOAD_M_12								// M0
																cmp			r1, r10
CALC_NRM_1
CALC_TAN_1
vld1.32		{d11}, [r3,:64]				// wgt ->

WEIGHT_M0_1
																pld			[r1,#256]

CALC_POS_2
LOAD_M_34								// M0
																add			r7, r0, r6, lsl #6			// M1 ..
CALC_NRM_2
CALC_TAN_2
																ldrcc		r5, [r3, #24]				// idx0
WEIGHT_M0_2
CALC_POS_3

																cmp			r1, r2
CALC_NRM_3
CALC_TAN_3
LOAD_M_12								// M1


WEIGHT_M0_3

CALC_POS_4
CALC_TAN_4

WEIGHT_M0_4
LOAD_M_34								// M1

beq LOOP_EPILOGUE

WEIGHT_M1_1
STORE_POS

WEIGHT_M1_2
STORE_NRM
																add			r3, r3, #16
WEIGHT_M1_3
STORE_TAN

bcc LOOP_NAME

LOOP_EPILOGUE:
STORE_POS_LAST1
STORE_POS_LAST2
STORE_NRM_LAST1
STORE_NRM_LAST2
STORE_TAN


ldmfd		sp!, {r4,r5,r6,r7,r8,r10}
vpop		{d8-d11}
bx			lr


//===========================================================================================================================================
//
// 4 bones skinning

#elif (SKIN_4BONES == SKIN_POS || SKIN_4BONES == SKIN_POS_NRM || SKIN_4BONES == SKIN_POS_NRM_TAN)


mov			ip, sp

vpush		{d8-d12}
stmfd		sp!, {r4-r8}

ldr			r4, [ip, #0]

vld1.32		{d11,d12}, [r3,:128]!	// wgt ->
ldmia		r3!, {r5-r6}			// idx' ->

add			r7, r0, r5, lsl #6		// M0 ..
LOAD_M_12							// M0
LOAD_M_34							// M0

mov			r8, #12

.align 4
LOOP_NAME:

WEIGHT_M0_1
LOAD_POS_NRM

WEIGHT_M0_2
LOAD_TAN
													add			r7, r0, r6, lsl #6		// M1 ..


WEIGHT_M0_3
LOAD_M_12							// M1

WEIGHT_M0_4
LOAD_M_34							// M1

WEIGHT_M1_1
													ldmia		r3!, {r5-r6}			// idx'' ->

WEIGHT_M1_2
													add			r7, r0, r5, lsl #6		// M2 ..
													cmp			r1, r2

WEIGHT_M1_3
LOAD_M_12							// M2

WEIGHT_M1_4
													pld			[r3, #256]
LOAD_M_34							// M2

WEIGHT_M2_1
													add			r7, r0, r6, lsl #6		// M3 ..
WEIGHT_M2_2
WEIGHT_M2_3
LOAD_M_12							// M3
WEIGHT_M2_4

LOAD_M_34							// M3
WEIGHT_M3_1
WEIGHT_M3_2
WEIGHT_M3_3
WEIGHT_M3_4
													pld			[r1, #256]

CALC_POS_1
vld1.32		{d11,d12}, [r3,:128]!	// wgt ->

CALC_NRM_1
CALC_TAN_1
													ldmcc		r3!, {r5-r6}			// idx ->

CALC_POS_2
													add			r7, r0, r5, lsl #6		// M0 ..
CALC_NRM_2
CALC_TAN_2
vldmia		r7, {q8-q11}			// M0 ->

CALC_POS_3
CALC_NRM_3
CALC_TAN_3

CALC_POS_4
CALC_TAN_4

beq LOOP_EPILOGUE

STORE_POS
STORE_NRM
STORE_TAN

bcc LOOP_NAME

LOOP_EPILOGUE:
STORE_POS_LAST1
STORE_POS_LAST2
STORE_NRM_LAST1
STORE_NRM_LAST2
STORE_TAN


ldmfd		sp!, {r4-r8}
vpop		{d8-d12}
bx			lr


//===========================================================================================================================================

#endif

#undef __NAME_EPILOGUE
#undef _NAME_EPILOGUE
#undef LOOP_EPILOGUE
#undef CALC_POS_1
#undef CALC_POS_2
#undef CALC_POS_3
#undef STORE_POS
#undef STORE_POS_LAST1
#undef STORE_POS_LAST2
#undef LOAD_POS_NRM
#undef STORE_NRM
#undef STORE_NRM_LAST1
#undef STORE_NRM_LAST2
#undef CALC_NRM_1
#undef CALC_NRM_2
#undef CALC_NRM_3
#undef LOAD_TAN
#undef STORE_TAN
#undef CALC_TAN_1
#undef CALC_TAN_2
#undef CALC_TAN_3
#undef CALC_TAN_4
#undef LOAD_M_12
#undef LOAD_M_34
#undef WEIGHT_MATRIX_1
#undef WEIGHT_MATRIX_2
#undef WEIGHT_MATRIX_3
#undef WEIGHT_MATRIX_4
#undef WEIGHT_M0_1
#undef WEIGHT_M0_2
#undef WEIGHT_M0_3
#undef WEIGHT_M0_4
#undef WEIGHT_M1_1
#undef WEIGHT_M1_2
#undef WEIGHT_M1_3
#undef WEIGHT_M1_4
#undef WEIGHT_M2_1
#undef WEIGHT_M2_2
#undef WEIGHT_M2_3
#undef WEIGHT_M2_4
#undef WEIGHT_M3_1
#undef WEIGHT_M3_2
#undef WEIGHT_M3_3
#undef WEIGHT_M3_4
