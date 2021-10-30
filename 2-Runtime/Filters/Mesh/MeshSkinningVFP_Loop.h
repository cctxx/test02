
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

// s0,s1,s2			<- output: pos
// s3,s4,s5			<- output: nrm
// s6,s7,s8,s9		<- output: tan
// s10,s11,s12		<- input: pos
// s13,s14,s15		<- input: nrm
// s16,s17,s18,s19	<- input: tan
// s20-s31			<- matrix [3x4] last row loaded directly to output pos

//===========================================================================================================================================
//
// Common

#define CALC_POS_2 		FMACS3		(0,1,2, 20,21,22, 10,10,10)
#define CALC_POS_3 		FMACS3		(0,1,2, 24,25,26, 11,11,11)
#define CALC_POS_4 		FMACS3		(0,1,2, 28,29,30, 12,12,12)


#if		(SKIN_1BONE == SKIN_POS_NRM) || (SKIN_1BONE == SKIN_POS_NRM_TAN)		\
	||	(SKIN_2BONES == SKIN_POS_NRM) || (SKIN_2BONES == SKIN_POS_NRM_TAN)		\
	||	(SKIN_4BONES == SKIN_POS_NRM) || (SKIN_4BONES == SKIN_POS_NRM_TAN)

	#define LOAD_POS_NRM 	vldmia.32	r1!, {s10-s15}
	#define STORE_POS_NRM	vstmia.32	r4!, {s0-s5}
	#define CALC_NRM_1		FMULS3		(3,4,5, 20,21,22, 13,13,13)
	#define CALC_NRM_2 		FMACS3		(3,4,5, 24,25,26, 14,14,14)
	#define CALC_NRM_3 		FMACS3		(3,4,5, 28,29,30, 15,15,15)
#else
	#define LOAD_POS_NRM 	vldmia.32	r1!, {s10-s12}
	#define STORE_POS_NRM	vstmia.32	r4!, {s0-s2}
	#define CALC_NRM_1
	#define CALC_NRM_2
	#define CALC_NRM_3
#endif

#if	(SKIN_1BONE == SKIN_POS_NRM_TAN) || (SKIN_2BONES == SKIN_POS_NRM_TAN) || (SKIN_4BONES == SKIN_POS_NRM_TAN)
	#define LOAD_TAN		vldmia.32	r1!, {s16-s19}
	#define STORE_TAN		vstmia.32	r4!, {s6-s9}
	#define CALC_TAN_1 		FMULS3		(6,7,8, 20,21,22, 16,16,16)
	#define CALC_TAN_2 		FMACS3		(6,7,8, 24,25,26, 17,17,17)
	#define CALC_TAN_3 		FMACS3		(6,7,8, 28,29,30, 18,18,18)
	#define CALC_TAN_4 		fcpys		s9, s19
#else
	#define LOAD_TAN
	#define STORE_TAN
	#define CALC_TAN_1
	#define CALC_TAN_2
	#define CALC_TAN_3
	#define CALC_TAN_4
#endif




//===========================================================================================================================================
//
// 1 bone skinning

#if (SKIN_1BONE == SKIN_POS) || (SKIN_1BONE == SKIN_POS_NRM) || (SKIN_1BONE == SKIN_POS_NRM_TAN)

mov			ip, sp
vpush		{d7-d15}
stmfd		sp!, {r4,r5,r6,r7,r8,r10,r11}

