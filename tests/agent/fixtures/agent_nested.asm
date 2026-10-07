bits 16
org 100h

; Runs AGENTRUN.COM (an endless loop) as a child program, then loops itself.
; session.stop while the child runs ends only the child, so this parent keeps
; running and DEBUGBOX never returns.
start:
    mov sp, stack_top
    mov bx, (end_of_program - $$ + 0x100 + 15) / 16
    mov ah, 0x4a
    int 0x21                ; shrink our memory block so the child can load
    mov [params + 4], cs
    mov [params + 8], cs
    mov [params + 12], cs
    mov dx, child
    mov bx, params
    mov ax, 0x4b00
    int 0x21                ; run the child
parent_loop:
    jmp parent_loop

child:
    db "AGENTRUN.COM", 0
params:
    dw 0                    ; inherit the environment
    dw tail, 0
    dw fcb, 0
    dw fcb, 0
tail:
    db 0, 0x0d
fcb:
    times 16 db 0
    times 256 db 0
stack_top:
end_of_program:
