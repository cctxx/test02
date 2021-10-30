;; SkinSSE2.s
;;
;; Created by Kaspar Daugaard on 1/12/11.
;; Copyright 2011 Unity Technologies. All rights reserved.

bits 32

section .text align=32

%define normalOffset 12
%define tangentOffset 24
		
%macro SkinSSE2_Generic 3
	; %1 numBones
	; %2 hasNormals
	; %3 hasTangents
	; [ebp +  8] inVertices
	; [ebp + 12] outVertices
	; [ebp + 16] numVertices
	; [ebp + 20] boneMatrices
	; [ebp + 24] weightsAndIndices
	; [ebp + 28] inputStride
	; [ebp + 32] outputStride

	push ebp
	mov	ebp, esp
	pushad
	
	; Local variables (32 byte aligned)
	; [esp +  0] MaskW
	; [esp + 16] MaskVec3
	; [esp + 32] savedEcx
	sub esp, 16*3
	and esp, ~31

	; Create bitmasks on stack
	sub eax, eax
	mov [esp +  0], eax ; MaskW
	mov [esp +  4], eax
	mov [esp +  8], eax
	dec eax
	mov [esp + 12], eax
	mov [esp + 16], eax ; MaskVec3
	mov [esp + 20], eax
	mov [esp + 24], eax
	inc eax
	mov [esp + 28], eax
	
	mov	esi, [ebp + 8]  ; inVertices
	mov	edi, [ebp + 12] ; outVertices
	mov ecx, [ebp + 16] ; numVertices
	mov edx, [ebp + 24] ; weightsAndIndices

	; Prefetch vertices
	prefetchnta [edx]
	prefetchnta [esi]
	prefetchnta [esi + 32]
	
	align 32

%%SkinSSE2_loop:
	prefetchnta [esi + 64]

	mov ebx, [ebp + 20] ; boneMatrices
	mov [esp + 32], ecx ; savedEcx

	; Load first bone index
%if %1 == 1
	; Single bone, no weight
	mov eax, [edx]
	shl eax, 6
%else
	; Indices come after weights
	mov eax, [edx + %1*4]
	shl eax, 6
	prefetchnta [ebx + eax]
	prefetchnta [ebx + eax + 32]

	; Load second bone index
	mov ecx, [edx + %1*4 + 4]
	shl ecx, 6
	prefetchnta [ebx + ecx]
	prefetchnta [ebx + ecx + 32]

	; Load all weights to xmm0
	movups xmm0, [edx]
%endif
	
	; Load first matrix to xmm4-xmm7
	movaps xmm4, [ebx + eax]
	movaps xmm5, [ebx + eax + 16]
	movaps xmm6, [ebx + eax + 32]
	movaps xmm7, [ebx + eax + 48]

%if %1 >= 2
	; Multiply first matrix with first weight
	movaps xmm1, xmm0
	shufps xmm1, xmm1, 0x00
	mulps xmm4, xmm1
	mulps xmm5, xmm1
	mulps xmm6, xmm1
	mulps xmm7, xmm1
%endif

%if %1 >= 3
	; Load third bone index
	mov eax, [edx + %1*4 + 8]
	shl eax, 6
	prefetchnta [ebx + eax]
	prefetchnta [ebx + eax + 32]
%endif

%if %1 >= 2
	; Load first two rows of the second matrix to xmm2-xmm3
	movaps xmm2, [ebx + ecx]
	movaps xmm3, [ebx + ecx + 16]
	; Shuffle second weight to all elements of xmm1
	movaps xmm1, xmm0
	shufps xmm1, xmm1, 0x55
	; Multiply two first rows of second matrix with second weight
	mulps xmm2, xmm1
	mulps xmm3, xmm1
	; Add
	addps xmm4, xmm2
	addps xmm5, xmm3

	; Load last two rows of the second matrix to xmm2-xmm3
	movaps xmm2, [ebx + ecx + 32]
	movaps xmm3, [ebx + ecx + 48]
	; Multiply two last rows of the second matri with second weight
	mulps xmm2, xmm1
	mulps xmm3, xmm1
	; Add
	addps xmm6, xmm2
	addps xmm7, xmm3
%endif

%if %1 >= 4
	; Load fourth bone index
	mov ecx, [edx + %1*4 + 12]
	shl ecx, 6
	prefetchnta [ebx + ecx]
	prefetchnta [ebx + ecx + 32]
%endif

%if %1 >= 3
	; Load first two rows of the third matrix to xmm2-xmm3
	movaps xmm2, [ebx + eax]
	movaps xmm3, [ebx + eax + 16]
	; Shuffle third weight to all elements of xmm1
	movaps xmm1, xmm0
	shufps xmm1, xmm1, 0xaa
	; Multiply first two rows of third matrix with third weight
	mulps xmm2, xmm1
	mulps xmm3, xmm1
	; Add
	addps xmm4, xmm2
	addps xmm5, xmm3

	; Load last two rows of the third matrix to xmm2-xmm3
	movaps xmm2, [ebx + eax + 32]
	movaps xmm3, [ebx + eax + 48]
	; Multiply last two rows of third matrix with third weight
	mulps xmm2, xmm1
	mulps xmm3, xmm1
	; Add
	addps xmm6, xmm2
	addps xmm7, xmm3
%endif

