// defines
// LOOP_XYZ
// LOOP_XYZN
// LOOP_XYZNT
// LOOP_SPRITE
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
//[sp+8]:	const void* tangent

mov			ip, sp

vpush		{d0-d15}
stmfd		sp!, {r4-r11}

// {s16-s31} xform

vldmia.32	r3!, {s16-s31}

// r3:	dstData
// r4:  stride
//r11:		tangent
ldr			r3, [ip, #0]
ldr			r4, [ip, #4]

#if LOOP_XYZNT
ldr			r11, [ip, #8]
#endif

#if LOOP_SPRITE
//r6:		color
ldr			r6, [ip, #8]
#endif


mov			ip, r0
// VFP_VECTOR_LENGTH(3)
mov			r0, ip


#if LOOP_XYZ

.align 4
LOOP_NAME:

mov			r5, r0
pld			[r0, #512]				// prefetch

vldmia.32	r5!, {s0-s2}			// load pos
FCPYS4		(8,9,10,11,	28,29,30,31)						// pos.w

FMACS4		(8,9,10,11, 16,17,18,19, 0,0,0,0)				// pos.x
#if COPY_DATA_SZ == 1
ldmia		r2, {r6}				// load additional data
#elif COPY_DATA_SZ == 2
ldmia		r2, {r6-r7}				// load additional data
#elif COPY_DATA_SZ == 3
ldmia		r2, {r6-r8}				// load additional data
#elif COPY_DATA_SZ == 4
ldmia		r2, {r6-r9}				// load additional data
#elif COPY_DATA_SZ == 5
ldmia		r2, {r6-r10}			// load additional data
#endif

FMACS4		(8,9,10,11, 20,21,22,23, 1,1,1,1)				// pos.y
add			r0, r0, r4				// inc srcData

FMACS4		(8,9,10,11, 24,25,26,27, 2,2,2,2)				// pos.z
add			r2, r2, r4				// inc srcAddData

vstmia.32	r3!, {s8-s10}			// store pos
cmp			r0, r1					// check cycle

#if COPY_DATA_SZ == 1
stmia		r3!, {r6}				// save additional data
#elif COPY_DATA_SZ == 2
stmia		r3!, {r6-r7}			// save additional data
#elif COPY_DATA_SZ == 3
stmia		r3!, {r6-r8}			// save additional data
#elif COPY_DATA_SZ == 4
stmia		r3!, {r6-r9}			// save additional data
#elif COPY_DATA_SZ == 5
stmia		r3!, {r6-r10}			// save additional data
#endif

bcc			LOOP_NAME


#elif LOOP_XYZN

.align 4
LOOP_NAME:

mov			r5, r0
pld			[r0, #512]				// prefetch

vldmia.32	r5!, {s0-s2}			// load pos
FCPYS4		(8,9,10,11,   28,29,30,31)						// pos.w

vldmia.32	r5!, {s3-s5}			// load normal
FMACS4		(8,9,10,11,   16,17,18,19, 0,0,0,0)				// pos.x

FMULS4		(12,13,14,15, 16,17,18,19, 3,3,3,3)				// normal.x
FMACS4		(8,9,10,11,   20,21,22,23, 1,1,1,1)				// pos.y

#if COPY_DATA_SZ == 1
ldmia		r2, {r6}				// load additional data
#elif COPY_DATA_SZ == 2
ldmia		r2, {r6-r7}				// load additional data
#elif COPY_DATA_SZ == 3
ldmia		r2, {r6-r8}				// load additional data
#elif COPY_DATA_SZ == 4
ldmia		r2, {r6-r9}				// load additional data
#elif COPY_DATA_SZ == 5
ldmia		r2, {r6-r10}			// load additional data
#endif
FMACS4		(8,9,10,11,   24,25,26,27, 2,2,2,2)				// pos.z

FMACS4		(12,13,14,15, 20,21,22,23, 4,4,4,4)				// normal.y
vstmia.32	r3!, {s8-s10}			// store pos

FMACS4		(12,13,14,15, 24,25,26,27, 5,5,5,5)				// normal.z
add			r0, r0, r4				// inc srcData

vstmia.32	r3!, {s12-s14}			// store normal
add			r2, r2, r4				// inc srcAddData

cmp			r0, r1					// check cycle
#if COPY_DATA_SZ == 1
stmia		r3!, {r6}				// save additional data
#elif COPY_DATA_SZ == 2
stmia		r3!, {r6-r7}			// save additional data
#elif COPY_DATA_SZ == 3
stmia		r3!, {r6-r8}			// save additional data
#elif COPY_DATA_SZ == 4
stmia		r3!, {r6-r9}			// save additional data
#elif COPY_DATA_SZ == 5
stmia		r3!, {r6-r10}			// save additional data
#endif

bcc			LOOP_NAME

#elif LOOP_XYZNT

.align 4
LOOP_NAME:

mov			r5, r0
pld			[r0, #512]				// prefetch

vldmia.32	r5!, {s0-s2}			// load pos
FCPYS4		(8,9,10,11,   28,29,30,31)						// pos.w

vldmia.32	r5!, {s3-s5}			// load normal
FMACS4		(8,9,10,11,   16,17,18,19, 0,0,0,0)				// pos.x

FMULS4		(12,13,14,15, 16,17,18,19, 3,3,3,3)				// normal.x
FMACS4		(8,9,10,11,   20,21,22,23, 1,1,1,1)				// pos.y

#if COPY_DATA_SZ == 1
ldmia		r2, {r6}				// load additional data
#elif COPY_DATA_SZ == 2
ldmia		r2, {r6-r7}				// load additional data
#elif COPY_DATA_SZ == 3
ldmia		r2, {r6-r8}				// load additional data
#elif COPY_DATA_SZ == 4
ldmia		r2, {r6-r9}				// load additional data
#elif COPY_DATA_SZ == 5
ldmia		r2, {r6-r10}			// load additional data
#endif
FMACS4		(8,9,10,11,   24,25,26,27, 2,2,2,2)				// pos.z

FMACS4		(12,13,14,15, 20,21,22,23, 4,4,4,4)				// normal.y
vstmia.32	r3!, {s8-s10}			// store pos

FMACS4		(12,13,14,15, 24,25,26,27, 5,5,5,5)				// normal.z
vldmia.32	r11, {s0-s3}			// load tangent

add			r0, r0, r4				// inc srcData
FMULS4		(8,9,10,11, 16,17,18,19,   0,0,0,0)				// tangent.x

vstmia.32	r3!, {s12-s14}			// store normal
FMACS4		(8,9,10,11, 20,21,22,23,   1,1,1,1)				// tangent.y

cmp			r0, r1					// check cycle
FMACS4		(8,9,10,11, 24,25,26,27,   2,2,2,2)				// tangent.z

#if COPY_DATA_SZ == 1
stmia		r3!, {r6}				// save additional data
#elif COPY_DATA_SZ == 2
stmia		r3!, {r6-r7}			// save additional data
#elif COPY_DATA_SZ == 3
stmia		r3!, {r6-r8}			// save additional data
#elif COPY_DATA_SZ == 4
stmia		r3!, {r6-r9}			// save additional data
#elif COPY_DATA_SZ == 5
stmia		r3!, {r6-r10}			// save additional data
#endif
fcpys		s11, s3											// copy tangent.w

vstmia.32	r3!, {s8-s11}			// store tangent
add			r2, r2, r4				// inc srcAddData

add			r11, r11, r4			// inc srcTangent
bcc			LOOP_NAME

#elif LOOP_SPRITE

.align 4
LOOP_NAME:

mov			r5, r0
pld			[r0, #512]				// prefetch

vldmia.32	r5!, {s0-s2}			// load pos
FCPYS4		(8,9,10,11,	28,29,30,31)						// pos.w

FMACS4		(8,9,10,11, 16,17,18,19, 0,0,0,0)				// pos.x


ldmia		r2, {r7-r8}				// load uv

FMACS4		(8,9,10,11, 20,21,22,23, 1,1,1,1)				// pos.y
add			r0, r0, r4				// inc srcData

FMACS4		(8,9,10,11, 24,25,26,27, 2,2,2,2)				// pos.z
add			r2, r2, r4				// inc srcAddData

vstmia.32	r3!, {s8-s10}			// store pos
cmp			r0, r1					// check cycle

stmia		r3!, {r6-r8}			// save color and uv

bcc			LOOP_NAME
#endif

// VFP_VECTOR_LENGTH_ZERO

ldmfd		sp!, {r4-r11}
vpop		{d0-d15}
bx			lr

#endif // STRIDED_INPUT
