	AREA .text, CODE

	EXPORT _s_TransformVertices_Strided_XYZ_0_NEON
	EXPORT _s_TransformVertices_Strided_XYZ_1_NEON
	EXPORT _s_TransformVertices_Strided_XYZ_2_NEON
	EXPORT _s_TransformVertices_Strided_XYZ_3_NEON
	EXPORT _s_TransformVertices_Strided_XYZ_4_NEON
	EXPORT _s_TransformVertices_Strided_XYZ_5_NEON
	EXPORT _s_TransformVertices_Strided_XYZN_0_NEON
	EXPORT _s_TransformVertices_Strided_XYZN_1_NEON
	EXPORT _s_TransformVertices_Strided_XYZN_2_NEON
	EXPORT _s_TransformVertices_Strided_XYZN_3_NEON
	EXPORT _s_TransformVertices_Strided_XYZN_4_NEON
	EXPORT _s_TransformVertices_Strided_XYZN_5_NEON
	EXPORT _s_TransformVertices_Strided_XYZNT_0_NEON
	EXPORT _s_TransformVertices_Strided_XYZNT_1_NEON
	EXPORT _s_TransformVertices_Strided_XYZNT_2_NEON
	EXPORT _s_TransformVertices_Strided_XYZNT_3_NEON
	EXPORT _s_TransformVertices_Strided_XYZNT_4_NEON
	EXPORT _s_TransformVertices_Strided_XYZNT_5_NEON

|_s_TransformVertices_Strided_XYZ_0_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop

|TransformVertices_Strided_XYZ_0_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d6-d7}, [r0], r4
	vmla.f32	q0, q12, d6[0]
	vmul.f32	q1, q13, d6[1]
	vmul.f32	q2, q14, d7[0]
	vadd.f32	q0, q0, q1
	vadd.f32	q0, q0, q2
	cmp	r0, r1
	vst1.32	{d0-d1}, [r3], r6
	vorr	q0, q15, q15
	bcc.w	|TransformVertices_Strided_XYZ_0_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZ_1_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	nop.w
	nop.w
	nop.w

|TransformVertices_Strided_XYZ_1_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d6-d7}, [r0], r4
	vmla.f32	q0, q12, d6[0]
	vmul.f32	q1, q13, d6[1]
	vmul.f32	q2, q14, d7[0]
	vadd.f32	q0, q0, q1
	vld1.32	{d9}, [r2], r4
	vadd.f32	q0, q0, q2
	cmp	r0, r1
	vst1.32	{d0-d1}, [r3], r6
	vorr	q0, q15, q15
	vst1.32	{d9[0]}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZ_1_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZ_2_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	nop.w

|TransformVertices_Strided_XYZ_2_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d6-d7}, [r0], r4
	vmla.f32	q0, q12, d6[0]
	vmul.f32	q1, q13, d6[1]
	vmul.f32	q2, q14, d7[0]
	vadd.f32	q0, q0, q1
	vld1.32	{d9}, [r2], r4
	vadd.f32	q0, q0, q2
	cmp	r0, r1
	vst1.32	{d0-d1}, [r3], r6
	vorr	q0, q15, q15
	vst1.32	{d9}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZ_2_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZ_3_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	nop.w

|TransformVertices_Strided_XYZ_3_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d6-d7}, [r0], r4
	vmla.f32	q0, q12, d6[0]
	vmul.f32	q1, q13, d6[1]
	vmul.f32	q2, q14, d7[0]
	vadd.f32	q0, q0, q1
	vld1.32	{d9-d10}, [r2], r4
	vadd.f32	q0, q0, q2
	cmp	r0, r1
	vst1.32	{d0-d1}, [r3], r6
	vorr	q0, q15, q15
	vst1.32	{d9}, [r3]!
	vst1.32	{d10[0]}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZ_3_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZ_4_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop

|TransformVertices_Strided_XYZ_4_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d6-d7}, [r0], r4
	vmla.f32	q0, q12, d6[0]
	vmul.f32	q1, q13, d6[1]
	vmul.f32	q2, q14, d7[0]
	vadd.f32	q0, q0, q1
	vld1.32	{d9-d10}, [r2], r4
	vadd.f32	q0, q0, q2
	cmp	r0, r1
	vst1.32	{d0-d1}, [r3], r6
	vorr	q0, q15, q15
	vst1.32	{d9-d10}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZ_4_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZ_5_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	nop.w

|TransformVertices_Strided_XYZ_5_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d6-d7}, [r0], r4
	vmla.f32	q0, q12, d6[0]
	vmul.f32	q1, q13, d6[1]
	vmul.f32	q2, q14, d7[0]
	vadd.f32	q0, q0, q1
	vld1.32	{d9-d11}, [r2], r4
	vadd.f32	q0, q0, q2
	cmp	r0, r1
	vst1.32	{d0-d1}, [r3], r6
	vorr	q0, q15, q15
	vst1.32	{d9-d10}, [r3]!
	vst1.32	{d11[0]}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZ_5_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZN_0_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	
|TransformVertices_Strided_XYZN_0_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	bcc.w	|TransformVertices_Strided_XYZN_0_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZN_1_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	nop.w
	
|TransformVertices_Strided_XYZN_1_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vld1.32	{d9}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9[0]}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZN_1_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZN_2_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	nop.w
	nop.w
	nop.w
	
|TransformVertices_Strided_XYZN_2_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vld1.32	{d9}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZN_2_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP

	
|_s_TransformVertices_Strided_XYZN_3_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	nop.w
	nop.w
	nop.w

|TransformVertices_Strided_XYZN_3_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vld1.32	{d9-d10}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9}, [r3]!
	vst1.32	{d10[0]}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZN_3_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZN_4_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	nop.w
	nop.w
	
|TransformVertices_Strided_XYZN_4_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vld1.32	{d9-d10}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9-d10}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZN_4_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZN_5_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	nop
	nop.w
	nop.w
	nop.w
	
|TransformVertices_Strided_XYZN_5_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vld1.32	{d9-d11}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9-d10}, [r3]!
	vst1.32	{d11[0]}, [r3]!
	bcc.w	|TransformVertices_Strided_XYZN_5_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZNT_0_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	ldr.w	r8, [ip, #8]
	mov.w	r9, #12
	mov.w	sl, #4
	nop
	nop.w
	nop.w
	nop.w
	
|TransformVertices_Strided_XYZNT_0_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vld1.32	{d7-d8}, [r8], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vmul.f32	q11, q12, d7[0]
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q11, q13, d7[1]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vmla.f32	q11, q14, d8[0]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vtrn.32	d8, d7
	vst1.32	{d22-d23}, [r3], r9
	vst1.32	{d7[0]}, [r3], sl
	bcc.w	|TransformVertices_Strided_XYZNT_0_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZNT_1_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	ldr.w	r8, [ip, #8]
	mov.w	r9, #12
	mov.w	sl, #4
	nop
	nop.w
	nop.w
	nop.w
	
|TransformVertices_Strided_XYZNT_1_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vld1.32	{d7-d8}, [r8], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vmul.f32	q11, q12, d7[0]
	vld1.32	{d9}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q11, q13, d7[1]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vmla.f32	q11, q14, d8[0]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9[0]}, [r3]!
	vtrn.32	d8, d7
	vst1.32	{d22-d23}, [r3], r9
	vst1.32	{d7[0]}, [r3], sl
	bcc.w	|TransformVertices_Strided_XYZNT_1_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZNT_2_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	ldr.w	r8, [ip, #8]
	mov.w	r9, #12
	mov.w	sl, #4
	nop
	nop.w
	
|TransformVertices_Strided_XYZNT_2_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vld1.32	{d7-d8}, [r8], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vmul.f32	q11, q12, d7[0]
	vld1.32	{d9}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q11, q13, d7[1]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vmla.f32	q11, q14, d8[0]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9}, [r3]!
	vtrn.32	d8, d7
	vst1.32	{d22-d23}, [r3], r9
	vst1.32	{d7[0]}, [r3], sl
	bcc.w	|TransformVertices_Strided_XYZNT_2_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZNT_3_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	ldr.w	r8, [ip, #8]
	mov.w	r9, #12
	mov.w	sl, #4
	nop
	nop.w
	
|TransformVertices_Strided_XYZNT_3_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vld1.32	{d7-d8}, [r8], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vmul.f32	q11, q12, d7[0]
	vld1.32	{d9-d10}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q11, q13, d7[1]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vmla.f32	q11, q14, d8[0]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9}, [r3]!
	vst1.32	{d10[0]}, [r3]!
	vtrn.32	d8, d7
	vst1.32	{d22-d23}, [r3], r9
	vst1.32	{d7[0]}, [r3], sl
	bcc.w	|TransformVertices_Strided_XYZNT_3_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZNT_4_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	ldr.w	r8, [ip, #8]
	mov.w	r9, #12
	mov.w	sl, #4
	nop
	
|TransformVertices_Strided_XYZNT_4_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vld1.32	{d7-d8}, [r8], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vmul.f32	q11, q12, d7[0]
	vld1.32	{d9-d10}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q11, q13, d7[1]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vmla.f32	q11, q14, d8[0]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9-d10}, [r3]!
	vtrn.32	d8, d7
	vst1.32	{d22-d23}, [r3], r9
	vst1.32	{d7[0]}, [r3], sl
	bcc.w	|TransformVertices_Strided_XYZNT_4_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	ENDP


|_s_TransformVertices_Strided_XYZNT_5_NEON| PROC
	mov	ip, sp
	vpush	{s0-s15}
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vldmia	r3!, {d24-d31}
	mov.w	r6, #12
	ldr.w	r3, [ip]
	ldr.w	r4, [ip, #4]
	vorr	q0, q15, q15
	ldr.w	r8, [ip, #8]
	mov.w	r9, #12
	mov.w	sl, #4
	nop
	nop.w

|TransformVertices_Strided_XYZNT_5_Loop|
	pld	[r0, #512]	; 0x200
	vld1.32	{d4-d6}, [r0], r4
	vld1.32	{d7-d8}, [r8], r4
	vmla.f32	q0, q12, d4[0]
	vmul.f32	q1, q12, d5[1]
	vmul.f32	q11, q12, d7[0]
	vld1.32	{d9-d11}, [r2], r4
	vmla.f32	q0, q13, d4[1]
	vmla.f32	q1, q13, d6[0]
	vmla.f32	q11, q13, d7[1]
	vmla.f32	q0, q14, d5[0]
	vmla.f32	q1, q14, d6[1]
	vmla.f32	q11, q14, d8[0]
	vst1.32	{d0-d1}, [r3], r6
	cmp	r0, r1
	vorr	q0, q15, q15
	vst1.32	{d2-d3}, [r3], r6
	vst1.32	{d9-d10}, [r3]!
	vst1.32	{d11[0]}, [r3]!
	vtrn.32	d8, d7
	vst1.32	{d22-d23}, [r3], r9
	vst1.32	{d7[0]}, [r3], sl
	bcc.w	|TransformVertices_Strided_XYZNT_5_Loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp}
	vpop	{s0-s15}
	bx	lr
	nop.w
	nop.w
	nop.w
	ENDP


	END
