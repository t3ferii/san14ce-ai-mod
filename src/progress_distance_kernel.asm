; Private leaf kernel. ECX/EDX inputs, EAX result, volatile GPRs only.
; Both unsigned bounds checks precede table access and preserve the arguments.
; Sentinel immediates are replaced before the relay becomes executable.
.code
PUBLIC progress_kernel_begin
progress_kernel_begin PROC
    cmp ecx, 48400
    jae original_fallback
    cmp edx, 48400
    jae original_fallback
    mov r11, 1122334455667788h
    movsx eax, word ptr [r11+rdx*4]
    movsx r8d, word ptr [r11+rcx*4]
    sub eax, r8d
    movsx r8d, word ptr [r11+rdx*4+2]
    movsx r9d, word ptr [r11+rcx*4+2]
    sub r8d, r9d
    mov r9d, eax
    neg r9d
    cmovs r9d, eax
    mov eax, r8d
    neg eax
    cmovs eax, r8d
    cmp eax, r9d
    cmovl eax, r9d
    add eax, r9d
    shr eax, 1
    ret
original_fallback:
    db 0ffh,025h,0,0,0,0
    dq 8877665544332211h
progress_kernel_begin ENDP
END
