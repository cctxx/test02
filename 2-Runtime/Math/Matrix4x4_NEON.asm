	AREA .text, CODE

	EXPORT _CopyMatrix_NEON
	EXPORT _TransposeMatrix4x4_NEON
	EXPORT _MultiplyMatrices4x4_NEON
	EXPORT _MultiplyMatrixArray4x4_NEON
	EXPORT _MultiplyMatrixArrayWithBase4x4_NEON

|_CopyMatrix_NEON| PROC
	vld1.32	{d0-d3}, [r0]!
	vld1.32	{d4-d7}, [r0]
	vst1.32	{d0-d3}, [r1]!
	vst1.32	{d4-d7}, [r1]
	bx	lr
	ENDP


|_TransposeMatrix4x4_NEON| PROC
	vld4.32	{d0,d2,d4,d6}, [r0]!
	vld4.32	{d1,d3,d5,d7}, [r0]
	vst1.32	{d0-d3}, [r1]!
	vst1.32	{d4-d7}, [r1]
	bx	lr
	ENDP


|_MultiplyMatrices4x4_NEON| PROC
	vld1.32	{d0-d3}, [r1]!
	vld1.32	{d16-d17}, [r0]!
	vmul.f32	q12, q8, d0[0]
	vld1.32	{d4-d5}, [r1]!
	vmul.f32	q13, q8, d2[0]
	vld1.32	{d6-d7}, [r1]!
	vmul.f32	q14, q8, d4[0]
	vld1.32	{d18-d19}, [r0]!
	vmul.f32	q15, q8, d6[0]
	vld1.32	{d20-d21}, [r0]!
	vmla.f32	q12, q9, d0[1]
	vld1.32	{d22-d23}, [r0]!
	vmla.f32	q13, q9, d2[1]
	vmla.f32	q14, q9, d4[1]
	vmla.f32	q15, q9, d6[1]
	vmla.f32	q12, q10, d1[0]
	vmla.f32	q13, q10, d3[0]
	vmla.f32	q14, q10, d5[0]
	vmla.f32	q15, q10, d7[0]
	vmla.f32	q12, q11, d1[1]
	vmla.f32	q13, q11, d3[1]
	vmla.f32	q14, q11, d5[1]
	vmla.f32	q15, q11, d7[1]
	vst1.32	{d24-d27}, [r2]!
	vst1.32	{d28-d31}, [r2]!
	bx	lr
	ENDP


|_MultiplyMatrixArray4x4_NEON| PROC
	vpush	{d8-d15}
	add.w	r3, r0, r3, lsl #6
	vld1.32	{d0-d3}, [r1]!
	vld1.32	{d4-d7}, [r1]!
	vld1.32	{d16-d17}, [r0]!
	nop

|_MultiplyMatrixArray4x4_NEON_loop|
	vmul.f32	q12, q8, d0[0]
	vld1.32	{d18-d19}, [r0]!
	vmul.f32	q13, q8, d2[0]
	vmul.f32	q14, q8, d4[0]
	vmul.f32	q15, q8, d6[0]
	vmla.f32	q12, q9, d0[1]
	vld1.32	{d20-d21}, [r0]!
	vmla.f32	q13, q9, d2[1]
	vld1.32	{d8-d11}, [r1]!
	vmla.f32	q14, q9, d4[1]
	vld1.32	{d12-d15}, [r1]!
	vmla.f32	q15, q9, d6[1]
	vmla.f32	q12, q10, d1[0]
	vld1.32	{d22-d23}, [r0]!
	vmla.f32	q13, q10, d3[0]
	vmla.f32	q14, q10, d5[0]
	vmla.f32	q15, q10, d7[0]
	vmla.f32	q12, q11, d1[1]
	vld1.32	{d16-d17}, [r0]!
	vmla.f32	q13, q11, d3[1]
	vmla.f32	q14, q11, d5[1]
	vmla.f32	q15, q11, d7[1]
	vst1.32	{d24-d27}, [r2]!
	vst1.32	{d28-d31}, [r2]!
	cmp	r0, r3
	bcs.w	|_MultiplyMatrixArray4x4_out|
	vmul.f32	q12, q8, d8[0]
	vld1.32	{d18-d19}, [r0]!
	vmul.f32	q13, q8, d10[0]
	vmul.f32	q14, q8, d12[0]
	vmul.f32	q15, q8, d14[0]
	vmla.f32	q12, q9, d8[1]
	vld1.32	{d20-d21}, [r0]!
	vmla.f32	q13, q9, d10[1]
	vld1.32	{d0-d3}, [r1]!
	vmla.f32	q14, q9, d12[1]
	vld1.32	{d4-d7}, [r1]!
	vmla.f32	q15, q9, d14[1]
	vmla.f32	q12, q10, d9[0]
	vld1.32	{d22-d23}, [r0]!
	vmla.f32	q13, q10, d11[0]
	vmla.f32	q14, q10, d13[0]
	vmla.f32	q15, q10, d15[0]
	vmla.f32	q12, q11, d9[1]
	vld1.32	{d16-d17}, [r0]!
	vmla.f32	q13, q11, d11[1]
	vmla.f32	q14, q11, d13[1]
	vmla.f32	q15, q11, d15[1]
	vst1.32	{d24-d27}, [r2]!
	vst1.32	{d28-d31}, [r2]!
	cmp	r0, r3
	bcc.w	|_MultiplyMatrixArray4x4_NEON_loop|
	nop.w

