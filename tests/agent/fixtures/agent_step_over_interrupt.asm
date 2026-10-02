bits 16
org 100h

start:
    int 0x2f
    mov bx, 0x5678
    int 0x20