ldr			r4, [ip, #0]

ldr			r5, [r3], #4
add			r5, r0, r5, lsl #6
add			r6, r5, #48

vldmia.32	r6,  {s0-s2}
vldmia.32	r5!, {s20-s23}
vldmia.32	r5!, {s24-s27}

.align 4
LOOP_NAME:

LOAD_POS_NRM

CALC_POS_2
CALC_NRM_1
												ldr			r6, [r3], #4					// next matrix index
vldmia.32	r5, {s28-s30}					// bone matrix
												add			r5, r0, r6, lsl #6				// next matrix addr


CALC_POS_3
CALC_NRM_2

LOAD_TAN
												add			r6, r5, #48
												cmp			r1, r2

CALC_TAN_1
												vldmiacc.32	r5!, {s20-s23}					// next bone matrix


CALC_POS_4

CALC_TAN_2
CALC_NRM_3
												vldmiacc.32	r5!, {s24-s27}					// next bone matrix

CALC_TAN_3
CALC_TAN_4

												pld [r1, #1024]


STORE_POS_NRM
STORE_TAN

												vldmiacc.32	r6,  {s0-s2}

bcc			LOOP_NAME

ldmfd		sp!, {r4,r5,r6,r7,r8,r10,r11}
vpop		{d7-d15}
bx			lr


//===========================================================================================================================================

#elif (SKIN_2BONES == SKIN_POS) || (SKIN_2BONES == SKIN_POS_NRM) || (SKIN_2BONES == SKIN_POS_NRM_TAN)

mov			ip, sp
vpush		{d7-d15}
stmfd		sp!, {r4,r5,r6,r7,r8,r10,r11}

ldr			r4, [ip, #0]


.align 4
LOOP_NAME:

vldmia.32	r3!, {s3,s4}						// w
													ldmia		r3!, {r5-r6}		// idx

													add			r5, r0, r5, lsl #6	// M0
													add			r6, r0, r6, lsl #6	// M1


vldmia.64	r5!, {d4,d5} 						// M0[0]

vldmia.64	r6!, {d6,d7}						// M1[0]
FMULS3		(20,21,22,  8,9,10,  3,3,3)			//   M0[0] * w

vldmia.64	r5!, {d4,d5}						// M0[1]
FMACS3		(20,21,22,  12,13,14,  4,4,4)		// + M1[0] * w

vldmia.64	r6!, {d6,d7}						// M1[1]
FMULS3		(24,25,26,  8,9,10,  3,3,3)			//   M0[1] * w

vldmia.64	r5!, {d4,d5}						// M0[2]
FMACS3		(24,25,26,  12,13,14,  4,4,4)		// + M1[1] * w

vldmia.64	r6!, {d6,d7}						// M1[2]
FMULS3		(28,29,30,  8,9,10,  3,3,3)			//   M0[2] * w

vldmia.64	r5!, {d4,d5}						// M0[3]
FMACS3		(28,29,30,  12,13,14,  4,4,4)		// + M1[2] * w

vldmia.64	r6!, {d6,d7}						// M1[3]
FMULS3		(0,1,2,  8,9,10,  3,3,3)			//   M0[3] * w

FMACS3		(0,1,2,  12,13,14,  4,4,4)			// + M1[3] * w


LOAD_POS_NRM
LOAD_TAN

CALC_POS_2
CALC_NRM_1
CALC_TAN_1

CALC_POS_3
CALC_NRM_2
CALC_TAN_2
														pld			[r1, #1024]
														cmp			r1, r2
CALC_POS_4
CALC_NRM_3
CALC_TAN_3

CALC_TAN_4


STORE_POS_NRM
STORE_TAN

bcc			LOOP_NAME

ldmfd		sp!, {r4,r5,r6,r7,r8,r10,r11}
vpop		{d7-d15}
bx			lr



//===========================================================================================================================================

#elif (SKIN_4BONES == SKIN_POS) || (SKIN_4BONES == SKIN_POS_NRM) || (SKIN_4BONES == SKIN_POS_NRM_TAN)

mov			ip, sp
vpush		{d7-d15}
stmfd		sp!, {r4,r5,r6,r7,r8}

ldr			r4, [ip, #0]


.align 4
LOOP_NAME:

vldmia.32	r3!, {s3-s6}						// w
													ldmia		r3!, {r5-r8}		// idx

													add			r5, r0, r5, lsl #6	// M0
													add			r6, r0, r6, lsl #6	// M1
													add			r7, r0, r7, lsl #6	// M2
													add			r8, r0, r8, lsl #6	// M3


vldmia.64	r5!, {d4,d5} 						// M0[0]

vldmia.64	r6!, {d6,d7}						// M1[0]
FMULS3		(20,21,22,  8,9,10,  3,3,3)			//   M0[0] * w

vldmia.64	r7!, {d4,d5}						// M2[0]
FMACS3		(20,21,22,  12,13,14,  4,4,4)		// + M1[0] * w

vldmia.64	r8!, {d6,d7}						// M3[0]
FMACS3		(20,21,22,  8,9,10,  5,5,5)			// + M2[0] * w

vldmia.64	r5!, {d4,d5}						// M0[1]
FMACS3		(20,21,22,  12,13,14,  6,6,6)		// + M3[0] * w

vldmia.64	r6!, {d6,d7}						// M1[1]
FMULS3		(24,25,26,  8,9,10,  3,3,3)			//   M0[1] * w

vldmia.64	r7!, {d4,d5}						// M2[1]
FMACS3		(24,25,26,  12,13,14,  4,4,4)		// + M1[1] * w

vldmia.64	r8!, {d6,d7}						// M3[1]
FMACS3		(24,25,26,  8,9,10,  5,5,5)			// + M2[1] * w

vldmia.64	r5!, {d4,d5}						// M0[2]
FMACS3		(24,25,26,  12,13,14,  6,6,6)		// + M3[1] * w

vldmia.64	r6!, {d6,d7}						// M1[2]
FMULS3		(28,29,30,  8,9,10,  3,3,3)			//   M0[2] * w

vldmia.64	r7!, {d4,d5}						// M2[2]
FMACS3		(28,29,30,  12,13,14,  4,4,4)		// + M1[2] * w

vldmia.64	r8!, {d6,d7}						// M3[2]
FMACS3		(28,29,30,  8,9,10,  5,5,5)			// + M2[2] * w

vldmia.64	r5!, {d4,d5}						// M0[3]
FMACS3		(28,29,30,  12,13,14,  6,6,6)		// + M3[2] * w

vldmia.64	r6!, {d6,d7}						// M1[3]
FMULS3		(0,1,2,  8,9,10,  3,3,3)			//   M0[3] * w

vldmia.64	r7!, {d4,d5}						// M2[3]
FMACS3		(0,1,2,  12,13,14,  4,4,4)			// + M1[3] * w

vldmia.64	r8!, {d6,d7}						// M3[3]
FMACS3		(0,1,2,  8,9,10,  5,5,5)			// + M2[3] * w

FMACS3		(0,1,2,  12,13,14,  6,6,6)			// + M3[3] * w


LOAD_POS_NRM
LOAD_TAN

CALC_POS_2
CALC_NRM_1
CALC_TAN_1

CALC_POS_3
CALC_NRM_2
CALC_TAN_2
														pld			[r1, #1024]
														cmp			r1, r2
CALC_POS_4
CALC_NRM_3
CALC_TAN_3

CALC_TAN_4


STORE_POS_NRM
STORE_TAN

bcc			LOOP_NAME

ldmfd		sp!, {r4,r5,r6,r7,r8}
vpop		{d7-d15}
bx			lr

#endif

//===========================================================================================================================================

#undef CALC_POS_1
#undef CALC_POS_2
#undef CALC_POS_3
#undef STORE_POS_NRM
#undef LOAD_POS_NRM
#undef CALC_NRM_1
#undef CALC_NRM_2
#undef CALC_NRM_3
#undef LOAD_TAN
#undef STORE_TAN
#undef CALC_TAN_1
#undef CALC_TAN_2
#undef CALC_TAN_3
#undef CALC_TAN_4