|_MultiplyMatrixArray4x4_out|
	vpop	{d8-d15}
	bx	lr
	ENDP


|_MultiplyMatrixArrayWithBase4x4_NEON| PROC
	mov	ip, sp
	vpush	{d8-d15}
	stmdb	sp!, {r4, r5}
	ldr.w	r4, [ip]
	add.w	r4, r1, r4, lsl #6
	vld1.32	{d16-d17}, [r1]!
	vld1.32	{d0-d3}, [r2]!
	vld1.32	{d4-d7}, [r2]!
	vld1.32	{d20-d23}, [r0]!
	add.w	r5, r0, #16
	vmul.f32	q4, q8, d0[0]
	vmul.f32	q5, q8, d2[0]
	nop.w
	nop.w
	nop.w
	
|_MultiplyMatrixArrayWithBase4x4_NEON_loop|
	vld1.32	{d18-d19}, [r1]!
	vmul.f32	q6, q8, d4[0]
	vmul.f32	q7, q8, d6[0]
	vmla.f32	q4, q9, d0[1]
	vmla.f32	q5, q9, d2[1]
	vld1.32	{d16-d17}, [r1]!
	vmla.f32	q6, q9, d4[1]
	vmla.f32	q7, q9, d6[1]
	vmla.f32	q4, q8, d1[0]
	vmla.f32	q5, q8, d3[0]
	vld1.32	{d18-d19}, [r1]!
	vmla.f32	q6, q8, d5[0]
	vmla.f32	q7, q8, d7[0]
	cmp	r1, r4
	vmla.f32	q4, q9, d1[1]
	vmla.f32	q5, q9, d3[1]
	vld1.32	{d16-d17}, [r0]
	vmla.f32	q6, q9, d5[1]
	vmla.f32	q7, q9, d7[1]
	vld1.32	{d18-d19}, [r5]
	vmul.f32	q12, q10, d8[0]
	vmul.f32	q13, q10, d10[0]
	vld1.32	{d0-d1}, [r2]!
	vmul.f32	q14, q10, d12[0]
	vmul.f32	q15, q10, d14[0]
	vmla.f32	q12, q11, d8[1]
	vmla.f32	q13, q11, d10[1]
	vld1.32	{d2-d3}, [r2]!
	vmla.f32	q14, q11, d12[1]
	vmla.f32	q15, q11, d14[1]
	vmla.f32	q12, q8, d9[0]
	vmla.f32	q13, q8, d11[0]
	vld1.32	{d4-d5}, [r2]!
	vmla.f32	q14, q8, d13[0]
	vmla.f32	q12, q9, d9[1]
	vmla.f32	q13, q9, d11[1]
	vmla.f32	q15, q8, d15[0]
	vld1.32	{d6-d7}, [r2]!
	vmla.f32	q14, q9, d13[1]
	vld1.32	{d16-d17}, [r1]!
	vmla.f32	q15, q9, d15[1]
	vst1.32	{d24-d27}, [r3]!
	vmul.f32	q4, q8, d0[0]
	vmul.f32	q5, q8, d2[0]
	vst1.32	{d28-d31}, [r3]!
	bcc.w	|_MultiplyMatrixArrayWithBase4x4_NEON_loop|
	pop	{r4, r5}
	vpop	{d8-d15}
	bx	lr
	nop
	ENDP


	END
