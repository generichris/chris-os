bits 32

global sched_switch
global sched_jump

; void sched_switch(uint32_t *old_esp, uint32_t new_esp)
; saves callee-saved regs + eflags onto current stack, stores esp,
; loads new stack, restores and returns into new thread
sched_switch:
    pushf
    push ebx
    push esi
    push edi
    push ebp

    mov eax, [esp + 24]     ; arg0: &old_esp (5 regs * 4 = 20, + 4 ret = 24)
    mov [eax], esp

    mov esp, [esp + 28]     ; arg1: new_esp  (24 + 4 for the slot we just read)

    pop ebp
    pop edi
    pop esi
    pop ebx
    popf

    ret

; void sched_jump(uint32_t new_esp)
; used by sched_exit: abandons current stack entirely, jumps into next thread
sched_jump:
    mov esp, [esp + 4]

    pop ebp
    pop edi
    pop esi
    pop ebx
    popf

    ret
