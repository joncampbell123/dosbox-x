bits 16
org 100h

start:
    call callee
    mov bx, 0x5678
    int 0x20

callee:
    mov ax, 0x1234
    ret
