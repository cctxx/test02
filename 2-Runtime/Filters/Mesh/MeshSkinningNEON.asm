	AREA .text, CODE

	EXPORT _s_SkinVertices_NEON
	EXPORT _s_SkinVertices_NoNormals_NEON
	EXPORT _s_SkinVertices_Tangents_NEON
	EXPORT _s_SkinVertices2Bones_NEON
	EXPORT _s_SkinVertices2Bones_NoNormals_NEON
	EXPORT _s_SkinVertices2Bones_Tangents_NEON
	EXPORT _s_SkinVertices4Bones_NEON
	EXPORT _s_SkinVertices4Bones_NoNormals_NEON
	EXPORT _s_SkinVertices4Bones_Tangents_NEON

|_s_SkinVertices_NEON| PROC
	mov	ip, sp
	vpush	{d8-d10}
	stmdb	sp!, {r4, r5, r6, r7, r8}
	ldr.w	r4, [ip]
	mov.w	r8, #12
	ldr.w	r5, [r3], #4
	add.w	r7, r0, r5, lsl #6

|_s_SkinVertices_NEON_loop|
	vld1.32	{d24-d27}, [r7@128]!
	vld1.32	{d28-d31}, [r7@128]
	vld1.32	{d6-d8}, [r1@64]!
	vmul.f32	q0, q12, d6[0]
	vmul.f32	q1, q12, d7[1]
	cmp	r1, r2
	pld	[r1, #256]	; 0x100
	vmla.f32	q0, q13, d6[1]
	vmla.f32	q1, q13, d8[0]
	it	cc
	ldrcc.w	r5, [r3], #4
	add.w	r7, r0, r5, lsl #6
	vmla.f32	q0, q14, d7[0]
	vmla.f32	q1, q14, d8[1]
	pld	[r7]
	vadd.f32	q0, q0, q15
	vst1.32	{d0-d1}, [r4], r8
	vst1.32	{d2-d3}, [r4], r8
	bcc.w	|_s_SkinVertices_NEON_loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8}
	vpop	{d8-d10}
	bx	lr
	ENDP


|_s_SkinVertices_NoNormals_NEON| PROC
	mov	ip, sp
	vpush	{d8-d10}
	stmdb	sp!, {r4, r5, r6, r7, r8}
	ldr.w	r4, [ip]
	mov.w	r8, #12
	ldr.w	r5, [r3], #4
	add.w	r7, r0, r5, lsl #6

|_s_SkinVertices_NoNormals_NEON_loop|
	vld1.32	{d24-d27}, [r7@128]!
	vld1.32	{d28-d31}, [r7@128]
	vld1.32	{d6-d7}, [r1], r8
	vmul.f32	q0, q12, d6[0]
	cmp	r1, r2
	pld	[r1, #256]	; 0x100
	vmla.f32	q0, q13, d6[1]
	it	cc
	ldrcc.w	r5, [r3], #4
	add.w	r7, r0, r5, lsl #6
	vmla.f32	q0, q14, d7[0]
	pld	[r7]
	vadd.f32	q0, q0, q15
	vst1.32	{d0-d1}, [r4], r8
	bcc.w	|_s_SkinVertices_NoNormals_NEON_loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8}
	vpop	{d8-d10}
	bx	lr
	ENDP


|_s_SkinVertices_Tangents_NEON| PROC
	mov	ip, sp
	vpush	{d8-d10}
	stmdb	sp!, {r4, r5, r6, r7, r8}
	ldr.w	r4, [ip]
	mov.w	r8, #12
	ldr.w	r5, [r3], #4
	add.w	r7, r0, r5, lsl #6

