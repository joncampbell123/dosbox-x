bits 16
org 100h

; Runs "COMMAND /C AGENTRUN.COM" (an endless loop), then loops itself.
; COMMAND.COM is a built-in program that runs as native code, so
; session.stop must let it finish before ending this parent.
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
    db "Z:\COMMAND.COM", 0
params:
    dw 0                    ; inherit the environment
    dw tail, 0
    dw fcb, 0
    dw fcb, 0
tail:
    db tail_end - tail - 2, " /C AGENTRUN.COM", 0x0d
tail_end:
fcb:
    times 16 db 0
    times 256 db 0
stack_top:
end_of_program:
