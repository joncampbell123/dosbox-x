bits 16
org 100h

start:
    mov si, source
    mov di, destination
    mov cx, 2
    rep movsb
    nop
    int 0x20

source:
    db 0x41, 0x42
destination:
    dw 0
