bits 16
org 100h

; Enters 16-bit protected mode, then enables paging with only the first 64 KiB
; identity-mapped. The debugger stops at pm_paged for memory-map RPC checks.
start:
    cli
    xor ax, ax
    mov es, ax

    ; Page directory at 0000:4000 and its first page table at 0000:5000.
    mov word [es:0x4000], 0x5003
    mov word [es:0x4002], 0
    mov word [es:0x4004], 0
    mov di, 0x5000
    mov cx, 16
    xor bx, bx
.map_page:
    mov ax, bx
    or ax, 3
    mov [es:di], ax
    mov word [es:di + 2], 0
    add bx, 0x1000
    add di, 4
    loop .map_page

    ; DEBUGBOX can load a COM image at a different conventional-memory segment.
    ; Build the GDT base and protected-mode far target from CS at runtime.
    xor eax, eax
    mov ax, cs
    shl eax, 4
    add eax, gdt
    mov [cs:gdt_descriptor + 2], eax
    sub eax, gdt
    add eax, pm_entry
    mov [cs:pm_far_pointer], ax

    lgdt [cs:gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp far [cs:pm_far_pointer]

pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov sp, 0x0ff0
    mov eax, 0x4000
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

pm_paged:
    xchg bx, bx
    jmp short pm_paged

align 4
gdt:
    dq 0
    ; selector 0x08: 16-bit flat executable/readable code, 64 KiB limit.
    dw 0xffff, 0
    db 0, 0x9a, 0, 0
    ; selector 0x10: 16-bit flat writable data/stack, 64 KiB limit.
    dw 0xffff, 0
    db 0, 0x92, 0, 0
    ; selector 0x18: writable data with a 0x000f byte limit.
    dw 0x000f, 0
    db 0, 0x92, 0, 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt - 1
    dd 0
pm_far_pointer:
    dw 0
    dw 0x08
