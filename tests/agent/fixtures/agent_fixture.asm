bits 16
org 100h

start:
    mov ax, 0x1234
    mov bx, 0x5678
    mov si, 0x0200
    mov byte [si], 0x41
    mov word [si + 1], 0x4243
    nop
    int 0x20