|_s_SkinVertices_Tangents_NEON_loop|
	vld1.32	{d24-d27}, [r7@128]!
	vld1.32	{d28-d31}, [r7@128]
	vld1.32	{d6-d8}, [r1@64]!
	vld1.32	{d9-d10}, [r1@64]!
	vmul.f32	q0, q12, d6[0]
	vmul.f32	q1, q12, d7[1]
	vmul.f32	q2, q12, d9[0]
	cmp	r1, r2
	pld	[r1, #256]	; 0x100
	vmla.f32	q0, q13, d6[1]
	vmla.f32	q1, q13, d8[0]
	vmla.f32	q2, q13, d9[1]
	it	cc
	ldrcc.w	r5, [r3], #4
	add.w	r7, r0, r5, lsl #6
	vmla.f32	q0, q14, d7[0]
	vmla.f32	q1, q14, d8[1]
	vmla.f32	q2, q14, d10[0]
	pld	[r7]
	vadd.f32	q0, q0, q15
	vmov.f32	s11, s21
	vst1.32	{d0-d1}, [r4], r8
	vst1.32	{d2-d3}, [r4], r8
	vst1.32	{d4-d5}, [r4]!
	bcc.w	|_s_SkinVertices_Tangents_NEON_loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8}
	vpop	{d8-d10}
	bx	lr
	ENDP


