// TODO: SOA

// defines
// LOOP_XYZ
// LOOP_XYZN
// LOOP_XYZNT
// LOOP_NAME
// COPY_DATA_SZ
// STRIDED_INPUT

#if STRIDED_INPUT

//r0:		const void* srcData
//r1:		const void* srcDataEnd
//r2:		const void* addData
//r3:		const void* xform
//[sp+0]:	void* dstData
//[sp+4]:	const int stride

mov			ip, sp

vpush		{d0-d15}
stmfd		sp!, {r4-r11}

vldmia		r3!, {q12-q15}

// r3:dstData
// r4: stride
// r6: proper offset for out ptr (pos, normal)

mov			r6, #12

ldr			r3, [ip, #0]
ldr			r4, [ip, #4]

// overlap calculation

vmov.32		q0, q15						// pos.w (1.0)


#if LOOP_XYZ

.align 4
LOOP_NAME:

pld			[r0, #512]					// prefetch

vld1.32		{d6,d7},		[r0], r4	// load pos

vmla.f32	q0, q12, d6[0]				// pos.x
vmul.f32	q1, q13, d6[1]				// pos.y
vmul.f32	q2, q14, d7[0]				// pos.z

vadd.f32	q0, q0, q1
										// load additional data
#if COPY_DATA_SZ == 1
vld1.32		{d9},			[r2], r4
#elif COPY_DATA_SZ == 2
vld1.32		{d9},			[r2], r4
#elif COPY_DATA_SZ == 3
vld1.32		{d9,d10},		[r2], r4
#elif COPY_DATA_SZ == 4
vld1.32		{d9,d10},		[r2], r4
#elif COPY_DATA_SZ == 5
vld1.32		{d9,d10,d11},	[r2], r4
#endif

vadd.f32	q0, q0, q2
cmp			r0, r1						// check cycle

vst1.32		{d0,d1}, [r3], r6

vmov.32		q0, q15						// pos.w (1.0)
										// save additional data
#if COPY_DATA_SZ == 1
vst1.32		{d9[0]},		[r3]!
#elif COPY_DATA_SZ == 2
vst1.32		{d9},			[r3]!
#elif COPY_DATA_SZ == 3
vst1.32		{d9},		    [r3]!
vst1.32     {d10[0]},       [r3]!
#elif COPY_DATA_SZ == 4
vst1.32		{d9,d10},		[r3]!
#elif COPY_DATA_SZ == 5
vst1.32		{d9,d10},       [r3]!
vst1.32     {d11[0]},       [r3]!
#endif

bcc			LOOP_NAME


#elif LOOP_XYZN


.align 4
LOOP_NAME:

pld			[r0, #512]				// prefetch

vld1.32		{d4,d5,d6},	[r0], r4	// load pos + normal

vmla.f32	q0, q12, d4[0]			// pos.x
vmul.f32	q1, q12, d5[1]			// normal.x

									// load additional data
#if COPY_DATA_SZ == 1
vld1.32		{d9},			[r2], r4
#elif COPY_DATA_SZ == 2
vld1.32		{d9},			[r2], r4
#elif COPY_DATA_SZ == 3
vld1.32		{d9,d10},		[r2], r4
#elif COPY_DATA_SZ == 4
vld1.32		{d9,d10},		[r2], r4
#elif COPY_DATA_SZ == 5
vld1.32		{d9,d10,d11},	[r2], r4
#endif

vmla.f32	q0, q13, d4[1]			// pos.y
vmla.f32	q1, q13, d6[0]			// normal.y

vmla.f32	q0, q14, d5[0]			// pos.z
vmla.f32	q1, q14, d6[1]			// normal.z

vst1.32		{d0,d1}, [r3], r6

cmp			r0, r1					// check cycle
vmov.32		q0, q15					// pos.w (1.0)
vst1.32		{d2,d3}, [r3], r6
									// save additional data
#if COPY_DATA_SZ == 1
vst1.32     {d9[0]},        [r3]!
#elif COPY_DATA_SZ == 2
vst1.32     {d9},           [r3]!
#elif COPY_DATA_SZ == 3
vst1.32     {d9},           [r3]!
vst1.32     {d10[0]},       [r3]!
#elif COPY_DATA_SZ == 4
vst1.32     {d9,d10},       [r3]!
#elif COPY_DATA_SZ == 5
vst1.32     {d9,d10},       [r3]!
vst1.32     {d11[0]},       [r3]!
#endif


bcc			LOOP_NAME


#elif LOOP_XYZNT

//[sp+8]:	const void* tangent
//r8:		tangent

ldr			r8, [ip, #8]

mov			r9,  #12
mov			r10, #4

.align 4
LOOP_NAME:

pld			[r0, #512]				// prefetch

vld1.32		{d4,d5,d6},	[r0], r4	// load pos + normal
vld1.32		{d7,d8}, [r8], r4		// load tangent

vmla.f32	q0,  q12, d4[0]			// pos.x
vmul.f32	q1,  q12, d5[1]			// normal.x
vmul.f32	q11, q12, d7[0]			// tangent.x

									// load additional data
#if COPY_DATA_SZ == 1
vld1.32		{d9},			[r2], r4
#elif COPY_DATA_SZ == 2
vld1.32		{d9},			[r2], r4
#elif COPY_DATA_SZ == 3
vld1.32		{d9,d10},		[r2], r4
#elif COPY_DATA_SZ == 4
vld1.32		{d9,d10},		[r2], r4
#elif COPY_DATA_SZ == 5
vld1.32		{d9,d10,d11},	[r2], r4
#endif

vmla.f32	q0,  q13, d4[1]			// pos.y
vmla.f32	q1,  q13, d6[0]			// normal.y
vmla.f32	q11, q13, d7[1]			// tangent.y

vmla.f32	q0,  q14, d5[0]			// pos.z
vmla.f32	q1,  q14, d6[1]			// normal.z
vmla.f32	q11, q14, d8[0]			// tangent.z

vst1.32		{d0,d1}, [r3], r6

cmp			r0, r1					// check cycle
vmov.32		q0, q15					// pos.w (1.0)
vst1.32		{d2,d3}, [r3], r6
									// save additional data
#if COPY_DATA_SZ == 1
vst1.32     {d9[0]},        [r3]!
#elif COPY_DATA_SZ == 2
vst1.32     {d9},           [r3]!
#elif COPY_DATA_SZ == 3
vst1.32     {d9},           [r3]!
vst1.32     {d10[0]},       [r3]!
#elif COPY_DATA_SZ == 4
vst1.32     {d9,d10},       [r3]!
#elif COPY_DATA_SZ == 5
vst1.32     {d9,d10},       [r3]!
vst1.32     {d11[0]},       [r3]!
#endif


// TODO: less stupid way

vtrn.32		d8, d7
vst1.32		{d22,d23},     [r3], r9
vst1.32		{d7[0]},       [r3], r10

bcc			LOOP_NAME
#elif LOOP_SPRITE
.align 4
ldr		   r7, [ip, #8]				    // load color32
vmov.32    d10[0], r7
LOOP_NAME:

pld			[r0, #512]					// prefetch

vld1.32		{d6,d7},		[r0], r4	// load pos

vmla.f32	q0, q12, d6[0]				// pos.x
vmul.f32	q1, q13, d6[1]				// pos.y
vmul.f32	q2, q14, d7[0]				// pos.z
vadd.f32	q0, q0, q1
// load data
vld1.32		{d9},			[r2], r4

vadd.f32	q0, q0, q2
cmp			r0, r1						// check cycle

vst1.32		{d0,d1}, [r3], r6

vmov.32		q0, q15						// pos.w (1.0)
// save data
vst1.32		{d10[0]},			[r3]!    
vst1.32		{d9},    			[r3]!


bcc			LOOP_NAME
#endif

ldmfd		sp!, {r4-r11}
vpop		{d0-d15}
bx			lr

#endif
