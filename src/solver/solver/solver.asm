;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Copyright (c) Krzesimir Hyżyk - All Rights Reserved                      ;
; Unauthorized copying of this file, via any medium is strictly prohibited ;
; Proprietary and confidential                                             ;
; Created on 18/11/2024 by Krzesimir Hyżyk                                 ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

[bits 64]
extern pow
global _solver_step_batch_rkf45_internal:function
section .text



%macro align_code 1
[sectalign %1]
%define _align_code_0
%define _align_code_1 0x90
%define _align_code_2 0x66,0x90
%define _align_code_3 0x0f,0x1f,0x00
%define _align_code_4 0x0f,0x1f,0x40,0x00
%define _align_code_5 0x0f,0x1f,0x44,0x00,0x00
%define _align_code_6 0x66,0x0f,0x1f,0x44,0x00,0x00
%define _align_code_7 0x0f,0x1f,0x80,0x00,0x00,0x00,0x00
%assign pad (((%1)-(($-$$)%(%1)))%(%1))
times %eval(pad>>3) db 0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00
db _align_code_%[%eval(pad&7)]
%endmacro



align_code 8
_solver_step_batch_rkf45_internal:
	push rbp
	mov rbp, rsp
	push r15
	push r14
	push r13
	push r12
	push rbx
	; rbx	ctx->point_size
	; r12	in
	; r13	out
	; r14	k1
	; r15	solver
	; rbp	<stack_frame_pointer>
	; [rsp]	count
	push rsi
	mov r15, rdi
	mov r14, qword [rdi+72]
	mov r13, rcx
	mov r12, rdx
	mov ebx, dword [rdi+24]
._next_step:
	; xmm0	solver->time
	; rdi	in
	; rsi	k1
	mov rdi, r12
	mov rsi, r14
	vmovq xmm0, qword [r15+32]
	vzeroupper
	call qword [r15]
	; rax	index
	; rdi	k5
	; ymm0	{solver->time_step}
	; ymm1	{1/2}
	; ymm2	inv
	; ymm3	scratch #1
	vbroadcastsd ymm0, [r15+40]
	vbroadcastsd ymm1, [rel _packed_vector_data]
	xor eax, eax
	lea rdi, [r14+rbx*4]
._next_first_order_step:
	vmulpd ymm3, ymm0, [r14+rax]
	vmovapd ymm2, [r12+rax]
	vmovdqa [r14+rax], ymm3
	vfmadd231pd ymm2, ymm1, ymm3
	vmovdqa [rdi+rax], ymm2
	add eax, 32
	cmp eax, ebx
	jl ._next_first_order_step
	; xmm0	solver->time+solver->time_step/2
	; rdi	k5
	; rsi	k2
	vfmadd132pd xmm0, xmm1, [r15+32]
	lea rsi, [r14+rbx]
	vzeroupper
	call qword [r15]
	; rax	index
	; rdi	k5
	; rsi	k2
	; ymm0	{solver->time_step}
	; ymm1	{1/4}
	; ymm2	scratch #1
	; ymm3	scratch #2
	; xmm4	1/2
	vbroadcastsd ymm0, [r15+40]
	vbroadcastsd ymm1, [rel _packed_vector_data+8]
	vmovsd xmm4, [rel _packed_vector_data]
	xor eax, eax
	lea rdi, [r14+rbx*4]
	lea rsi, [r14+rbx]
._next_second_order_step:
	vmulpd ymm3, ymm0, [rsi+rax]
	vmovapd ymm2, [r12+rax]
	vmovdqa [rsi+rax], ymm3
	vaddpd ymm3, [r14+rax]
	vfmadd231pd ymm2, ymm1, ymm3
	vmovdqa [rdi+rax], ymm2
	add eax, 32
	cmp eax, ebx
	jl ._next_second_order_step
	; xmm0	solver->time+solver->time_step/2
	; rdi	k5
	; rsi	k3
	vfmadd132pd xmm0, xmm4, [r15+32]
	add rsi, rbx
	vzeroupper
	call qword [r15]
	; rax	index
	; rcx	k2
	; rdi	k5
	; rsi	k3
	; ymm0	{solver->time_step}
	; ymm1	scratch #1
	; ymm2	scratch #2
	vbroadcastsd ymm0, [r15+40]
	xor eax, eax
	lea rcx, [r14+rbx]
	lea rdi, [r14+rbx*4]
	lea rsi, [r14+rbx*2]
._next_third_order_step:
	vmulpd ymm2, ymm0, [rsi+rax]
	vmovapd ymm1, [r12+rax]
	vmovdqa [rsi+rax], ymm2
	vsubpd ymm1, [rcx+rax]
	vaddpd ymm2, ymm2
	vaddpd ymm1, ymm2
	vmovdqa [rdi+rax], ymm1
	add eax, 32
	cmp eax, ebx
	jl ._next_third_order_step
	; xmm0	solver->time+solver->time_step
	; rdi	k5
	; rsi	k4
	vaddpd xmm0, [r15+32]
	add rsi, rbx
	vzeroupper
	call qword [r15]
	; rax	index
	; rcx	k2
	; rdi	k6
	; rsi	k4
	; ymm0	{solver->time_step}
	; ymm1	{7/27}
	; ymm2	{10/27}
	; ymm3	{1/27}
	; ymm4	scratch #1
	; ymm5	scratch #2
	; xmm6	2/3
	vbroadcastsd ymm0, [r15+40]
	vbroadcastsd ymm1, [rel _packed_vector_data+16]
	vbroadcastsd ymm2, [rel _packed_vector_data+24]
	vbroadcastsd ymm3, [rel _packed_vector_data+32]
	vmovsd xmm6, [rel _packed_vector_data+40]
	xor eax, eax
	lea rcx, [r14+rbx]
	lea rdi, [rcx+rbx*4]
	lea rsi, [rcx+rbx*2]
._next_fourth_order_step:
	vmulpd ymm5, ymm0, [rsi+rax]
	vmovapd ymm4, [r12+rax]
	vmovdqa [rsi+rax], ymm5
	vfmadd231pd ymm4, ymm1, [r14+rax]
	vmulpd ymm5, ymm3
	vfmadd231pd ymm4, ymm2, [rcx+rax]
	vaddpd ymm4, ymm5
	vmovdqa [rdi+rax], ymm4
	add eax, 32
	cmp eax, ebx
	jl ._next_fourth_order_step
	; xmm0	solver->time+solver->time_step*2/3
	; rdi	k6
	; rsi	k5
	vfmadd132pd xmm0, xmm6, [r15+32]
	add rsi, rbx
	vzeroupper
	call qword [r15]
	; rax	index
	; rcx	k3
	; rdx	k4
	; rdi	k2
	; rsi	k6
	; r8	k5
	; ymm0	{solver->time_step}
	; ymm1	{28/625}
	; ymm2	{1/5}
	; ymm3	{546/625}
	; ymm4	{54/625}
	; ymm5	{378/625}
	; ymm6	scratch #1
	; ymm7	scratch #2
	vbroadcastsd ymm0, [r15+40]
	vbroadcastsd ymm1, [rel _packed_vector_data+48]
	vbroadcastsd ymm2, [rel _packed_vector_data+56]
	vbroadcastsd ymm3, [rel _packed_vector_data+64]
	vbroadcastsd ymm4, [rel _packed_vector_data+72]
	vbroadcastsd ymm5, [rel _packed_vector_data+80]
	xor eax, eax
	lea rcx, [r14+rbx*2]
	lea rdx, [rcx+rbx]
	lea rdi, [r14+rbx]
	lea rsi, [rdi+rbx*4]
	lea r8, [r14+rbx*4]
._next_fifth_order_step:
	vmulpd ymm7, ymm0, [r8+rax]
	vmovapd ymm6, [r12+rax]
	vmovdqa [r8+rax], ymm7
	vfmadd231pd ymm6, ymm1, [r14+rax]
	vmulpd ymm7, ymm5
	vfnmadd231pd ymm6, ymm2, [rdi+rax]
	vfmsub231pd ymm7, ymm4, [rdx+rax]
	vfmadd231pd ymm6, ymm3, [rcx+rax]
	vaddpd ymm6, ymm7
	vmovdqa [rdi+rax], ymm6
	add eax, 32
	cmp eax, ebx
	jl ._next_fifth_order_step
	; xmm0	solver->time+solver->time_step/5
	; rdi	k2
	; rsi	k6
	vfmadd132pd xmm0, xmm2, [r15+32]
	vzeroupper
	call qword [r15]
	; rax	index
	; rcx	k3
	; rdx	k4
	; rdi	k5
	; rsi	k6
	; ymm0	{solver->time_step}
	; ymm1	{1/8}
	; ymm2	{2/3}
	; ymm3	{1/16}
	; ymm4	{27/56}
	; ymm5	{125/336}
	; ymm6	acc
	; ymm7	scratch #1
	; ymm8	scratch #2
	vbroadcastsd ymm0, [r15+40]
	vbroadcastsd ymm1, [rel _packed_vector_data+88]
	vbroadcastsd ymm2, [rel _packed_vector_data+40]
	vbroadcastsd ymm3, [rel _packed_vector_data+96]
	vbroadcastsd ymm4, [rel _packed_vector_data+104]
	vbroadcastsd ymm5, [rel _packed_vector_data+112]
	vpxor ymm6, ymm6
	xor eax, eax
	lea rcx, [r14+rbx*2]
	lea rdx, [rcx+rbx]
	lea rdi, [r14+rbx*4]
	lea rsi, [rdi+rbx]
._next_error_step:
	vmulpd ymm8, ymm0, [rsi+rax]
	vmovapd ymm7, [r12+rax]
	vmovdqa [rsi+rax], ymm8
	vfmadd231pd ymm7, ymm1, [r14+rax]
	vmulpd ymm8, ymm5
	vfmadd231pd ymm7, ymm2, [rcx+rax]
	vfmadd231pd ymm8, ymm4, [rdi+rax]
	vfmadd231pd ymm7, ymm3, [rdx+rax]
	vsubpd ymm7, ymm8
	vfmadd231pd ymm6, ymm7, ymm7
	add eax, 32
	cmp eax, ebx
	jl ._next_error_step
	; xmm0	h/sqrt(hsum(acc))
	; xmm1	1/5
	; xmm2	scratch #1
	; xmm6	scratch #2
	vextractf128 xmm2, ymm6, 1
	vmovsd xmm1, [rel _packed_vector_data+56]
	vaddpd xmm6, xmm2
	vunpckhpd xmm2, xmm6
	vaddsd xmm2, xmm6
	vsqrtsd xmm2, xmm2
	vdivsd xmm0, xmm2
	vzeroupper
	call [rel pow wrt ..got]
	; xmm0	h_new
	; xmm1	h
	vmovsd xmm1, [r15+40]
	vmulsd xmm0, [rel _packed_vector_data+120]
	vmulsd xmm0, xmm1
	vmaxsd xmm0, xmm0, [r15+48]
	vmovsd [r15+40], xmm0
	vcomisd xmm0, xmm1
	jc ._next_step
	vaddsd xmm1, [r15+32]
	vmovsd [r15+32], xmm1
	; rax	index
	; rcx	k4
	; rdx	k5
	; rdi	k6
	; ymm0	{1/24}
	; ymm1	{5/48}
	; ymm2	{27/56}
	; ymm3	{125/336}
	; ymm4	scratch #1
	; ymm5	scratch #2
	vbroadcastsd ymm0, [rel _packed_vector_data+128]
	vbroadcastsd ymm1, [rel _packed_vector_data+136]
	vbroadcastsd ymm2, [rel _packed_vector_data+104]
	vbroadcastsd ymm3, [rel _packed_vector_data+112]
	xor eax, eax
	lea rcx, [r14+rbx*2]
	add rcx, rbx
	lea rdx, [r14+rbx*4]
	lea rdi, [rdx+rbx]
._next_output_step:
	vmovapd ymm4, [r12+rax]
	vmulpd ymm5, ymm2, [rdx+rax]
	vfmadd231pd ymm4, ymm0, [r14+rax]
	vfmadd231pd ymm5, ymm3, [rdi+rax]
	vfmadd231pd ymm4, ymm1, [rcx+rax]
	vaddpd ymm4, ymm5
	vmovdqa [r13+rax], ymm4
	add eax, 32
	cmp eax, ebx
	jl ._next_output_step
	mov r12, r13
	lea r13, [r12+rbx]
	sub dword [rsp], 1
	jnz ._next_step
	pop rax
	pop rbx
	pop r12
	pop r13
	pop r14
	pop r15
	pop rbp
	vzeroupper
	ret



align_code 32
_packed_vector_data:
	dq 0x3fe0000000000000 ; 1/2
	dq 0x3fd0000000000000 ; 1/4
	dq 0x3fd097b425ed097b ; 7/27
	dq 0x3fd7b425ed097b42 ; 10/27
	dq 0x3fa2f684bda12f68 ; 1/27
	dq 0x3fe5555555555555 ; 2/3
	dq 0x3fa6f0068db8bac7 ; 28/625
	dq 0x3fc999999999999a ; 1/5
	dq 0x3febf487fcb923a3 ; 546/625
	dq 0x3fb61e4f765fd8ae ; 54/625
	dq 0x3fe35a858793dd98 ; 378/625
	dq 0x3fc0000000000000 ; 1/8
	dq 0x3fb0000000000000 ; 1/16
	dq 0x3fdedb6db6db6db7 ; 27/56
	dq 0x3fd7cf3cf3cf3cf4 ; 125/336
	dq 0x3feccccccccccccd ; 9/10
	dq 0x3fa5555555555555 ; 1/24
	dq 0x3fbaaaaaaaaaaaab ; 5/48