%if %1 >= 4
	; Load first two rows of the fourth matrix into xmm2-xmm3
	movaps xmm2, [ebx + ecx]
	movaps xmm3, [ebx + ecx + 16]
	; Shuffle fourth weight to all elements of xmm1
	movaps xmm1, xmm0
	shufps xmm1, xmm1, 0xff
	; Multiply first two rows of the fourth matrix with fourth weight
	mulps xmm2, xmm1
	mulps xmm3, xmm1
	; Add
	addps xmm4, xmm2
	addps xmm5, xmm3

	; Load last two rows of the fourth matrix to xmm2-xmm3
	movaps xmm2, [ebx + ecx + 32]
	movaps xmm3, [ebx + ecx + 48]
	; Multiply last two rows of the fourth matrix with fourth weight
	mulps xmm2, xmm1
	mulps xmm3, xmm1
	; Add
	addps xmm6, xmm2
	addps xmm7, xmm3
%endif

	; Matrix is in xmm4-xmm7
	; Transform position by 4x4 matrix in xmm4-xmm7
	movups xmm0, [esi]
	movaps xmm1, xmm0
	movaps xmm2, xmm0
	shufps xmm1, xmm1, 0x55
	shufps xmm2, xmm2, 0xaa
	shufps xmm0, xmm0, 0x00
	mulps xmm1, xmm5
	mulps xmm2, xmm6
	mulps xmm0, xmm4
	addps xmm1, xmm2
	addps xmm0, xmm7
	addps xmm0, xmm1
	; Store vertex position in outvert
	movaps xmm7, [esp + 16] ; MaskVec3
	maskmovdqu xmm0, xmm7

%if %2 ; Has normal 
	; Transform vector by 3x3 matrix in xmm4-xmm6
	movups xmm0, [esi + normalOffset]
	movaps xmm1, xmm0
	movaps xmm2, xmm0
	shufps xmm1, xmm1, 0x55
	shufps xmm2, xmm2, 0xaa
	shufps xmm0, xmm0, 0x00
	mulps xmm1, xmm5
	mulps xmm2, xmm6
	mulps xmm0, xmm4
	addps xmm1, xmm2
	addps xmm0, xmm1
%endif

%if %3 ; Has tangent
	; Transform vector by 3x3 matrix in xmm4-xmm6
	movups xmm1, [esi + tangentOffset]
	movaps xmm2, xmm1
	movaps xmm3, xmm1
	shufps xmm2, xmm2, 0x55
	shufps xmm3, xmm3, 0xaa
	mulps xmm2, xmm5
	mulps xmm3, xmm6
	movaps xmm6, xmm1 ; Save original tangent's W in xmm6
	shufps xmm1, xmm1, 0x00
	andps xmm6, [esp + 0] ; MaskW
	mulps xmm1, xmm4
	addps xmm2, xmm3
	addps xmm1, xmm2
%endif

%if %2 || %3 ; Has normal or tangent 
	; Calculate lengths and normalize
	movaps xmm2, xmm0
	movaps xmm5, xmm1
	mulps xmm2, xmm2
	mulps xmm5, xmm5
	movaps xmm3, xmm2
	movaps xmm4, xmm2
	shufps xmm3, xmm5, 0x55
	shufps xmm4, xmm5, 0xaa
	shufps xmm2, xmm5, 0x00
	addps xmm3, xmm4
	addps xmm2, xmm3
	sqrtps xmm2, xmm2
	rcpps xmm2, xmm2
	movaps xmm3, xmm2
	shufps xmm2, xmm2, 0x00
	shufps xmm3, xmm3, 0xaa
	mulps xmm0, xmm2
	mulps xmm1, xmm3
%endif

%if %2 ; Write normal
	add edi, normalOffset
	maskmovdqu xmm0, xmm7 ; MaskVec3
	sub edi, normalOffset
%endif

%if %3 ; Write tangent
	andps xmm1, xmm7	; MaskVec3
	orps xmm1, xmm6		; Restore original W 
	movups [edi + tangentOffset], xmm1
%endif
	
%if %1 == 1
	; Indices only
	add edx, 4 
%else
	; Indices and weights
	add edx, %1 * 8 
%endif

	add esi, [ebp + 28] ; inputStride
	add edi, [ebp + 32] ; outputStride
	mov ecx, [esp + 32] ; savedEcx
	dec ecx
	jnz %%SkinSSE2_loop

	; Remove local variables from stack
	lea esp, [ebp-32]
	
	popad
	pop ebp
	ret
	align 16
%endmacro


global SkinSSE2_1Bone_Pos
global SkinSSE2_2Bones_Pos
global SkinSSE2_4Bones_Pos
global SkinSSE2_1Bone_PosNormal
global SkinSSE2_2Bones_PosNormal
global SkinSSE2_4Bones_PosNormal
global SkinSSE2_1Bone_PosNormalTan
global SkinSSE2_2Bones_PosNormalTan
global SkinSSE2_4Bones_PosNormalTan


SkinSSE2_1Bone_Pos:					SkinSSE2_Generic 1, 0, 0
SkinSSE2_2Bones_Pos:				SkinSSE2_Generic 2, 0, 0
SkinSSE2_4Bones_Pos:				SkinSSE2_Generic 4, 0, 0
SkinSSE2_1Bone_PosNormal:			SkinSSE2_Generic 1, 1, 0
SkinSSE2_2Bones_PosNormal:			SkinSSE2_Generic 2, 1, 0
SkinSSE2_4Bones_PosNormal:			SkinSSE2_Generic 4, 1, 0
SkinSSE2_1Bone_PosNormalTan:		SkinSSE2_Generic 1, 1, 1
SkinSSE2_2Bones_PosNormalTan:		SkinSSE2_Generic 2, 1, 1
SkinSSE2_4Bones_PosNormalTan:		SkinSSE2_Generic 4, 1, 1
