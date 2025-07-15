data32 SEGMENT align(32)
maskLo DB 32 DUP(0fh)
data32 ENDS

.code

ALIGN 16

;rcx = ptrWeight, rdx = ptrInput, r8 = ptrDst,
;r9 = ptrSumIn, rsp+0x28 = ptrZpWgt, 
;rsp+0x30 = ptrScaleIn, rsp+0x38 = ptrScaleWgt,
;rsp+0x40 = totalN
GemmW4A8OVAsm PROC FRAME
    ; backup frampointer, rsp = rbp+0x08
    push rbp
.pushreg rbp
    mov rbp, rsp
.setframe rbp, 0
    ; backup GRF
    push rbx
.pushreg rbx
    push r12
.pushreg r12
    push r13
.pushreg r13
    push r14
.pushreg r14
    push r15
.pushreg r15
    push rdi
.pushreg rdi
    push rsi
.pushreg rsi
.endprolog
    ; backup xmm6-xmm15
    sub rsp, 100h
    vmovdqu xmmword ptr [rsp], xmm6
    vmovdqu xmmword ptr [rsp+10h], xmm7
    vmovdqu xmmword ptr [rsp+20h], xmm8
    vmovdqu xmmword ptr [rsp+30h], xmm9
    vmovdqu xmmword ptr [rsp+40h], xmm10
    vmovdqu xmmword ptr [rsp+50h], xmm11
    vmovdqu xmmword ptr [rsp+60h], xmm12
    vmovdqu xmmword ptr [rsp+70h], xmm13
    vmovdqu xmmword ptr [rsp+80h], xmm14
    vmovdqu xmmword ptr [rsp+90h], xmm15

    ; r15 = totalN
    mov r15d, [rbp+48h]

    ; load output, reserve stack for output
    vmovups ymm14, [r8]
    vmovups ymm13, [r8+20h]
    vmovups ymm12, [r8+40h]
    vmovups ymm11, [r8+60h]
    sub rsp, 80h

