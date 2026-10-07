bits 16
org 100h

; Makes a fake PSP that is its own parent the current PSP, then loops. The
; agent cannot trace that PSP back to the DOS shell, so session.stop refuses
; to end the program and DOSBox-X must be restarted.
start:
    mov ax, cs
    add ax, (fake_psp - $$ + 0x100) / 16
    mov es, ax
    mov [es:0x16], ax       ; the fake PSP's parent is itself
    mov bx, ax
    mov ah, 0x50
    int 0x21                ; make it the current PSP
spin:
    jmp spin

    align 16
fake_psp:
    db 0xcd, 0x20           ; INT 20h, as at the start of every PSP
    times 0x100 - 2 db 0