|_s_SkinVertices2Bones_NEON| PROC
	mov	ip, sp
	vpush	{d8-d11}
	stmdb	sp!, {r4, r5, r6, r7, r8, sl}
	ldr.w	r4, [ip]
	vld1.32	{d11}, [r3]!
	ldmia	r3!, {r5, r6}
	add.w	r7, r0, r5, lsl #6
	vld1.32	{d16-d19}, [r7@128]!
	vmul.f32	q12, q8, d11[0]
	vmul.f32	q13, q9, d11[0]
	vld1.32	{d20-d23}, [r7@128]
	add.w	r7, r0, r6, lsl #6
	vmul.f32	q14, q10, d11[0]
	vmul.f32	q15, q11, d11[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmla.f32	q12, q8, d11[1]
	vmla.f32	q13, q9, d11[1]
	ldr	r5, [r3, #8]
	mov.w	r8, #12
	sub.w	sl, r2, #24
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q14, q10, d11[1]
	nop

|_s_SkinVertices2Bones_NEON_loop|
	cmp	r1, sl
	add.w	r7, r0, r5, lsl #6
	it	cc
	ldrcc	r6, [r3, #12]
	vld1.32	{d6-d8}, [r1@64]!
	vmla.f32	q15, q11, d11[1]
	vmul.f32	q0, q12, d6[0]
	vld1.32	{d16-d19}, [r7@128]!
	cmp	r1, sl
	vmul.f32	q1, q12, d7[1]
	vld1.32	{d11}, [r3]
	vmul.f32	q12, q8, d11[0]
	pld	[r1, #256]	; 0x100
	vmla.f32	q0, q13, d6[1]
	vld1.32	{d20-d23}, [r7@128]
	add.w	r7, r0, r6, lsl #6
	vmla.f32	q1, q13, d8[0]
	it	cc
	ldrcc	r5, [r3, #24]
	vmul.f32	q13, q9, d11[0]
	vmla.f32	q0, q14, d7[0]
	cmp	r1, r2
	vmla.f32	q1, q14, d8[1]
	vld1.32	{d16-d19}, [r7@128]!
	vmul.f32	q14, q10, d11[0]
	vadd.f32	q0, q0, q15
	vmul.f32	q15, q11, d11[0]
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d11[1]
	vst1.32	{d0-d1}, [r4], r8
	vmla.f32	q13, q9, d11[1]
	vst1.32	{d2-d3}, [r4], r8
	add.w	r3, r3, #16
	vmla.f32	q14, q10, d11[1]
	bcc.w	|_s_SkinVertices2Bones_NEON_loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, sl}
	vpop	{d8-d11}
	bx	lr
	ENDP


|_s_SkinVertices2Bones_NoNormals_NEON| PROC
	mov	ip, sp
	vpush	{d8-d11}
	stmdb	sp!, {r4, r5, r6, r7, r8, sl}
	ldr.w	r4, [ip]
	vld1.32	{d11}, [r3]!
	ldmia	r3!, {r5, r6}
	add.w	r7, r0, r5, lsl #6
	vld1.32	{d16-d19}, [r7@128]!
	vmul.f32	q12, q8, d11[0]
	vmul.f32	q13, q9, d11[0]
	vld1.32	{d20-d23}, [r7@128]
	add.w	r7, r0, r6, lsl #6
	vmul.f32	q14, q10, d11[0]
	vmul.f32	q15, q11, d11[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmla.f32	q12, q8, d11[1]
	vmla.f32	q13, q9, d11[1]
	ldr	r5, [r3, #8]
	mov.w	r8, #12
	sub.w	sl, r2, #12
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q14, q10, d11[1]
	nop
	nop.w

|_s_SkinVertices2Bones_NoNormals_NEON_loop|
	cmp	r1, sl
	add.w	r7, r0, r5, lsl #6
	it	cc
	ldrcc	r6, [r3, #12]
	vld1.32	{d6-d7}, [r1], r8
	vmla.f32	q15, q11, d11[1]
	vmul.f32	q0, q12, d6[0]
	vld1.32	{d16-d19}, [r7@128]!
	cmp	r1, sl
	vld1.32	{d11}, [r3]
	vmul.f32	q12, q8, d11[0]
	pld	[r1, #256]	; 0x100
	vmla.f32	q0, q13, d6[1]
	vld1.32	{d20-d23}, [r7@128]
	add.w	r7, r0, r6, lsl #6
	it	cc
	ldrcc	r5, [r3, #24]
	vmul.f32	q13, q9, d11[0]
	vmla.f32	q0, q14, d7[0]
	cmp	r1, r2
	vld1.32	{d16-d19}, [r7@128]!
	vmul.f32	q14, q10, d11[0]
	vadd.f32	q0, q0, q15
	vmul.f32	q15, q11, d11[0]
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d11[1]
	vst1.32	{d0-d1}, [r4], r8
	vmla.f32	q13, q9, d11[1]
	add.w	r3, r3, #16
	vmla.f32	q14, q10, d11[1]
	bcc.w	|_s_SkinVertices2Bones_NoNormals_NEON_loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, sl}
	vpop	{d8-d11}
	bx	lr
	ENDP


|_s_SkinVertices2Bones_Tangents_NEON| PROC
	mov	ip, sp
	vpush	{d8-d11}
	stmdb	sp!, {r4, r5, r6, r7, r8, sl}
	ldr.w	r4, [ip]
	vld1.32	{d11}, [r3]!
	ldmia	r3!, {r5, r6}
	add.w	r7, r0, r5, lsl #6
	vld1.32	{d16-d19}, [r7@128]!
	vmul.f32	q12, q8, d11[0]
	vmul.f32	q13, q9, d11[0]
	vld1.32	{d20-d23}, [r7@128]
	add.w	r7, r0, r6, lsl #6
	vmul.f32	q14, q10, d11[0]
	vmul.f32	q15, q11, d11[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmla.f32	q12, q8, d11[1]
	vmla.f32	q13, q9, d11[1]
	ldr	r5, [r3, #8]
	mov.w	r8, #12
	sub.w	sl, r2, #40	; 0x28
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q14, q10, d11[1]
	nop
	nop.w

|_s_SkinVertices2Bones_Tangents_NEON_loop|
	cmp	r1, sl
	add.w	r7, r0, r5, lsl #6
	it	cc
	ldrcc	r6, [r3, #12]
	vld1.32	{d6-d8}, [r1@64]!
	vmla.f32	q15, q11, d11[1]
	vld1.32	{d9-d10}, [r1@64]!
	vmul.f32	q0, q12, d6[0]
	vld1.32	{d16-d19}, [r7@128]!
	cmp	r1, sl
	vmul.f32	q1, q12, d7[1]
	vmul.f32	q2, q12, d9[0]
	vld1.32	{d11}, [r3]
	vmul.f32	q12, q8, d11[0]
	pld	[r1, #256]	; 0x100
	vmla.f32	q0, q13, d6[1]
	vld1.32	{d20-d23}, [r7@128]
	add.w	r7, r0, r6, lsl #6
	vmla.f32	q1, q13, d8[0]
	vmla.f32	q2, q13, d9[1]
	it	cc
	ldrcc	r5, [r3, #24]
	vmul.f32	q13, q9, d11[0]
	vmla.f32	q0, q14, d7[0]
	cmp	r1, r2
	vmla.f32	q1, q14, d8[1]
	vmla.f32	q2, q14, d10[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmul.f32	q14, q10, d11[0]
	vadd.f32	q0, q0, q15
	vmov.f32	s11, s21
	vmul.f32	q15, q11, d11[0]
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d11[1]
	vst1.32	{d0-d1}, [r4], r8
	vmla.f32	q13, q9, d11[1]
	vst1.32	{d2-d3}, [r4], r8
	add.w	r3, r3, #16
	vmla.f32	q14, q10, d11[1]
	vst1.32	{d4-d5}, [r4]!
	bcc.w	|_s_SkinVertices2Bones_Tangents_NEON_loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8, sl}
	vpop	{d8-d11}
	bx	lr
	ENDP


|_s_SkinVertices4Bones_NEON| PROC
	mov	ip, sp
	vpush	{d8-d12}
	stmdb	sp!, {r4, r5, r6, r7, r8}
	ldr.w	r4, [ip]
	vld1.32	{d11-d12}, [r3]!
	ldmia	r3!, {r5, r6}
	add.w	r7, r0, r5, lsl #6
	vld1.32	{d16-d19}, [r7@128]!
	vld1.32	{d20-d23}, [r7@128]
	mov.w	r8, #12
	nop.w
	nop.w
	nop.w

|_s_SkinVertices4Bones_NEON_loop|
	vmul.f32	q12, q8, d11[0]
	vld1.32	{d6-d8}, [r1@64]!
	vmul.f32	q13, q9, d11[0]
	add.w	r7, r0, r6, lsl #6
	vmul.f32	q14, q10, d11[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmul.f32	q15, q11, d11[0]
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d11[1]
	ldmia	r3!, {r5, r6}
	vmla.f32	q13, q9, d11[1]
	add.w	r7, r0, r5, lsl #6
	cmp	r1, r2
	vmla.f32	q14, q10, d11[1]
	vld1.32	{d16-d19}, [r7@128]!
	vmla.f32	q15, q11, d11[1]
	pld	[r3, #256]	; 0x100
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d12[0]
	add.w	r7, r0, r6, lsl #6
	vmla.f32	q13, q9, d12[0]
	vmla.f32	q14, q10, d12[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmla.f32	q15, q11, d12[0]
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d12[1]
	vmla.f32	q13, q9, d12[1]
	vmla.f32	q14, q10, d12[1]
	vmla.f32	q15, q11, d12[1]
	pld	[r1, #256]	; 0x100
	vmul.f32	q0, q12, d6[0]
	vld1.32	{d11-d12}, [r3]!
	vmul.f32	q1, q12, d7[1]
	it	cc
	ldmiacc	r3!, {r5, r6}
	vmla.f32	q0, q13, d6[1]
	add.w	r7, r0, r5, lsl #6
	vmla.f32	q1, q13, d8[0]
	vldmia	r7, {d16-d23}
	vmla.f32	q0, q14, d7[0]
	vmla.f32	q1, q14, d8[1]
	vadd.f32	q0, q0, q15
	vst1.32	{d0-d1}, [r4], r8
	vst1.32	{d2-d3}, [r4], r8
	bcc.w	|_s_SkinVertices4Bones_NEON_loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8}
	vpop	{d8-d12}
	bx	lr
	ENDP


|_s_SkinVertices4Bones_NoNormals_NEON| PROC
	mov	ip, sp
	vpush	{d8-d12}
	stmdb	sp!, {r4, r5, r6, r7, r8}
	ldr.w	r4, [ip]
	vld1.32	{d11-d12}, [r3]!
	ldmia	r3!, {r5, r6}
	add.w	r7, r0, r5, lsl #6
	vld1.32	{d16-d19}, [r7@128]!
	vld1.32	{d20-d23}, [r7@128]
	mov.w	r8, #12
	nop
	nop.w

|_s_SkinVertices4Bones_NoNormals_NEON_loop|
	vmul.f32	q12, q8, d11[0]
	vld1.32	{d6-d7}, [r1], r8
	vmul.f32	q13, q9, d11[0]
	add.w	r7, r0, r6, lsl #6
	vmul.f32	q14, q10, d11[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmul.f32	q15, q11, d11[0]
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d11[1]
	ldmia	r3!, {r5, r6}
	vmla.f32	q13, q9, d11[1]
	add.w	r7, r0, r5, lsl #6
	cmp	r1, r2
	vmla.f32	q14, q10, d11[1]
	vld1.32	{d16-d19}, [r7@128]!
	vmla.f32	q15, q11, d11[1]
	pld	[r3, #256]	; 0x100
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d12[0]
	add.w	r7, r0, r6, lsl #6
	vmla.f32	q13, q9, d12[0]
	vmla.f32	q14, q10, d12[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmla.f32	q15, q11, d12[0]
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d12[1]
	vmla.f32	q13, q9, d12[1]
	vmla.f32	q14, q10, d12[1]
	vmla.f32	q15, q11, d12[1]
	pld	[r1, #256]	; 0x100
	vmul.f32	q0, q12, d6[0]
	vld1.32	{d11-d12}, [r3]!
	it	cc
	ldmiacc	r3!, {r5, r6}
	vmla.f32	q0, q13, d6[1]
	add.w	r7, r0, r5, lsl #6
	vldmia	r7, {d16-d23}
	vmla.f32	q0, q14, d7[0]
	vadd.f32	q0, q0, q15
	vst1.32	{d0-d1}, [r4], r8
	bcc.w	|_s_SkinVertices4Bones_NoNormals_NEON_loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8}
	vpop	{d8-d12}
	bx	lr
	ENDP


|_s_SkinVertices4Bones_Tangents_NEON| PROC
	mov	ip, sp
	vpush	{d8-d12}
	stmdb	sp!, {r4, r5, r6, r7, r8}
	ldr.w	r4, [ip]
	vld1.32	{d11-d12}, [r3]!
	ldmia	r3!, {r5, r6}
	add.w	r7, r0, r5, lsl #6
	vld1.32	{d16-d19}, [r7@128]!
	vld1.32	{d20-d23}, [r7@128]
	mov.w	r8, #12
	nop
	nop.w

|_s_SkinVertices4Bones_Tangents_NEON_loop|
	vmul.f32	q12, q8, d11[0]
	vld1.32	{d6-d8}, [r1@64]!
	vmul.f32	q13, q9, d11[0]
	vld1.32	{d9-d10}, [r1@64]!
	add.w	r7, r0, r6, lsl #6
	vmul.f32	q14, q10, d11[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmul.f32	q15, q11, d11[0]
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d11[1]
	ldmia	r3!, {r5, r6}
	vmla.f32	q13, q9, d11[1]
	add.w	r7, r0, r5, lsl #6
	cmp	r1, r2
	vmla.f32	q14, q10, d11[1]
	vld1.32	{d16-d19}, [r7@128]!
	vmla.f32	q15, q11, d11[1]
	pld	[r3, #256]	; 0x100
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d12[0]
	add.w	r7, r0, r6, lsl #6
	vmla.f32	q13, q9, d12[0]
	vmla.f32	q14, q10, d12[0]
	vld1.32	{d16-d19}, [r7@128]!
	vmla.f32	q15, q11, d12[0]
	vld1.32	{d20-d23}, [r7@128]
	vmla.f32	q12, q8, d12[1]
	vmla.f32	q13, q9, d12[1]
	vmla.f32	q14, q10, d12[1]
	vmla.f32	q15, q11, d12[1]
	pld	[r1, #256]	; 0x100
	vmul.f32	q0, q12, d6[0]
	vld1.32	{d11-d12}, [r3]!
	vmul.f32	q1, q12, d7[1]
	vmul.f32	q2, q12, d9[0]
	it	cc
	ldmiacc	r3!, {r5, r6}
	vmla.f32	q0, q13, d6[1]
	add.w	r7, r0, r5, lsl #6
	vmla.f32	q1, q13, d8[0]
	vmla.f32	q2, q13, d9[1]
	vldmia	r7, {d16-d23}
	vmla.f32	q0, q14, d7[0]
	vmla.f32	q1, q14, d8[1]
	vmla.f32	q2, q14, d10[0]
	vadd.f32	q0, q0, q15
	vmov.f32	s11, s21
	vst1.32	{d0-d1}, [r4], r8
	vst1.32	{d2-d3}, [r4], r8
	vst1.32	{d4-d5}, [r4]!
	bcc.w	|_s_SkinVertices4Bones_Tangents_NEON_loop|
	ldmia.w	sp!, {r4, r5, r6, r7, r8}
	vpop	{d8-d12}
	bx	lr
	nop
	ENDP


	END