unrollN_label:
    ALIGN 16
    ; unrollN = 0
    
    ; ymm15 = maskLo
    vmovdqa ymm15, ymmword ptr [maskLo]
    ; spill output to stack, initialized zero to accum
    vmovups ymmword ptr [rsp], ymm14
    vxorps  ymm14, ymm14, ymm14
    vmovups ymmword ptr [rsp+20h], ymm13
    vxorps  ymm13, ymm13, ymm13
    vmovups ymmword ptr [rsp+40h], ymm12
    vxorps  ymm12, ymm12, ymm12
    vmovups ymmword ptr [rsp+60h], ymm11
    vxorps  ymm11, ymm11, ymm11

    ; loopN = 0
    ; load wgt get lo
    prefetcht0 [rcx+200h]
    vmovups ymm10, ymmword ptr [rcx]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+20h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+40h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+60h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+200h]
    vmovups ymm10, ymmword ptr [rcx]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+20h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+40h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+60h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+4]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 1
    ; load wgt get lo
    prefetcht0 [rcx+280h]
    vmovups ymm10, ymmword ptr [rcx+80h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+0a0h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+0c0h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+0e0h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+8]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+280h]
    vmovups ymm10, ymmword ptr [rcx+80h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+0a0h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+0c0h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+0e0h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+0ch]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 2
    ; load wgt get lo
    prefetcht0 [rcx+300h]
    vmovups ymm10, ymmword ptr [rcx+100h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+120h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+140h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+160h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+10h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+300h]
    vmovups ymm10, ymmword ptr [rcx+100h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+120h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+140h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+160h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+14h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 3
    ; load wgt get lo
    prefetcht0 [rcx+380h]
    vmovups ymm10, ymmword ptr [rcx+180h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+1a0h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+1c0h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+1e0h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+18h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+380h]
    vmovups ymm10, ymmword ptr [rcx+180h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+1a0h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+1c0h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+1e0h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+1ch]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; ymm10 = zpWgt
    mov r12, qword ptr [rbp+30h]
    movzx r13d, byte ptr [r12]
    vmovq xmm10, r13
    vbroadcastss ymm10, xmm10
    ; ymm0 = sumIn
    vbroadcastss ymm0, dword ptr [r9]
    ; accum -= zpWgt * sumIn
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm14, ymm14, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm13, ymm13, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm12, ymm12, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm11, ymm11, ymm15

    ; r12 = scaleIn, r13 = scaleWgt
    mov r13, qword ptr [rbp+40h]
    mov r12, qword ptr [rbp+38h]
    ; ymm0 = scaleIn
    vbroadcastss ymm0, dword ptr [r12]
    ; ymm10 = scaleWgt, accum to f32 and fma to output
    vmovups ymm10, ymmword ptr [r13]
    vcvtdq2ps ymm14, ymm14
    vmulps  ymm15, ymm14, ymm0
    vmovups ymm14, ymmword ptr [rsp]
    vfmadd231ps ymm14, ymm15, ymm10
    vmovups ymm9, ymmword ptr [r13+20h]
    vcvtdq2ps ymm13, ymm13
    vmulps  ymm15, ymm13, ymm0
    vmovups ymm13, ymmword ptr [rsp+20h]
    vfmadd231ps ymm13, ymm15, ymm9
    vmovups ymm8, ymmword ptr [r13+40h]
    vcvtdq2ps ymm12, ymm12
    vmulps  ymm15, ymm12, ymm0
    vmovups ymm12, ymmword ptr [rsp+40h]
    vfmadd231ps ymm12, ymm15, ymm8
    vmovups ymm7, ymmword ptr [r13+60h]
    vcvtdq2ps ymm11, ymm11
    vmulps  ymm15, ymm11, ymm0
    vmovups ymm11, ymmword ptr [rsp+60h]
    vfmadd231ps ymm11, ymm15, ymm7

    ; adjust ptr for in, wgt, sumIn, scaleIn
    add rcx, 200h
    add rdx, 20h
    add r9, 4
    mov r12, qword ptr [rbp+38h]
    add r12, 4
    mov qword ptr [rbp+38h], r12
    

    ; unrollN = 1
    
    ; ymm15 = maskLo
    vmovdqa ymm15, ymmword ptr [maskLo]
    ; spill output to stack, initialized zero to accum
    vmovups ymmword ptr [rsp], ymm14
    vxorps  ymm14, ymm14, ymm14
    vmovups ymmword ptr [rsp+20h], ymm13
    vxorps  ymm13, ymm13, ymm13
    vmovups ymmword ptr [rsp+40h], ymm12
    vxorps  ymm12, ymm12, ymm12
    vmovups ymmword ptr [rsp+60h], ymm11
    vxorps  ymm11, ymm11, ymm11

    ; loopN = 0
    ; load wgt get lo
    prefetcht0 [rcx+200h]
    vmovups ymm10, ymmword ptr [rcx]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+20h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+40h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+60h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+200h]
    vmovups ymm10, ymmword ptr [rcx]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+20h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+40h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+60h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+4]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 1
    ; load wgt get lo
    prefetcht0 [rcx+280h]
    vmovups ymm10, ymmword ptr [rcx+80h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+0a0h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+0c0h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+0e0h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+8]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+280h]
    vmovups ymm10, ymmword ptr [rcx+80h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+0a0h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+0c0h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+0e0h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+0ch]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 2
    ; load wgt get lo
    prefetcht0 [rcx+300h]
    vmovups ymm10, ymmword ptr [rcx+100h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+120h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+140h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+160h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+10h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+300h]
    vmovups ymm10, ymmword ptr [rcx+100h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+120h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+140h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+160h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+14h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 3
    ; load wgt get lo
    prefetcht0 [rcx+380h]
    vmovups ymm10, ymmword ptr [rcx+180h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+1a0h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+1c0h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+1e0h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+18h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+380h]
    vmovups ymm10, ymmword ptr [rcx+180h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+1a0h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+1c0h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+1e0h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+1ch]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; ymm10 = zpWgt
    mov r12, qword ptr [rbp+30h]
    movzx r13d, byte ptr [r12]
    vmovq xmm10, r13
    vbroadcastss ymm10, xmm10
    ; ymm0 = sumIn
    vbroadcastss ymm0, dword ptr [r9]
    ; accum -= zpWgt * sumIn
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm14, ymm14, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm13, ymm13, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm12, ymm12, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm11, ymm11, ymm15

    ; r12 = scaleIn, r13 = scaleWgt
    mov r13, qword ptr [rbp+40h]
    mov r12, qword ptr [rbp+38h]
    ; ymm0 = scaleIn
    vbroadcastss ymm0, dword ptr [r12]
    ; ymm10 = scaleWgt, accum to f32 and fma to output
    vmovups ymm10, ymmword ptr [r13]
    vcvtdq2ps ymm14, ymm14
    vmulps  ymm15, ymm14, ymm0
    vmovups ymm14, ymmword ptr [rsp]
    vfmadd231ps ymm14, ymm15, ymm10
    vmovups ymm9, ymmword ptr [r13+20h]
    vcvtdq2ps ymm13, ymm13
    vmulps  ymm15, ymm13, ymm0
    vmovups ymm13, ymmword ptr [rsp+20h]
    vfmadd231ps ymm13, ymm15, ymm9
    vmovups ymm8, ymmword ptr [r13+40h]
    vcvtdq2ps ymm12, ymm12
    vmulps  ymm15, ymm12, ymm0
    vmovups ymm12, ymmword ptr [rsp+40h]
    vfmadd231ps ymm12, ymm15, ymm8
    vmovups ymm7, ymmword ptr [r13+60h]
    vcvtdq2ps ymm11, ymm11
    vmulps  ymm15, ymm11, ymm0
    vmovups ymm11, ymmword ptr [rsp+60h]
    vfmadd231ps ymm11, ymm15, ymm7

    ; adjust ptr for in, wgt, sumIn, scaleIn
    add rcx, 200h
    add rdx, 20h
    add r9, 4
    mov r12, qword ptr [rbp+38h]
    add r12, 4
    mov qword ptr [rbp+38h], r12
    

    ; unrollN = 2
    
    ; ymm15 = maskLo
    vmovdqa ymm15, ymmword ptr [maskLo]
    ; spill output to stack, initialized zero to accum
    vmovups ymmword ptr [rsp], ymm14
    vxorps  ymm14, ymm14, ymm14
    vmovups ymmword ptr [rsp+20h], ymm13
    vxorps  ymm13, ymm13, ymm13
    vmovups ymmword ptr [rsp+40h], ymm12
    vxorps  ymm12, ymm12, ymm12
    vmovups ymmword ptr [rsp+60h], ymm11
    vxorps  ymm11, ymm11, ymm11

    ; loopN = 0
    ; load wgt get lo
    prefetcht0 [rcx+200h]
    vmovups ymm10, ymmword ptr [rcx]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+20h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+40h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+60h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+200h]
    vmovups ymm10, ymmword ptr [rcx]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+20h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+40h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+60h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+4]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 1
    ; load wgt get lo
    prefetcht0 [rcx+280h]
    vmovups ymm10, ymmword ptr [rcx+80h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+0a0h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+0c0h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+0e0h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+8]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+280h]
    vmovups ymm10, ymmword ptr [rcx+80h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+0a0h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+0c0h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+0e0h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+0ch]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 2
    ; load wgt get lo
    prefetcht0 [rcx+300h]
    vmovups ymm10, ymmword ptr [rcx+100h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+120h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+140h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+160h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+10h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+300h]
    vmovups ymm10, ymmword ptr [rcx+100h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+120h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+140h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+160h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+14h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 3
    ; load wgt get lo
    prefetcht0 [rcx+380h]
    vmovups ymm10, ymmword ptr [rcx+180h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+1a0h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+1c0h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+1e0h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+18h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+380h]
    vmovups ymm10, ymmword ptr [rcx+180h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+1a0h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+1c0h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+1e0h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+1ch]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; ymm10 = zpWgt
    mov r12, qword ptr [rbp+30h]
    movzx r13d, byte ptr [r12]
    vmovq xmm10, r13
    vbroadcastss ymm10, xmm10
    ; ymm0 = sumIn
    vbroadcastss ymm0, dword ptr [r9]
    ; accum -= zpWgt * sumIn
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm14, ymm14, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm13, ymm13, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm12, ymm12, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm11, ymm11, ymm15

    ; r12 = scaleIn, r13 = scaleWgt
    mov r13, qword ptr [rbp+40h]
    mov r12, qword ptr [rbp+38h]
    ; ymm0 = scaleIn
    vbroadcastss ymm0, dword ptr [r12]
    ; ymm10 = scaleWgt, accum to f32 and fma to output
    vmovups ymm10, ymmword ptr [r13]
    vcvtdq2ps ymm14, ymm14
    vmulps  ymm15, ymm14, ymm0
    vmovups ymm14, ymmword ptr [rsp]
    vfmadd231ps ymm14, ymm15, ymm10
    vmovups ymm9, ymmword ptr [r13+20h]
    vcvtdq2ps ymm13, ymm13
    vmulps  ymm15, ymm13, ymm0
    vmovups ymm13, ymmword ptr [rsp+20h]
    vfmadd231ps ymm13, ymm15, ymm9
    vmovups ymm8, ymmword ptr [r13+40h]
    vcvtdq2ps ymm12, ymm12
    vmulps  ymm15, ymm12, ymm0
    vmovups ymm12, ymmword ptr [rsp+40h]
    vfmadd231ps ymm12, ymm15, ymm8
    vmovups ymm7, ymmword ptr [r13+60h]
    vcvtdq2ps ymm11, ymm11
    vmulps  ymm15, ymm11, ymm0
    vmovups ymm11, ymmword ptr [rsp+60h]
    vfmadd231ps ymm11, ymm15, ymm7

    ; adjust ptr for in, wgt, sumIn, scaleIn
    add rcx, 200h
    add rdx, 20h
    add r9, 4
    mov r12, qword ptr [rbp+38h]
    add r12, 4
    mov qword ptr [rbp+38h], r12
    

    ; unrollN = 3
    
    ; ymm15 = maskLo
    vmovdqa ymm15, ymmword ptr [maskLo]
    ; spill output to stack, initialized zero to accum
    vmovups ymmword ptr [rsp], ymm14
    vxorps  ymm14, ymm14, ymm14
    vmovups ymmword ptr [rsp+20h], ymm13
    vxorps  ymm13, ymm13, ymm13
    vmovups ymmword ptr [rsp+40h], ymm12
    vxorps  ymm12, ymm12, ymm12
    vmovups ymmword ptr [rsp+60h], ymm11
    vxorps  ymm11, ymm11, ymm11

    ; loopN = 0
    ; load wgt get lo
    prefetcht0 [rcx+200h]
    vmovups ymm10, ymmword ptr [rcx]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+20h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+40h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+60h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+200h]
    vmovups ymm10, ymmword ptr [rcx]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+20h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+40h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+60h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+4]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 1
    ; load wgt get lo
    prefetcht0 [rcx+280h]
    vmovups ymm10, ymmword ptr [rcx+80h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+0a0h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+0c0h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+0e0h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+8]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+280h]
    vmovups ymm10, ymmword ptr [rcx+80h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+0a0h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+0c0h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+0e0h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+0ch]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 2
    ; load wgt get lo
    prefetcht0 [rcx+300h]
    vmovups ymm10, ymmword ptr [rcx+100h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+120h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+140h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+160h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+10h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+300h]
    vmovups ymm10, ymmword ptr [rcx+100h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+120h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+140h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+160h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+14h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; loopN = 3
    ; load wgt get lo
    prefetcht0 [rcx+380h]
    vmovups ymm10, ymmword ptr [rcx+180h]
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+1a0h]
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+1c0h]
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+1e0h]
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+18h]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0
    ; load wgt get hi
    prefetcht0 [rcx+380h]
    vmovups ymm10, ymmword ptr [rcx+180h]
    vpsrld  ymm10, ymm10, 4
    vandps  ymm10, ymm10, ymm15
    vmovups ymm9, ymmword ptr [rcx+1a0h]
    vpsrld  ymm9, ymm9, 4
    vandps  ymm9, ymm9, ymm15
    vmovups ymm8, ymmword ptr [rcx+1c0h]
    vpsrld  ymm8, ymm8, 4
    vandps  ymm8, ymm8, ymm15
    vmovups ymm7, ymmword ptr [rcx+1e0h]
    vpsrld  ymm7, ymm7, 4
    vandps  ymm7, ymm7, ymm15
    ; load input
    vpbroadcastd ymm0, dword ptr [rdx+1ch]
    ; dp4a
    vex     vpdpbusd ymm14, ymm10, ymm0
    vex     vpdpbusd ymm13, ymm9, ymm0
    vex     vpdpbusd ymm12, ymm8, ymm0
    vex     vpdpbusd ymm11, ymm7, ymm0

    ; ymm10 = zpWgt
    mov r12, qword ptr [rbp+30h]
    movzx r13d, byte ptr [r12]
    vmovq xmm10, r13
    vbroadcastss ymm10, xmm10
    ; ymm0 = sumIn
    vbroadcastss ymm0, dword ptr [r9]
    ; accum -= zpWgt * sumIn
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm14, ymm14, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm13, ymm13, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm12, ymm12, ymm15
    vpmulld ymm15, ymm0, ymm10
    vpsubd  ymm11, ymm11, ymm15

    ; r12 = scaleIn, r13 = scaleWgt
    mov r13, qword ptr [rbp+40h]
    mov r12, qword ptr [rbp+38h]
    ; ymm0 = scaleIn
    vbroadcastss ymm0, dword ptr [r12]
    ; ymm10 = scaleWgt, accum to f32 and fma to output
    vmovups ymm10, ymmword ptr [r13]
    vcvtdq2ps ymm14, ymm14
    vmulps  ymm15, ymm14, ymm0
    vmovups ymm14, ymmword ptr [rsp]
    vfmadd231ps ymm14, ymm15, ymm10
    vmovups ymm9, ymmword ptr [r13+20h]
    vcvtdq2ps ymm13, ymm13
    vmulps  ymm15, ymm13, ymm0
    vmovups ymm13, ymmword ptr [rsp+20h]
    vfmadd231ps ymm13, ymm15, ymm9
    vmovups ymm8, ymmword ptr [r13+40h]
    vcvtdq2ps ymm12, ymm12
    vmulps  ymm15, ymm12, ymm0
    vmovups ymm12, ymmword ptr [rsp+40h]
    vfmadd231ps ymm12, ymm15, ymm8
    vmovups ymm7, ymmword ptr [r13+60h]
    vcvtdq2ps ymm11, ymm11
    vmulps  ymm15, ymm11, ymm0
    vmovups ymm11, ymmword ptr [rsp+60h]
    vfmadd231ps ymm11, ymm15, ymm7

    ; adjust ptr for in, wgt, sumIn, scaleIn
    add rcx, 200h
    add rdx, 20h
    add r9, 4
    mov r12, qword ptr [rbp+38h]
    add r12, 4
    mov qword ptr [rbp+38h], r12


    ; adjust totalN
    sub r15, 80h
    cmp r15, 0
    jg unrollN_label

    ; store output
    vmovups [r8], ymm14
    vmovups [r8+20h], ymm13
    vmovups [r8+40h], ymm12
    vmovups [r8+60h], ymm11
    add rsp, 80h

    ; restore xmm6-xmm15
    vmovdqu xmm6,  xmmword ptr [rsp]
    vmovdqu xmm7,  xmmword ptr [rsp+10h]
    vmovdqu xmm8,  xmmword ptr [rsp+20h]
    vmovdqu xmm9,  xmmword ptr [rsp+30h]
    vmovdqu xmm10, xmmword ptr [rsp+40h]
    vmovdqu xmm11, xmmword ptr [rsp+50h]
    vmovdqu xmm12, xmmword ptr [rsp+60h]
    vmovdqu xmm13, xmmword ptr [rsp+70h]
    vmovdqu xmm14, xmmword ptr [rsp+80h]
    vmovdqu xmm15, xmmword ptr [rsp+90h]
    add rsp, 100h
    vzeroupper
    ; restore GRF
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ; restore frampointer
    mov rsp, rbp
    pop rbp
    ret

GemmW4A8OVAsm ENDP

end