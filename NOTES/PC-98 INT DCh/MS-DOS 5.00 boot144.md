Entry point (MS-DOS 5.00) 1.44MB disk image (on my hard drive, boot144.dsk). Configuration menu option "nothing". 0ADC segment may change location based on configuration.

--

    STRUCT FUNCROW_ENTRY:
        +00h length of string
        +01h string to insert when key is pressed
        +0Fh NUL
        =10h ------

        NOTE: function key row will only show the first 6 chars

        NOTE: If the first byte is 0xFE, then the next 5 chars are display only
              and the string to insert starts at byte 6. Length field still
              covers the combined display + insert string.

    STRUCT EDITKEY_ENTRY:
        +00h length of string
        +01h string to insert when key is pressed
        +06h NUL (only 5 bytes accepted from application)
        +07h NUL
        =08h ------

--

    0060:0020 WORD MS-DOS product number        [see INT DCh CL=12h]
    0060:0022 WORD Internal revision number     [see INT DCh CL=15h AH=0]
    0060:002E WORD INT DCh / ANSI segment (in this dump, 0ADCh)
    0060:0030 WORD ?? (in this dump, 3800h)
    0060:0032 WORD Text VRAM segment (A000h)
    0060:0037 BYTE ??
    0060:0068 WORD RS-232C channel 0 AUX protocol [undocumented PC-98 webtech] [see INT DCh CL=0Ah, INT DCh CL=0Eh AH=01h]
    0060:008A BYTE kanji / graph mode flag
    0060:008B BYTE ??
    0060:00A5 BYTE ??
    0060:00A6 BYTE ??
    0060:00B4 BYTE INT DCh in-progress flag [undocumented PC-98 webtech]
    0060:010C BYTE CTRL+Fn shortcut inhibit bitfield
                   bit 0: inhibit Ctrl+Fn shortcuts         (CL=0Fh AX=0h/1h)
                   bit 1: ????                              (CL=0Fh AX=2h/3h)
    0060:0110 BYTE Cursor Y coordinate
    0060:0111 BYTE Function row display (0=off 1=on 2=shift function keys)
    0060:0112 BYTE Scroll range lower limit
    0060:0113 BYTE Number of screen lines
    0060:0114 BYTE Erasure attribute (usually E1h)
    0060:0115 BYTE kanji upper byte storage flag
    0060:0116 BYTE kanji upper byte
    0060:0117 BYTE Line wrap flag
    0060:0118 BYTE Scroll speed (0=normal 1=slow)
    0060:0119 WORD Erasure character (usually 20h)
    0060:011B BYTE Cursor display state (0=off 1=on)
    0060:011C BYTE Cursor X coordinate
    0060:011D BYTE Character attribute (i.e. controlled by <ESC>[m ) [INT DCh CL=10h AH=02h]
    0060:011E BYTE Scroll range upper limit
    0060:011F BYTE Scroll "weight" (delay, apparently?) (0001h = normal  E000h = slow)
    0060:0124 WORD ACFh BX value (?))
    0060:0128 BYTE ANSI escape handling state (0=normal 1=ESC 2>=ANSI processing)
    0060:0129 BYTE ANSI escape handling state (?)
    0060:012A BYTE ANSI escape handling state (?) 0 = ?   1 = ESC[>  2 = ESC[?  3 = ESC[=
    0060:012B BYTE Saved cursor attribute ( ESC [s )
    0060:0134 WORD ANSI escape handling pointer of some kind (?)
    0060:0136 BYTE drive number last accessed by IO.SYS block driver [INT DCh CL=13h]
    0060:013C WORD display attribute in extended attribute mode [INT DCh CL=10h AL=02h]
    0060:013E WORD erasure attribute in extended attribute mode [INT DCh CL=10h AL=02h]
    0060:014E BYTE some sort of flag
    0060:0162 WORD ??
    0060:0214 WORD:WORD 16-bit far pointer (0ADC:3126)
    0060:0268 WORD ??
    0060:05DB WORD stored AX value from caller
    0060:05DD WORD stored SS value from caller
    0060:05DF WORD stored SP value from caller (after INT DCh int frame and PUSH DS)
    0060:05E1 WORD stored DS value from caller
    0060:05E3 WORD stored DX value from caller
    0060:05E5 WORD stored BX value from caller
    0060:0767 Stack pointer (from DOS segment), stack switches to on entry to procedure
    0060:17FA WORD ??
    0060:1802 WORD ??
    0060:197A BYTE ??
    0060:1DC4 BYTE ??
    0060:2852 BYTE x 0x28 ANSI escape parsing state (such as, numeric values from ESC[m )
    0060:2A7A WORD ??
    0060:2A7C WORD ??
    0060:2C86 WORD x 0x1A ??
    0060:2D2E FUNCROW_ENTRY * 10            Function keys F1 to F10
    0060:2DCE FUNCROW_ENTRY * 5             Function keys VF1 to VF5
    0060:2E1E FUNCROW_ENTRY * 10            Function keys Shift-F1 to Shift-F10
    0060:2EBE FUNCROW_ENTRY * 5             Function keys Shift-VF1 to Shift-VF5
    0060:2F0E EDITKEY_ENTRY * 11            Editor keys. 11 entries that correspond to scan codes 36h to 40h inclusive.
                                            They correspond to: ROLL UP, ROLL DOWN, INS, DEL, UP ARROW, LEFT ARROW, RIGHT ARROW,
                                                                DOWN ARROW, HOME/CLR, HELP, AND KEYPAD -
    0060:2F86 FUNCROW_ENTRY * 10            Function keys Ctrl-F1 to Ctrl-F10
    0060:3026 FUNCROW_ENTRY * 5             Function keys Ctrl-VF1 to Ctrl-VF5
    0060:36B3 INT DCh entry point
    0060:3B30 Subroutine called on INT DCh if 0060:014E is nonzero

--

INT DC = 60:36B3

    0060:36B3:
        if BYTE PTR cs:[014E] == 0 jmp 0060:36BE
        CALL 3B30h
    0060:36BE:
        jmp far WORD PTR cs:[0214] (0ADC:3126)

--

    0060:3B30: (called by INT DCh entry point if BYTE PTR CS:[014E] != 0
        PUSH DS, ES, CX, SI, DI
        DS:SI = WORD PTR cs:[3B28] (FFFF:0090)
        ES:DI = WORD PTR cs:[3B2C] (0000:0080)
        ZF = ((result=_fmemcmp(DS:SI,ES:DI,16)) == 0) (CX = 8, REP CMPSW)   ; <- Testing A20 gate, apparently?
        POP DI, SI, CX, ES, DS
        IF result == 0 JMP 3B4Ch (if ZF=1)
        return
    0060:3B4C: (if _fmemcmp(DS:SI,ES:DI,16) == 0)
        PUSH AX, BX
        AH = 5
        CALL FAR WORD PTR cs:[014F]     ; <- ??? Pointer so far has been either 0000:0000 or FFFF:FFFF
        POP BX, AX
        return
    ; ^ NOTE: Not sure, but this may be a call into HIMEM.SYS (XMS), in which case AH = 5 means Local Enable A20

--

    0ADC:0030 WORD DOS kernel segment (60h)

--

    0ADC:0032:
        PUSH DS
        DS = WORD PTR CS:[0030]                             ; DOS kernel segment (60h)
        IF (BYTE PTR DS:[197A] & 3) == 0 JMP 61h            ; 0060:197A
    0ADC:003F:
        ES = 0
        CX = WORD PTR ES:[05EA]                             ; 0000:05EA Disk 0 Parameter table segment??
        IF CX != WORD PTR DS:[002E] JMP 52h                 ; ??
        DI = 5EAh
        WORD PTR ES:[DI] = AX, DI += 2                      ; STOSW
    0ADC:0052:
        CX = WORD PTR ES:[05EE]                             ; 0000:05EE Disk 1 Parameter table segment?
        IF CX != WORD PTR DS:[002E] JMP 61h
        DI = 5EEh
        WORD PTR ES:[DI] = AX, DI += 2                      ; STOSW
    0ADC:0061:
        POP DS
        ES = WORD PTR CS:[0030]                             ; DOS kernel segment (60h)
        WORD PTR ES:[002E] = AX                             ; ??
        DI = 162h
        CX = 36h
    0ADC:0071:
        WORD PTR ES:[DI] = AX, DI += 4                      ; STOSW ; INC DI ; INC DI
        IF CX > 0 THEN CX--, JMP 71h                        ; LOOP 71h
    0ADC:0076:
        return far                                          ; RETF

--

    0ADC:0A00 Subroutine lookup table
        Referred from 0ADC:0AAC, CL is single char or first byte of kanji
        CL=10h AH=00h, search by CL register from caller, or normal CON output.
                  .        .        .        .        .        .
    0ADC:00000A00 07 E7 10 08 19 11 09 F8 10 0A 49 11 0B 3A 11 0C  ..........I..:..
                        .        .        .        .        .
    0ADC:00000A10 8C 11 0D 5E 11 1A 7D 11 1B C1 10 1E 6B 11 00 B3  ...^..}.....k...
                    |END
    0ADC:00000A20 11 5B B2 0B 3D E4 0A 2A 84 0B 28 77 0B 44 90 0B  .[..=..*..(w.D..

    CL value    |   Subroutine address
    CL = 0x07       0x10E7                  ; CTRL+G / BEL
    CL = 0x08       0x1119                  ; CTRL+H / BACKSPACE / LEFT ARROW
    CL = 0x09       0x10F8                  ; CTRL+I / TAB
    CL = 0x0A       0x1149                  ; CTRL+J / DOWN ARROW
    CL = 0x0B       0x113A                  ; CTRL+K / UP ARROW
    CL = 0x0C       0x118C                  ; CTRL+L / RIGHT ARROW
    CL = 0x0D       0x115E                  ; CTRL+M / CARRIAGE RETURN
    CL = 0x1A       0x117D                  ; CTRL+Z / SUBSTITUE
    CL = 0x1B       0x10C1                  ; ESCAPE
    CL = 0x1E       0x116B                  ; RECORD SEPARATOR
    Any other (0)   0x11B3

    The ESCAPE subroutine sets BYTE PTR [60:128] = 1, BYTE PTR [60:129] = 1,
    and WORD PTR [60:134] = 0x2852

--

    0ADC:0A21 Subroutine lookup table
        Referred from 0ADC:0AC0, CL is byte from caller when (BYTE PTR DS:[0128] == 2)
        In most cases this is called with CL the first byte after ANSI ESCAPE code.

                     .        .        .        .        .
    0ADC:00000A20 11 5B B2 0B 3D E4 0A 2A 84 0B 28 77 0B 44 90 0B  .[..=..*..(w.D..
                  .        .        .        .       |END
    0ADC:00000A30 45 8D 0B 4D 99 0B 29 26 0B 00 93 0B 41 49 0C 42  E..M..)&....AI.B

    CL value    |   Subroutine address
    CL = 0x5B       0x0BB2                  ; ESC [
    CL = 0x3D       0x0AE4                  ; ESC =
    CL = 0x2A       0x0B84                  ; ESC *
    CL = 0x28       0x0B77                  ; ESC (
    CL = 0x44       0x0B90                  ; ESC D
    CL = 0x45       0x0B8D                  ; ESC E
    CL = 0x4D       0x0B99                  ; ESC M
    CL = 0x29       0x0B26                  ; ESC )
    Any other (0)   0x0B93                  ; ESC (anything else)

    The subroutine for (anything else) sets BYTE PTR [60:128] = 0, cancelling the
    ESC handling and ignoring the byte.

--

    0ADC:0A3C Subroutine lookup table
        Referred from ADC:0BDC, CL is the last byte of the ANSI ESC [ code.
        This is called only on the final byte, not the intermediate digits and
        semicolons of the code.

                  .        .        .        .        .        .
    0ADC:00000A3C 41 49 0C 42 6C 0C 43 93 0C 44 BA 0C 48 DD 0C 4A  AI.Bl.C..D..H..J
                        .        .        .        .        .
    0ADC:00000A4C 25 0D 4B 69 0D 4C 3E 0E 4D 69 0E 6D BD 0D 66 DD  %.Ki.L>.Mi.m..f.
                     .        .        .        .        .
    0ADC:00000A5C 0C 68 32 0F 6C 9A 0E 6E CB 0F 73 24 10 70 6F 10  .h2.l..n..s$.po.
                  .        .        .        .        .       |END
    0ADC:00000A6C 75 48 10 3E 88 0E 3F 8E 0E 3D 94 0E 00 C0 10 90  uH.>..?..=......

    CL value    |   Subroutine address
    CL = 0x41       0x0C49                  ; ESC [ A
    CL = 0x42       0x0C6C                  ; ESC [ B
    CL = 0x43       0x0C93                  ; ESC [ C
    CL = 0x44       0x0CBA                  ; ESC [ D
    CL = 0x48       0x0CDD                  ; ESC [ H
    CL = 0x4A       0x0D25                  ; ESC [ J
    CL = 0x4B       0x0D69                  ; ESC [ K
    CL = 0x4C       0x0E3E                  ; ESC [ L
    CL = 0x4D       0x0E69                  ; ESC [ M
    CL = 0x6D       0x0DBD                  ; ESC [ m
    CL = 0x66       0x0CDD                  ; ESC [ f
    CL = 0x68       0x0F32                  ; ESC [ h
    CL = 0x6C       0x0E9A                  ; ESC [ l
    CL = 0x6E       0x0FCB                  ; ESC [ n
    CL = 0x73       0x1024                  ; ESC [ s
    CL = 0x70       0x106F                  ; ESC [ p
    CL = 0x75       0x1048                  ; ESC [ u
    CL = 0x3E       0x0E88                  ; ESC [ >
    CL = 0x3F       0x0E8E                  ; ESC [ ?
    CL = 0x3D       0x0E94                  ; ESC [ =
    Any other (0)   0x10C0                  ; ESC [ (anything else)

    The anything else handler returns immediately (escape code is ignored) and
    the calling code resets the flag and allows the console to proceed normally.

--

                  .     .     .     .     .     .     .     .
    0ADC:00000A7C E2 12 F2 12 E2 12 CA 12 BC 12 F2 12 C3 12 D1 12  ................
                  .     .    |END
    0ADC:00000A8C C3 12 D8 12 E8 01 00 CB B8 00 01 C3 E8 01 00 CB  ................
    
    0ADC:0A7C table (routine 0ADC:1201 return from CALL 129Dh)
        AX = 0x0000 DL = 0x00    0x12E2
        AX = 0x0000 DL = 0x01    0x12F2
        AX = 0x0001 DL = 0x00    0x12E2
        AX = 0x0001 DL = 0x01    0x12CA
        AX = 0x0002 DL = 0x00    0x12BC
        AX = 0x0002 DL = 0x01    0x12F2
        AX = 0x0003 DL = 0x00    0x12C3
        AX = 0x0003 DL = 0x01    0x12D1
        AX = 0x0004 DL = 0x00    0x12C3
        AX = 0x0004 DL = 0x01    0x12D8

--

    0ADC:0A90:
        CALL A94h
        return far                          ; RETF
    0ADC:0A94:
        MOV AX,0100h
        return

--

    0ADC:0A98:
        CALL A9Ch
        return far                          ; RET

--

    0ADC:0A9C: (CL=10h AH=00h, at this time CL == caller's DL and DS = DOS segment 60h)
        IF BYTE PTR DS:[0128] != 0 JMP AB5  (60:128)
        IF CL < 0x20 JMP AACh
        CALL 11B3h
    0ADC:0AAC: (CL < 0x20)
        BX = 0A00h
        CALL 0ACFh
        CALL NEAR BX (BX comes from subroutine ACFh)
        return
    0ADC:0AB5: (BYTE PTR DS:[0128] != 0) aka (60:128)
        (BYTE DS:[0128])++
        IF BYTE PTR DS:[0128] != 2 JMP 0ACAh
    0ADC:0AC0: (BYTE PTR DS:[0128] == 2)
        BX = 0A21h
        CALL 0ACFh
        WORD PTR DS:[0124] = BX (BX comes from subroutine ACFh) (60:124)
    0ADC:0ACA: (jmp here from BYTE PTR DS:[0128] != 2 compare under 0ADC:0AB5)
        CALL NEAR WORD PTR DS:[0124]
        return

--

    ; Entry: BX = memory location of a lookup table, 3 bytes/entry in this format:
    ;           BYTE    subroutine number
    ;           WORD    subroutine address
    ;        CL = subroutine number to match
    ;
    ; Exit: BX = subroutine address
    0ADC:0ACF:
        IF BYTE PTR CS:[BX] == 0 JMP ADFh
        IF BYTE PTR CS:[BX] == CL JMP ADFh
        BX += 3
        JMP ACFh
    0ADC:0ADF:
        BX = WORD PTR CS:[BX+1]
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed per character after ESC =
    0ADC:0AE4:
        BX = WORD PTR DS:[0134]                     ; [0134] is often set to 2852h
        WORD PTR DS:[BX],CL
        WORD PTR DS:[0134] += 1
        IF BYTE PTR DS:[0128] < 0x04 JMP B18h
    0ADC:0AF5:
        BX = 2853h                                  ; ANSI escape parsing address + 1
    0ADC:0AF8:
        CX = WORD PTR DS:[BX]
    0ADC:0AFA:
        AL = CL
        AH = BYTE PTR DS:[0112]                     ; Scroll range lower limit
        CALL B19h
        BYTE PTR DS:[0110],AL                       ; Cursor Y position
        AL = CH
        AH = 0x4F
        CALL B19h
        BYTE PTR DS:[011C],AL                       ; Cursor X position
        CALL 1535h                                  ; update cursor position
        BYTE PTR DS:[0128] = 0
    0ADC:0B18:
        return

--

    0ADC:0B19:
        AL = ((AL >= 0x20) ? (AL - 0x20)) : 0       ; SUB AL, 20h ; JNC B1Fh ; MOV AL,00h
    0ADC:0B1F:
        AL = ((AL > AH) ? AH : AL)                  ; CMP AL, AH ; JBE B25h ; MOV AL, AH
    0ADC:0B25:
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed per character after ESC ) until the end of the ANSI code
    0ADC:0B26:
        IF BYTE PTR DS:[0128] != 3 JMP B76h
        IF CL == 0x33 JMP B43h
        IF CL != 0x30 JMP B4Dh
    0ADC:0B37:
        BYTE PTR DS:[008A] = 0x01
        BYTE PTR DS:[008B] = 0x20
        JMP B4Dh
    0ADC:0B43:
        BYTE PTR DS:[008B] = 0x67
        BYTE PTR DS:[008A] = 0x00
    0ADC:0B4D:
        IF BYTE PTR DS:[0111] == 0 JMP B71h
    0ADC:0B54:
        PUSH ES
        ES = WORD PTR DS:[0032]                     ; video ram segment
        DH = (BYTE PTR DS:[0112]) + 1
        DL = 0
        CALL 14F5h                                  ; convert row DH col DL to video memory address
        BX += 2
        AX = BYTE PTR DS:[008B]
        WORD PTR DS:[BX] = AX
        POP ES
    0ADC:0B71:
        BYTE PTR DS:[0128] = 0
    0ADC:0B76:
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed per character after ESC ( until the end of the ANSI code
    0ADC:0B77:
        IF BYTE PTR DS:[0128] < 3 JMP B83h
        BYTE PTR DS:[0128] = 0
    0ADC:0B83:
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed per character after ESC * until the end of the ANSI code
    0ADC:0B84:
        CALL 117Dh                                  ; CTRL+Z handling
        BYTE PTR DS:[0128] = 0
    0ADC:0B8C:
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed per character after ESC E until the end of the ANSI code
    0ADC:0B8D:
        CALL 115Eh                                  ; carriage return
    0ADC:0B90:
        CALL 1149h                                  ; line feed / down arrow
    0ADC:0B93:                                      ; <- table jumps here for unknown escapes
        BYTE PTR DS:[0128] = 0
    0ADC:0B98:
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed per character after ESC M
    0ADC:0B99:
        IF BYTE PTR DS:[0110] == 0 JMP BA9h         ; Move cursor up one row
        BYTE PTR DS:[0110] -= 1
        CALL 1535h                                  ; update cursor position
        JMP BACh
    0ADC:0BA9:
        CALL 13B0h                                  ; scroll region down one line
    0ADC:0BAC:
        BYTE PTR DS:[0128] = 0
    0ADC:0BB1:
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed per character after ESC [ until the end of the ANSI code
    0ADC:0BB2:
        IF BYTE PTR DS:[0128] == 0x02 JMP BF3h
        IF CL == 0x3B JMP C05h
        IF CL == 0x27 JMP C1Bh
        IF CL == 0x22 JMP C1Bh
        IF BYTE PTR DS:[012A] == 0xFF JMP C2Ch
        IF CL >= 0x3A JMP BDCh
        CL = CL AND 0x0F
        IF CL <= 0x09 JMP BF4h
    0ADC:0BDC:
        BX = 0A3Ch
        CALL ACFh
        SI = 2852h
        CALL NEAR BX
    0ADC:0BE7:
        IF BYTE PTR DS:[012A] != 0 JMP BF3h
        BYTE PTR DS:[0128] = 0
    0ADC:0BF3:
        return

--

    ; ESC [ A
    ; SI = ANSI escape area (2852h)
    0ADC:0C49:
        IF BYTE PTR DS:[0129] != 1 JMP C6Bh
    0ADC:0C50:
        CX = WORD PTR DS:[SI]               ; numeric value before "A" character, parameter
    0ADC:0C52:
        AX = BYTE PTR DS:[0110]             ; Cursor Y position
        IF CX == 0 THEN CX = 1              ; AND CX, CX ; JNE C5Eh ; MOV CX, 1
    0ADC:0C5E:
        IF CX > AX THEN CX = AX             ; CMP AX, CX ; JNC C64h ; MOV CX, AX
    0ADC:0C64:
        BYTE PTR DS:[0110] -= CL            ; Cursor Y position -= parameter
        CALL 1535h                          ; update cursor position on screen
    0ADC:0C6B:
        return

--

    ; ESC [ B
    ; SI = ANSI escape area (2852h)
    0ADC:0C6C:
        IF BYTE PTR DS:[0129] != 1 JMP C92h
        CX = WORD PTR DS:[SI]
    0ADC:0C75:
        AL = (BYTE PTR DS:[0112]) - (BYTE PTR DS:[0110]), AH = 0
        IF CX == 0 THEN CX = 1              ; AND CX, CX ; JNE C85h ; MOV CX, 1
        IF CX > AX THEN CX = AX             ; CMP AX, CX ; JNC C8Bh ; MOV CX, AX
        BYTE PTR DS:[0110] += CL            ; Cursor Y position += CL
        CALL 1535h                          ; update cursor position on screen
    0ADC:0C92:
        return

--

    ; ESC [ C
    ; SI = ANSI escape area (2852h)
    0ADC:0C93:
        IF BYTE PTR DS:[0129] != 1 JMP CB9h
        CX = WORD PTR DS:[SI]
    0ADC:0C9C:
        AX = 0x4F
        if (AL -= BYTE PTR DS:[011C]) < 0 JMP CB6h  ; SUB AL,[011C] ; JC CB6h
        IF CX == 0 THEN CX = 1                      ; AND CX, CX ; JNZ CACh
    0ADC:0CAC:
        IF CX > AX THEN CX = AX             ; CMP AX, CX ; JNC CB2h ; MOV CX, AX
        BYTE PTR DS:[011C] += CL            ; Cursor X position += CL
    0ADC:0CB6:
        CALL 1535h                          ; update cursor position on screen
    0ADC:0CB9:
        return

--

    ; ESC [ D
    ; SI = ANSI escape area (2852h)
    0ADC:0CBA:
        IF BYTE PTR DS:[0129] != 1 JMP CDCh
        CX = WORD PTR DS:[SI]
    0ADC:0CC3:
        AX = BYTE PTR DS:[011C]                     ; Cursor X position
        IF CX == 0 THEN CX = 1                      ; AND CX, CX ; JNZ CCFh
        IF CX > AX THEN CX = AX                     ; CMP AX, CX ; JNC CD5h ; MOV CX, AX
        BYTE PTR DS:[011C] -= CL                    ; Cursor X position -= CL
    0ADC:0CD9:
        CALL 1535h                                  ; update cursor position on screen
    0ADC:0CDC:
        return

--

    ; ESC [ L
    ; SI = ANSI escape area (2852h)
    0ADC:0E3E:
        IF BYTE PTR DS:[012A] != 0 JMP E63h
        IF BYTE PTR DS:[0129] != 1 JMP E63h
    0ADC:0E4C:
        CX = WORD PTR DS:[SI]
    0ADC:0E4E:
        IF CX == 0 THEN CX = 1                      ; AND CX, CX ; JNZ E55h
    0ADC:0E55:
        PUSH CX
        AH = BYTE PTR DS:[0110]                     ; AH = Cursor Y position
        CALL 13B4h
        POP CX
        IF CX > 0 THEN CX--, JMP E55h               ; LOOP E55h
    0ADC:0E60:
        CALL 115Eh                                  ; carriage return
    0ADC:0E63:
        BYTE PTR DS:[012A] = 0
    0ADC:0E68:
        return

--

    ; ESC [ M
    ; SI = ANSI escape area (2852h)
    0ADC:0E69:
        IF BYTE PTR DS:[0129] != 1 JMP E87h
        CX = WORD PTR DS:[SI]
    0ADC:0E72:
        IF CX == 0 THEN CX = 1                      ; AND CX, CX ; JNZ E55h
    0ADC:0E79:
        PUSH CX
        AH = BYTE PTR DS:[0110]                     ; AH = Cursor Y position
        CALL 135Fh
        POP CX
        IF CX > 0 THEN CX--, JMP E79h               ; LOOP E79h
    0ADC:0E84:
        CALL 115Eh                                  ; carriage return
    0ADC:0E87:
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed on ESC [ >
    0ADC:0E88:
        BYTE PTR DS:[012A] = 1
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed on ESC [ ?
    0ADC:0E8E:
        BYTE PTR DS:[012A] = 2
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed on ESC [ =
    0ADC:0E94:
        BYTE PTR DS:[012A] = 3
        return

--

    ; Entry: CL = character code
    ;
    ; This is executed when the ESC is encountered outside of ANSI processing

    0ADC:10C1:
        PUSH ES
        WORD PTR DS:[0134] = 0x2852
        BYTE PTR DS:[0128] = 0x01
        BYTE PTR DS:[0129] = 0x01
        DI = 0x2852
        AX = 0
        CX = 0x14
        DX = ES = DS
        _fmemset(ES:DI,0,0x28)  ; REP STOSW
        POP ES
    0ADC:10E2:
        return

--

    ; ??
    0ADC:10E3:
        CALL 10E7h
    0ADC:10E6:
        return far                  ; RETF

--

    ; ASCII BEL 07h handling
    0ADC:10E7:
        AH = 17h                    ; INT 18h AH=17h turn on bell
        INT 18h
        CX = 0x6E8C                 ; <- delay loop
    0ADC:10EE:
        PUSH AX
        NOP AX
        POP AX
        IF CX > 0 THEN CX--, JMP 10EEh  ; LOOP 10EEh
    0ADC:10F3:
        AH = 18h                    ; INT 18h AH=18h turn off bell
        INT 18h
    0ADC:10F7:
        return

--

    ; ASCII TAB 09h handling
    0ADC:10F8:
        AL = ((BYTE PTR DS:[011C] + 8) >> 3) * 8        ; move cursor X position to next multiple of 8
        BYTE PTR DS:[011C] = AL
        IF AL < 0x4F JMP 1115h                          ; interesting bug here, AL would be 0x50 on overflow
    0ADC:110C:
        BYTE PTR DS:[011C] = 0x4F                       ; keep cursor on-screen, set to last column
        CALL 118Ch                                      ; call function to advance one column to wrap around to next line
        return
    0ADC:1115:
        call 1535h                                      ; update cursor position
    0ADC:1118:
        return

--

    ; ASCII BACKSPACE 08h / LEFT ARROW KEY
    0ADC:1119:
        IF BYTE PTR DS:[011C] == 0 JMP 1126h
    0ADC:1120:
        BYTE PTR DS:[011C] -= 1
        JMP 1136h
    0ADC:1126:
        IF BYTE PTR DS:[0110] == 0 JMP 1139h
    0ADC:112D:
        BYTE PTR DS:[0110] -= 1
        BYTE PTR DS:[011C] = 0x4F
    0ADC:1136:
        CALL 1535h                          ; update cursor position on screen
    0ADC:1139:
        return

--

    ; ASCII UP ARROW 0Bh
    0ADC:113A:
        IF BYTE PTR DS:[0110] == 0 JMP 1148h
        BYTE PTR DS:[0110] -= 1
        CALL 1535h                          ; update cursor position on screen
    0ADC:1148:
        return

--

    ; ASCII DOWN ARROW 0Ah
    0ADC:1149:
        AL = BYTE PTR DS:[0110]
        IF AL == BYTE PTR DS:[0112] JMP 115Ah
        BYTE PTR DS:[0110] += 1
        CALL 1535h                          ; update cursor position on screen
        return
    0ADC:115A:
        CALL 1348h                          ; scroll up screen region
        return

--

    ; ASCII CARRIAGE RETURN 0Dh
    0ADC:115E:
        BYTE PTR DS:[011C] = 0              ; set X coordinate = 0
        CALL 1535h                          ; update cursor position
    0ADC:1166:
        return

--

    ; ??
    0ADC:1167:
        CALL 116Bh
    0ADC:116A:
        return far                          ; RETF

--

    ; ASCII RECORD SEPARATOR 1Eh
    0ADC:116B:
        BYTE PTR DS:[0110] = 0              ; set Y pos = 0
        BYTE PTR DS:[011C] = 0              ; set X pos = 0
        CALL 1535h                          ; update cursor position
    0ADC:1178:
        return

--

    0ADC:1179:
        CALL 117Dh
    0ADC:117C:
        return far                          ; RETF

--

    0ADC:117D: (CTRL+Z handling)
        DI = 0
        CX = 0x9B0                          ; 0x9B0 = 80*31?
        CALL 1516h                          ; clear the screen (fill with erasure)
        CALL 13FFh                          ; redraw function row
        CALL 116Bh                          ; call 0x1E RECORD SEPARATOR function
    0ADC:118B:
        return

--

    0ADC:118C: (advance cursor one column. if past right edge of screen fall through to 1197h to advance one row)
        BYTE PTR DS:[011C] += 1 (cursor X coordinate += 1)
        IF BYTE PTR DS:[011C] <= 4Fh JMP 11AFh (if cursor X coordinate <= 4Fh then goto 11AFh)
    0ADC:1197: (advance cursor one row. if past lower scroll limit then fall through to 11A9h scroll up screen region) 
        BYTE PTR DS:[011C] = 0 (set cursor X coordinate = 0)
        AL = BYTE PTR DS:[0110] (AL = (cursor Y coordinate)++)
        BYTE PTR DS:[0110] += 1
        IF BYTE PTR DS:[0112] != AL JMP 11AFh (if cursor Y != scroll range lower limit then goto 11AFh)
    0ADC:11A9: (put cursor back on the bottom row and scroll screen region up)
        BYTE PTR DS:[0110] = AL (set cursor Y coordinate to scroll range lower limit)
        CALL 1348h
    0ADC:11AF:
        CALL 1535h (update cursor position on screen)
    0ADC:11B2:
        return

--

    0ADC:11B3: (CL=10h AH=00h, at this time CL == caller's DL and DS = DOS segment 60h)
        IF BYTE PTR DS:[011C] < 0x50 JMP 11C7h ; (60:11C cursor X position)
        IF BYTE PTR DS:[0117] == 0 JMP 11C2h ; (60:117 line wrap flag)
    0ADC:11C1:
        return
    0ADC:11C2:
        PUSH CX
        CALL 118Ch
        POP CX
    0ADC:11C7: (60:11C < 0x50)
        IF BYTE PTR DS:[008A] == 0 JMP 1201h ; (60:8A kanji / graph mode flag)
        IF BYTE PTR DS:[0115] != 0 JMP 11F3h ; (60:115 kanji upper byte storage flag)
        IF CL < 0x81 JMP 1201h
        IF CL < 0xA0 JMP 11E9h
        IF CL < 0xE0 JMP 1201h
        IF CL >= 0xFD JMP 1201h
        BYTE PTR DS:[0116] = CL (60:116 kanji upper byte)
        BYTE PTR DS:[0115] = 1 (60:115 kanji upper byte storage flag)
    0ADC:11E9:
        return
    0ADC:11F3: (60:115 kanji upper byte storage flag set)
        CH = DS:[0116] (60:116 kanji upper byte)
        CALL 1236h
        tCH = CH, tCL = CL, CL = tCH - 0x20, CH = tCL           ; XCHG CL, CH ; SUB CL, 0x20
        JMP 1203h
    0ADC:1201:
        CH = 0
    0ADC:1203:
        CALL 1260h
        CALL 129Dh
        DI = BX
        AX = ((AX * 2) + DL) * 2
        BX = (AX + 0A7Ch)
        BX = WORD PTR CS:[BX]
        PUSH ES
        ES = WORD PTR DS:[0032] ; (60:32 appears to be Text VRAM segment A000h)
        AX = CX
        CALL NEAR BX
        POP ES
        BYTE PTR DS:[0115] = 0 (60:115 upper byte storage flag)
        BYTE PTR DS:[011C] += AL (60:11C cursor X coordinate)
        CALL 1535h
    0ADC:1231:
        return

--

    0ADC:1232:
        CALL 1236h
    0ADC:1235:
        return far                                              ; RETF

--

    0ADC:1236: (CH = upper Kanji byte)
        IF CH == 0x80 JMP 125Fh
        IF CH >= 0xA0 JMP 1245h
        CH -= 0x70
        JMP 1248h
    0ADC:1245:
        CH -= 0xB0
    0ADC:1248:
        IF (CL & 0x80) != 0x00 THEN CL--                        ; OR CL, CL ; JNS 124E ; DEC CL
    0ADC:124E:
        CH = CH * 2
        IF CL < 0x9E JMP 125Ah
        CL -= 0x5E
        JMP 125Ch
    0ADC:125A:
        CH--
    0ADC:125C:
        CL -= 0x1F
    0ADC:125F:
        return

--

    0ADC:1260:
        PUSH ES, CX
        ES = WORD PTR DS:[0032]                                 ; video ram segment (A000h)
        DH = BYTE PTR DS:[0110]                                 ; Cursor Y position
        DL = BYTE PTR DS:[011C]                                 ; Cursor X position
        CALL 14F5h                                              ; convert to video memory address (return in BX)
        CX = WORD PTR ES:[BX]                                   ; read from video memory
        CALL 129Dh                                              ; Determine cell width (single or double, indicated in DL)
        AX = 2
        IF DL == 1 JMP 129Ah
        DH = DL
        CX = WORD PTR ES:[BX+2]
        CALL 129Dh
        AX = 0
        IF DH != 2 JMP 1294h
        AX = 3
    0ADC:1294:
        IF DL == 0 JMP 129Ah
        AX++
    0ADC:129A:
        POP CX, ES
    0ADC:129C:
        return

--

    0ADC:129D:
        DL = 0
        IF CH == 0x00 JMP 12BBh
        IF CH == 0xFF JMP 12BBh
        IF CL <  0x09 JMP 12B3h
        IF CL <  0x0C JMP 12BBh
    0ADC:12B3:
        DL = 1
        IF (CL & 0x80) != 0 JMP 12BBh
        DL = 2
    0ADC:12BB:
        return

--

    0ADC:12BC: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        CALL 1330h
        CALL 12E2h          ; write single-wide
    0ADC:12C2:
        return

--

    0ADC:12C3: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        CALL 1324h
        CALL 12E2h          ; write single-wide
    0ADC:12C9:
        return

--

    0ADC:12CA: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        CALL 133Ch
        CALL 12F2h          ; write double-wide
    0ADC:12D0:
        return

--

    0ADC:12D1: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        CALL 1324h
        CALL 12F2h          ; write double-wide
    0ADC:12D7:
        return

--

    0ADC:12D8: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        CALL 1324h
        CALL 133Ch
        CALL 12F2h          ; write double-wide
    0ADC:12E1:
        return

--

    ; This appears to write a single-wide character
    0ADC:12E2: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        WORD PTR ES:[DI] = AX ; write character code
        DI += 0x2000
        WORD PTR ES:[DI] = WORD PTR DS:[013C] (60:13C display attribute in extended attribute mode) ; write attribute code
        AL = 1 (this indicates to caller to move cursor X position 1 unit to the right)
    0ADC:12F1:
        return

--

    ; This appears to write a double-wide (two cell wide) character
    0ADC:12F2: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        IF BYTE PTR DS:[011C] < 0x4F JMP 1311h              ; if Cursor X coordinate < 0x4F
    ; NTS: Cursor wraparound case (no room on last column). Write an empty cell then wrap to the next line and print char.
        PUSH AX
        CX = 0x20, WORD PTR ES:[DI] = CX
        CALL 118Ch                                          ; advance cursor one column
        DH = BYTE PTR DS:[0110]                             ; DH = cursor Y
        DL = BYTE PTR DS:[011C]                             ; DL = cursor X
        CALL 14F5h                                          ; Convert DH, DL to VRAM address in BX
        DI = BX
        POP AX
    ; NTS: Notice the double-wide (usually Kanji) writing routine writes the code from AX, then
    ;      writes AX | 0x80. This has no effect on the display on real hardware, but follows
    ;      NEC PC-98 recommendations.
    0ADC:1311:
        WORD PTR ES:[DI] = AX, DI += 2                      ; STOSW
        WORD PTR ES:[DI] = AX | 0x80, DI += 2               ; OR AL,80h ; STOSW
        DI -= 4, DI += 0x2000
        AX = WORD PTR DS:[013C]                             ; AX = extended color attribute
        WORD PTR ES:[DI] = AX, DI += 2                      ; STOSW
        WORD PTR ES:[DI] = AX, DI += 2                      ; STOSW
        AL = 2
    0ADC:1323:
        return

--

    0ADC:1324: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        PUSH DI
        DI -= 2
        CX = 0x20
        WORD PTR ES:[DI] = CX
        POP DI
    0ADC:132F:
        return

--

    0ADC:1330: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        PUSH DI
        DI += 2
        CX = 0x20
        WORD PTR ES:[DI] = CX
        POP DI
    0ADC:133B:
        return

--

    0ADC:133C: (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        PUSH DI
        DI += 4
        CX = 0x20
        WORD PTR ES:[DI] = CX
        POP DI
    0ADC:1347:
        return

--

    0ADC:1348: (scroll up screen scroll region)
        AH = BYTE PTR DS:[011E]         ; scroll region upper limit
        WORD PTR DS:[011F] = 0x0001     ; scroll "weight" aka delay
        IF BYTE PTR DS:[0118] != 0 THEN WORD PTR DS:[011F] = 0xE000 ; if slow scroll mode set "weight" aka delay to larger value
    0ADC:135F:
        CL = BYTE PTR DS:[0112]         ; scroll region lower limit
        CX = CL - AH                    ; lower limit - upper limit and zero extend to CX
        if CX == 0 JMP 13A1h
    0ADC:1369:
        PUSH ES, DS
        DX = AH << 8                    ; DH = AH, DL = 0
        CALL 14F5h                      ; take cursor position, convert to video RAM byte address, return in BX
        ES = DS = WORD PTR DS:[0032]    ; retrieve video (text) RAM segment
        DX = BX + 0xA0                  ; note 0xA0 = 160 = 80 * 2
    0ADC:1381:
        PUSH CX
                                        ; This part uses REP MOVSW first on character data at A000:0000+x
                                        ; then on attribute data at A000:2000+x
                                        ; Move and swap is used to let REP MOVSW advance SI and DI, then
                                        ; re-use the same starting SI and DI for the attribute data, before
                                        ; then using the post REP MOVSW SI and DI values for the next line.
        SI = DX
        DI = BX
        _fmemcpy(ES:DI,DS:SI,0xA0)      ; CX = 0x50 REP MOVSW
        SWAP(DX,SI), SI += 0x2000
        SWAP(BX,DI), DI += 0x2000
        _fmemcpy(ES:DI,DS:SI,0xA0)      ; CX = 0x50 REP MOVSW
        POP CX
        IF CX > 0 THEN CX--, JMP 1381h  ; LOOP 1381h
    0ADC:139F:
        POP DS, ES
    0ADC:13A1:
        DH = BYTE PTR DS:[0112]
        call 1508h
        CX = WORD PTR DS:[011F]
    0ADC:13AC:                          ; <- delay loop, to "weight" down the console scroll
        NOP
        IF CX > 0 THEN CX--, JMP 13ACh  ; LOOP 13ACh
    0ADC:13AF:
        return

--

    0ADC:13B0: (take scroll region, scroll down one line)
        AH = BYTE PTR DS:[011E]         ; scroll range upper limit
        CL = BYTE PTR DS:[0112], CH = 0 ; scroll range lower limit
        CL -= AH
        IF CL == 0 THEN JMP 13F8h
    0ADC:13BE:
        STD                             ; set direction flag, so that REP MOVSW works backwards
        PUSH ES, DS
        DH = BYTE PTR DS:[0112]
        DL = 0x4F
        CALL 14F5h                      ; convert scroll lower limit row, column 0x4F (79) to video ram address
        ES = DS = WORD PTR DS:[0032]    ; video RAM segment
        DX = BX - 0xA0                  ; DX = video RAM address one row up, BX = video RAM address
    0ADC:13D8:
        PUSH CX
        SI = DX, DI = BX
        _fmemcpy_backwards(ES:DI, DS:SI, 0xA0)      ; CX = 0x50, REP MOVSW, with DF=1
        XCHG DX, SI ; XCHG BX, DI                   ; SI,DI modified by REP MOVSW, so swap with copy to re-use original addr for next step
        SI += 0x2000 ; DI += 0x2000                 ; point at attribute RAM
        _fmemcpy_backwards(ES:DI, DS:SI, 0xA0)      ; CX = 0x50, REP MOVSW, with DF=1
        POP CX
        IF CX > 0 THEN CX--, JMP 13D8h              ; LOOP 13D8h
    0ADC:13F6:
        POP DS, ES ; CLD
        DH = AH                                     ; DH = scroll range upper limit
        CALL 1508h
    0ADC:13FE:
        return

--

    0ADC:13FF:
        IF BYTE PTR DS:[0111] != 0 JMP 1407h
    0ADC:1406:
        return
    0ADC:1407:
        (TODO)

--

    0ADC:14F5: (DH = Y coordinate  DL = X coordinate    return in BX = video RAM byte address)
        BX = (WORD PTR DS:[(DH * 2) + 0x1814])
        DX = (DL * 2)
        BX += DX
        return ; 0ADC:1507

--

    0ADC:1508: (DH = Scroll range lower limit)
        DL = 0
        CALL 14F5h  (get video RAM address of row DH from caller and column zero)
        DI = BX
        CX = 0x50
        CALL 1516h
        return
    0ADC:1516: (CX = number of WORDs to write, DI = video ram address to fill)
        PUSH ES
        ES = WORD PTR DS:[0032]             ; get video RAM (text) segment A000h
        DX = CX                             ; save CX (for later)
        BX = DI                             ; save DI (for later)
        AX = WORD PTR DS:[0119]             ; erasure character
        _fmemset(ES:DI,cx*2)                ; REP STOSW (CX from caller)
        AX = WORD PTR DS:[013E]             ; erasure attribute
        CX = DX                             ; restore CX from caller
        DI = BX + 0x2000                    ; restore DI from caller and add 2000h
        _fmemset(ES:DI,cx*2)                ; REP STOSW (CX from caller)
        POP ES
        return

--

    0ADC:1534:
        return
    0ADC:1535: (update cursor position on screen)
        IF BYTE PTR DS:[011B] == 0 JMP 1534h (return immediately if cursor is turned off)
    0ADC:153C:
        DH = BYTE PTR DS:[0110]
        DL = BYTE PTR DS:[011C]
        IF DL >= 0x50 THEN DL--
    0ADC:154B:
        CALL 14F5h (take cursor position, convert to video RAM byte address, return in BX)
        AH = 0x13 (INT 18h AX=13h set cursor position)
        DX = BX (byte position of cursor)
        JMP 198Ah (calls INT 18h AX=13h to set cursor pos. function 198Ah then returns)

--

    0ADC:1814 Row value to video RAM address lookup table used by subroutine at 0ADC:14F5

        VRAM_ADDRESS = WORD PTR [0ADC:1814 + (row * 2)]

                  .     .     .     .     .     .     .     .      <- Cursor rows 0-7
    0060:00001814 00 00 A0 00 40 01 E0 01 80 02 20 03 C0 03 60 04  ....@..... ...`.
                  .     .     .     .     .     .     .     .      <- Cursor rows 8-15
    0060:00001824 00 05 A0 05 40 06 E0 06 80 07 20 08 C0 08 60 09  ....@..... ...`.
                  .     .     .     .     .     .     .     .      <- Cursor rows 16-23
    0060:00001834 00 0A A0 0A 40 0B E0 0B 80 0C 20 0D C0 0D 60 0E  ....@..... ...`.
                  .     .     .     .     .     .     .    |END    <- Cursor rows 24-30
    0060:00001844 00 0F A0 0F 40 10 E0 10 80 11 20 12 C0 12 00 00  ....@..... .....

--

    0ADC:198A (this code calls INT 18h in an elaborate way in order to make sure interrupts are disabled at entry point for some reason)
        PUSH DS
        DS = 0
        PUSHF, CLI, CALL FAR WORD PTR DS:[0060] (DS=0, CALL FAR 0000:0060, which is INT 18h but with interrupts disabled at entry)
        POP DS
        return

--

    0ADC:3126:
        PUSH DS
        DS = WORD PTR CS:[0030] = DOS segment 60h
        WORD PTR DS:[05E1] = caller DS
        Store caller AX, SS, SP, DX, BX into DS: [5DB], [5DD], [5DF], [5E3], [5E5]
        SS:SP = WORD PTR CS:[0030] (DOS segment 60h : offset 767h)
        CLD, STI
        PUSH ES, BX, CX, DX, SI, DI
        BYTE PTR DS:[00B4] = 1
    0ADC:3154:
        CL -= 9, JMP to 0ADC:3180 if carry (if caller CL < 9)
    0ADC:315E:
        IF CL < 0x0D, JMP TO 0ADC:3173 (if caller CL < (0x16 = 0x0D+9))
    0ADC:3163:
        IF CL < 0x77, JMP TO 0ADC:3180 (if caller CL < (0x80 = 0x77+9))
    0ADC:3168:
        IF CL > 0x79, JMP TO 0ADC:3180 (if caller CL > (0x82 = 0x79+9))
    0ADC:316D: (JMPed here if CL >= 0x80 && CL <= 0x82)
        CL = (CL - 0x77) + 0x0D
    0ADC:3173: (JMPed here if CL >= 0x09 && CL <= 0x15)
        SI = (CL * 2) + 0x3A5C
    0ADC:317D:
        CALL NEAR WORD PTR 0ADC:[SI]
    0ADC:3180:
        CALL NEAR 0ADC:4080
    0ADC:3183:
        POP DI, SI, DX, CX, BX, ES
        DS = WORD PTR CS:[0030] = DOS segment 60h
        Restore caller AX, SS, SP, DX, BX from [5DB], [5DD], [5DF], [5E3], [5E5]
        POP DS (restore caller DS)
        IRET
--

    0ADC:31A4: (CL=0Bh entry point)
        return
    0ADC:31A5: (CL=0Ch entry point)
        ES = WORD PTR DS:[05E1]                 ; DS from caller
        DI = DX                                 ; DX from caller
        AX = WORD PTR DS:[05DB]                 ; AX from caller
        IF AX > 0xFF JMP 31B8h
        IF AX != 0xFF JMP 31C8h
        JMP 3202h
    0ADC:31B8: (CL=0Ch, AX > 0xFF)
        IF AX < 0x101 JMP 31C0h
        IF AX == 0x101 JMP 31C4h
    0ADC:31BF:
        return
    0ADC:31C0: (CL=0Ch, AX < 0x101 && AX > 0xFF)
        CALL 32C9h
        return
    0ADC:31C4: (CL=0Ch, AX == 0x101)
        CALL 32D2h
        return
    0ADC:31C8: (CL=0Ch, AX < 0xFF)
        IF AX > 0 THEN (AX--; JMP 3234h)        ; SUB AX, 1 ; JNC 3234h
    0ADC:31CD: (CL=0Ch, AX == 0xFFFF)           ; AX = 0, AX--, AX == 0xFFFF
        SI = 2D2Fh
        DL = 0Ah
        AL = 00h
    0ADC:31D4:
        _fmemcpy(ES:DI, DS:SI, 0x0F);           ; CX = 0Fh ; REP MOVSB
        BYTE PTR [ES:DI] = AL, DI++             ; AL == 00h. STOSB
        SI++
        DL--
        IF DL != 0 JMP 31D4h
    0ADC:31DF:
        SI += 0x50                              ; SI becomes 2D2Fh + (16*10) + 0x50 = 2E1Fh
        DL = 0Ah
    0ADC:31E4:
        _fmemcpy(ES:DI, DS:SI, 0x0F);           ; CX = 0Fh ; REP MOVSB
        BYTE PTR [ES:DI] = AL, DI++             ; AL == 00h. STOSB
        SI++
        DL--
        IF DL != 0 JMP 31E4h
    0ADC:31EF:
        SI += 0x50                              ; SI becomes 2E1Fh + (16*10) + 0x50 = 2F0Fh
        DL = 0Bh
    0ADC:31F4:
        _fmemcpy(ES:DI, DS:SI, 0x05);           ; CX = 05h ; REP MOVSB
        BYTE PTR [ES:DI] = AL, DI++             ; AL == 00h. STOSB
        SI += 3
        DL--
        IF DL != 0 JMP 31F4h
    0ADC:3201:
        return

    0ADC:3202: (CL=0Ch, AX < FFh)
        SI = 2D2Fh
        DL = 1Eh
        AL = 00h
    0ADC:3209:
        _fmemcpy(ES:DI, DS:SI, 0x0F);           ; CX = 0Fh ; REP MOVSB
        BYTE PTR [ES:DI] = AL, DI++             ; AL == 00h. STOSB
        SI++
        DL--
        IF DL != 0 JMP 3209h
    0ADC:3214:
        DL = 0Bh                                ; SI is 2F0Fh
    0ADC:3216:
        _fmemcpy(ES:DI, DS:SI, 0x05);           ; CX = 05h ; REP MOVSB
        BYTE PTR [ES:DI] = AL, DI++             ; AL == 00h. STOSB
        SI += 3
        DL--
        IF DL != 0 JMP 3216h
    0ADC:3223:
        SI = 2F87h
        DL = 0Fh
    0ADC:3228:
        _fmemcpy(ES:DI, DS:SI, 0x0F);           ; CX = 0Fh ; REP MOVSB
        BYTE PTR [ES:DI] = AL, DI++             ; AL == 00h. STOSB
        SI++
        DL--
        IF DL != 0 JMP 3228h
    0ADC:3233:
        return

    0ADC:3234: (CL=0Ch, AX != 0, AX was decremented by 1 before coming here)
        IF AX > 28h JMP 3242h
    0ADC:3239:
        PUSH DS
        DS = CS                                 ; DS = CS = INT DCh segment 0ADCh
        AL = (BYTE PTR DS:[3DE0h+AL]) - 1       ; BX = 3DE0h ; XLAT ; DEC AX
        POP DS
    0ADC:3242:
        IF AX >= 38h JMP 3288h                  ; CMP AX,38h ; JNC 3288h
        IF AX >= 1Eh JMP 325Bh                  ; CMP AX,1Eh ; JNC 325Bh
    0ADC:324C:
        SI = (AX * 16) + 0x2D2F                 ; CL = 4 ; SHL AX,CL ; MOV SI,AX ; ADD SI,2D2Fh
        CX = 0Fh
        JMP 3282h
    0ADC:325B:
        IF AX >= 29h JMP 3272h                  ; CMP AX,29h ; JNC 3272h
        AX -= 1Eh                               ; SUB AX, 1Eh
        SI = (AX * 8) + 0x2F0F                  ; CL = 3 ; SHL AX,CL ; MOV SI,AX ; ADD SI,2F0Fh
        CX = 05h
        JMP 3282h
    0ADC:3272:
        AX -= 0x29
        SI = (AX * 16) + 0x2F87                 ; CL = 4 ; SHL AX,CL ; MOV SI,AX ; ADD SI,2F87h
    0ADC:3282:
        _fmemcpy(ES:DI, DS:SI, CX)              ; REP MOVSB
        AL = 0, BYTE PTR [ES:DI] = AL, DI++     ; AL = 0 ; STOSB
    0ADC:3287:
        return
    0ADC:3288:
        IF AX < 29h JMP 3287h                   ; CMP AX,29h ; JC 3287h
        IF AX > 39h JMP 3287h                   ; CMP AX,39h ; JA 3287h
        IF AX == 39h JMP 32B3h                  ; JE 32B3h
        IF AX <= 32h JMP 32A1h                  ; CMP AX,32h ; JBE 32A1h
        IF AX != 38h JMP 3287h                  ; CMP AX,38h ; JNE 3287h
    0ADC:329E:
        AX = 28h                                ; MOV AX,28h
    0ADC:32A1:
        AX -= 29h
        SI = (AX * 16) + 0x2F87                 ; CL = 4 ; SHL AX,CL ; MOV SI,AX ; ADD SI,2F87h
        CX = 0Fh
        JMP 3282h
    0ADC:32B3:
        AX = 38h
        CALL 3294h
        CX = 0Ah
    0ADC:32BC:
        PUSH CX
        AX = 33h - CX
        CALL 3294h
        POP CX
        IF CX > 0 THEN CX--, JMP 32BCh          ; LOOP 32BCh
    0ADC:32C8:
        return

    0ADC:32C9:
        SI = 3078h
        _fmemcpy(ES:DI, DS:SI, 0x202);          ; CX = 202h ; REP MOVSB
        return

    0ADC:32D2:
        BX = WORD PTR DS:[3076]
        AX = 200h - BX
        WORD PTR DS:[05DB] = AX                 ; write AX return to caller
    0ADC:32DE:
        return

--

    0ADC:32DF: (CL=0Dh entry point)
        ES = WORD PTR CS:[0030]                 ; MS-DOS segment (60h)
        AX = WORD PTR DS:[05DB]                 ; AX from caller
        DS = WORD PTR DS:[05E1]                 ; DS from caller
        SI = DX                                 ; DX from caller
        IF AX > 0xFF JMP 32F7h                  ; CMP AX,00FF ; JA 32F7h
        IF AX != 0xFF JMP 3310h                 ; ... JNE 3310h
        JMP 3350h
    0ADC:32F7: (AX > 0xFF)
        IF AX < 0x101 JMP 3304h                 ; CMP AX,0101 ; JC 3304h
        IF AX == 0x101 JMP 3308h                ; ... JE 3308h
        IF AX == 0x102 JMP 330Ch                ; CMP AX,0102 ; JE 330Ch
    0ADC:3303:
        return
    0ADC:3304: (AX < 0x101)
        CALL 3445h
        return
    0ADC:3308: (AX == 0x101)
        CALL 349Ch
        return
    0ADC:330C: (AX == 0x102)
        CALL 3538h
        return
    0ADC:3310: (AX < 0xFF)
        IF AX != 0 THEN AX--, JMP 3390h         ; SUB AX, 1 ; JNC 3390h
    0ADC:3315: (AX == 0, DS = caller DS segment, SI = caller DX register, ES = MS-DOS segment 60h)
        DI = 2D2Eh                              ; This appears to be the memory location that holds the function key row strings and attributes
        DL = 0Ah                                ; There are 10 entries, 16 bytes apart
    0ADC:331A:
        CALL 3547h                              ; 
        _fmemcpy(ES:DI, DS:SI, 0x0F)            ; CX = 0x0F ; REP MOVSB
                                                ; Side effect: SI += 0x0F, DI += 0x0F
        SI++
        IF DL > 0 THEN DL--, JMP 331Ah          ; DEC DL ; JNZ 331Ah
    0ADC:3327:
        DI += 0x50                              ; Reminder: 2D2Eh + (0x10 * 0x0A) + 0x50 = 2E1Eh
                                                ; This (2E1Eh) appears to hold the text that is injected when the function key is pressed.
                                                ; For example by default this table defines F1 -> "DIR A:\x0D"
        DL = 0Ah
    0ADC:332C:
        CALL 3547h
        _fmemcpy(ES:DI, DS:SI, 0x0F)            ; CX = 0x0F ; REP MOVSB
                                                ; Side effect: SI += 0x0F, DI += 0x0F
        SI++
        IF DL > 0 THEN DL--, JMP 332Ch          ; DEC DL ; JNZ 332Ch
    0ADC:3339:
        DI += 0x50                              ; Reminder: 2E1Eh + (0x10 * 0x0A) + 0x50 = 2F0Eh
        DL = 0x0B
    0ADC:333E:
        CALL 3547h
        _fmemcpy(ES:DI, DS:SI, 0x05)            ; CX = 0x05 ; REP MOVSB
                                                ; Side effect: SI += 0x05, DI += 0x05
        SI++
        DI += 2
        IF DL > 0 THEN DL--, JMP 333Eh          ; DEC DL ; JNZ 333Eh
    0ADC:334E:
        JMP 3386h
    0ADC:3350: (AX == 0xFF)
        DI = 2D2Eh                              ; Memory location that appears to hold function key row strings
        DL = 0x1E
    0ADC:3355:
        CALL 3547h
        _fmemcpy(ES:DI, DS:SI, 0x0F)            ; CX = 0x0F ; REP MOVSB
                                                ; Side effect: SI += 0x0F, DI += 0x0F
        SI++
        IF DL > 0 THEN DL--, JMP 3355h
    0ADC:3362:
        DL = 0x0B
    0ADC:3364:
        CALL 3547h
        _fmemcpy(ES:DI, DS:SI, 0x05)            ; CX = 0x05 ; REP MOVSB
                                                ; Side effect: SI += 0x05, DI += 0x05
        DI += 2
        IF DL > 0 THEN DL--, JMP 3364h
    0ADC:3374:
        DI = 0x2F86
        DL = 0x0F
    0ADC:3379:
        CALL 3547h
        _fmemcpy(ES:DI, DS:SI, 0x0F)            ; CX = 0x0F ; REP MOVSB
        SI++
        IF DL > 0 THEN DL--, JMP 3379h
    0ADC:3386:
        IF BYTE PTR ES:[0111] == 0 THEN JMP 33ECh   ; ES = DOS segment 60h
        JMP 33ECh                                   ; Wait... what?
    0ADC:3390:
        (TODO)

    0ADC:33EC:
        IF AX < 0x29 JMP 33E3h                      ; CMP AX, 29h ; JC 33E3h
        IF AX > 0x39 JMP 33E3h                      ; CMP AX, 39h ; JA 33E3h
        IF AX == 0x39 JMP 3417h                     ; ... JE 3417h
    0ADC:33F8:
        IF AX <= 0x32 JMP 3405h                     ; CMP AX, 32h ; JBE 3405h
        IF AX != 0x38 JMP 33E3h                     ; CMP AX, 38h ; JNE 33E3h
    0ADC:3402:
        AX = 0x28
    0ADC:3405:
        DI = ((AX - 0x29) * 16) + 0x2F86
        CX = 0x000F
        JMP 33DEh
    0ADC:3417:
        AX = 0x38
        PUSH DS, SI, DI
        CALL 33F8h
        POP DI, SI, DS
        SI += 0x10
        DI += 0x10
        CX = 0x0A
    0ADC:342C:
        PUSH CX, DS, SI, DI
        AX = 0x33 - CX
        CALL 33F8h
        POP DI, SI
        DI += 0x10
        SI += 0x10
        POP DS, CX
        IF CX > 0 THEN CX--, JMP 342Ch              ; LOOP 342Ch
    0ADC:3444:
        return
    0ADC:3445:
        DI = 307Ah
        SI += 2
        AX = BX = 0
        _fmemcpy(ES:DI, DS:SI, 0x200)               ; CX = 0x200 ; REP MOVSB
    0ADC:3454:
        DI = 307Ah
        CX = DI
        CX += 0x200
        IF DI >= CX JMP 346Fh                       ; CMP DI, CX ; JNC 346Fh
        IF BYTE PTR ES:[DI] == 0 JMP 346Fh
        AL = BYTE PTR ES:[DI]                       ; AH = 0 at this point
        DI += AX
        JMP 345Dh
    0ADC:346F:
        WORD PTR ES:[3076] = DI - 0x307A            ; SUB DI, 307Ah ; MOV ES:[3076] DI
        DI = 3078h
        (TODO)

--

    0ADC:355B: (CL=0Ah entry point)
        IF WORD PTR DS:[1802] != 0 JMP 3563h
        return
    0ADC:3563: (CL=0Ah and WORD PTR DS:[1802] == 0)
        IF (BYTE PTR DS:[0037] & 8) != 0 JMP 357Eh
    0ADC:356A:
        AH = 00h
        INT 19h
        CX = 0
        IF AH == 0 JMP 3579h
        CX = 0xFFFF
    0ADC:3579:
        WORD PTR DS:[05DB],CX (AX value to return to caller)
        return
    0ADC:357E:
        IF (AH & 0xF0) != 0 JMP 3597h
        BYTE PTR DS:[0068],DH
        PUSH DX
        DL = (DL AND 0x0F)
        BYTE PTR DS:[0069] = (BYTE PTR DS:[0069] & 0xF0) | DL
        POP DX
        JMP 35A4h
    0ADC:3597:
        IF AH > 0x20 JMP 35A3h
        IF WORD PTR DS:[17FA] != 0 JMP 35A4h
    0ADC:35A3:
        return
    0ADC:35A4:
        AH = (DL & 0x30)
        BX = AH >> 3
        ES = WORD PTR DS:[1802]
        DI = WORD PTR ES:[BX+0x40]
        AH = AH | 7
        BX = 0
        IF (DH & 1) == 0 JMP 35C9h
        BH = BH | 0x30
    0ADC:35C9:
        AL = (DL & 0x0F) + 1
        DH = ((DH & 0xFE) | 0x02)
    0ADC:35D5:
        IF (AH & 0x30) != 0 JMP 35E1h
        IF (AL & 0x08) != 0 JMP 35E1h
        DH = DH | 1
    0ADC:35E1:
        CH = DH
        CL = 37h
        AL = AL - 2
        IF (CH & 0x04) != 0 JMP 35F1h
        BH = BH | 0x40
    0ADC:35F1:
        DX = 0x0100
        IF WORD PTR [F800:7FFC] == 0 JMP 3605h (IF WORD at memory address 0xFFFFC == 0)
        DX = DX + 1
    0ADC:3605:
        PUSH AX
        INT 19h, RESULT = AH
        POP AX
        IF RESULT == 0 JMP 3614h
        WORD PTR DS:[05DB] = 0xFFFF (returned to caller as AX)
        return
    0ADC:3614:
        WORD PTR DS:[05DB] = 0x0000 (returned to caller as AX)
        return

--

    0ADC:3744: (CL=0Fh entry point)
        AX = WORD PTR DS:[05DB]                     ; Caller AX value
        IF (AX & 0x8000) != 0 JMP 376Fh             ; OR AX,AX ; JS 376Fh
        IF AX == 0 JMP 3757h                        ; ... JZ 3757h
        AX--
        IF AX == 0 JMP 3763h                        ; JZ 3763h
        AX--
        IF AX == 0 JMP 375Dh                        ; JZ 375Dh
        AX--
        IF AX == 0 JMP 3769h                        ; JZ 3769h
    0ADC:3756:
        return
    0ADC:3757: (CL=0Fh AX==0)
        (BYTE PTR DS:[010C]) |= 0x01                ; OR ... , 01
        return
    0ADC:375D: (CL=0FH AX==2)
        (BYTE PTR DS:[010C]) |= 0x02                ; OR ... , 02
        return
    0ADC:3763: (CL=0FH AX==1)
        (BYTE PTR DS:[010C]) &= ~0x01               ; AND ... , FEh
        return
    0ADC:3769: (CL=0FH AX==3)
        (BYTE PTR DS:[010C]) &= ~0x02               ; AND ... , FDh
        return
    0ADC:376F: (CL=0FH AX==0x8000)
        TODO

--

    0ADC:378E: (CL=10h entry point)
        BX = WORD PTR DS:[05DB] caller's AX value
        BX = (BX >> 8)   (BX = caller's AH value)
        ADDR = (BX * 4) + 3A7Ch
        AX = WORD PTR CS:[ADDR]   (0ADC:[ADDR])
        BX = WORD PTR CS:[ADDR+2]
        CALL NEAR AX
        return

--

    0ADC:37A7: (CL=10h AH=00h entry point)
        CL = DL
        CALL NEAR BX (BX is 0A9C, no other case)
    0ADC:37AB:
        return

--

    0ADC:37AC: (CL=10h AH=01h entry point)
        ES = WORD PTR DS:[05E1]                     ; caller's DS segment
        BX = DX                                     ; caller's DX register
    0ADC:37B2:
        CL = BYTE PTR ES:[BX]
        IF CL == 0x24 JMP 37C2h                     ; stop at '$'
        PUSH BX
        CALL A9Ch
        POP BX
        INC BX
        JMP 37B2h
    0ADC:37C2:
        return

--

    0ADC:37C3: (CL=10h AH=02h entry point)
        BYTE PTR DS:[011D],DL                       ; DL = caller's DL, assign to character attribute
        WORD PTR DS:[013C],DX                       ; DX = caller's DX, assign to extended character attribute
        return

--

    0ADC:37CC: (CL=10h AH=03h entry point)
        DX += 0x2020                                ; DX = caller's DX add 0x20 to both bytes
        swap(DL,DH)                                 ; XCHG DL, DH
        CX = DX
        CALL WORD PTR CS:[BX]                       ; BX = 0x0AFA in all cases, which then feeds the "ESC =" handling code
        return

--

    0ADC:37D7: (CL=10h AH=04h entry point, where BX = 0x0B90 aka the "ESC D" handler)
               (CL=10h AH=05h entry point, where BX = 0x0B99 aka the "ESC M" handler)
        CALL WORD PTR CS:[BX]                       ; BX = 0x0B90 or BX = 0x0B99
        return

--

    0ADC:37DA: (CL=10h AH=06h entry point, where BX = 0x0C52 aka the "ESC [ A" handler)
               (CL=10h AH=07h entry point, where BX = 0x0C75 aka the "ESC [ B" handler)
               (CL=10h AH=08h entry point, where BX = 0x0C9C aka the "ESC [ C" handler)
               (CL=10h AH=09h entry point, where BX = 0x0CC3 aka the "ESC [ D" handler)
               (CL=10h AH=0Ch entry point, where BX = 0x0E4E aka the "ESC [ L" handler)
               (CL=10h AH=0Dh entry point, where BX = 0x0E72 aka the "ESC [ M" handler)
        DH = 0
        CX = DX
        CALL WORD PTR CS:[BX]
        return

--

    0ADC:37E1: (CL=10h AH=0Ah entry point, where BX = 0x0D2E)
               (CL=10h AH=0Bh entry point, where BX = 0x0D72)
        DH = 0
        AX = DX
        CALL WORD PTR CS:[BX]
        return

--

    0ADC:3A5C array of WORD values, offsets of procedures for each value of CL.

    0ADC:3A5C table contents.
    Note the INT DCh code maps:
        CL = 0x09..0x15 to table index 0x00..0x0C
        CL = 0x80..0x82 to table index 0x0D..0x0F

                  .     .     .     .     .     .     .     .
    0ADC:00003A5C B8 3A 5B 35 A4 31 A5 31 DF 32 1B 36 44 37 8E 37
                  .     .     .     .     .     .     .     .    |END
    0ADC:00003A6C F0 37 6E 38 F7 38 85 3B 52 3C 27 39 B5 39 10 3A

    CL = 0x09    0x3AB8
    CL = 0x0A    0x355B
    CL = 0x0B    0x31A4
    CL = 0x0C    0x31A5
    CL = 0x0D    0x32DF
    CL = 0x0E    0x361B
    CL = 0x0F    0x3744
    CL = 0x10    0x378E
    CL = 0x11    0x37F0
    CL = 0x12    0x386E
    CL = 0x13    0x38F7
    CL = 0x14    0x3B85
    CL = 0x15    0x3C52
    CL = 0x80    0x3927
    CL = 0x81    0x39B5
    CL = 0x82    0x3A10

--

    0ADC:3A7C array of WORD value pairs (address, parameter). NOTE: Lack of range checking!
              Lookup table for CL=10h, index by AH

                  .           .           .           .
    0ADC:00003A7C A7 37 9C 0A AC 37 00 00 C3 37 00 00 CC 37 FA 0A  .7...7...7...7..
                  .           .           .           .
    0ADC:00003A8C D7 37 90 0B D7 37 99 0B DA 37 52 0C DA 37 75 0C  .7...7...7R..7u.
                  .           .           .           .
    0ADC:00003A9C DA 37 9C 0C DA 37 C3 0C E1 37 2E 0D E1 37 72 0D  .7...7...7...7r.
                  .           .           .           .          |END?
    0ADC:00003AAC DA 37 4E 0E DA 37 72 0E E8 37 2D 0B 8B 16 E3 05  .7N..7r..7-.....
                  ?           ?           ?           ?
    0ADC:00003ABC A1 DB 05 3D 00 00 74 10 3D 01 00 74 20 3D 10 00  ...=..t.=..t =..
                  ?           ?           ?           ?
    0ADC:00003ACC 74 4E 3D 11 00 74 49 C3 8E 06 E1 05 8B FA 2E 8E  tN=..tI.........

    0ADC:3A7C Call table (address, param)

    Calling convention on entry: AX = subroutine address, BX = param
    
    AH = 0x00    0x37A7    0x0A9C  ; 0ADC:00003A7C
    AH = 0x01    0x37AC    0x0000
    AH = 0x02    0x37C3    0x0000
    AH = 0x03    0x37CC    0x0AFA
    AH = 0x04    0x37D7    0x0B90  ; 0ADC:00003A8C
    AH = 0x05    0x37D7    0x0B99
    AH = 0x06    0x37DA    0x0C52
    AH = 0x07    0x37DA    0x0C75
    AH = 0x08    0x37DA    0x0C9C  ; 0ADC:00003A9C
    AH = 0x09    0x37DA    0x0CC3
    AH = 0x0A    0x37E1    0x0D2E
    AH = 0x0B    0x37E1    0x0D72
    AH = 0x0C    0x37DA    0x0E4E  ; 0ADC:00003AAC
    AH = 0x0D    0x37DA    0x0E72
    AH = 0x0E    0x37E8    0x0B2D
    AH = 0x0F    0x168B    0x05E3

--

    0ADC:3AB8: (INT DCh CL=0x09) [DS=0x0060 DOS SEGMENT]
        DX = WORD PTR DS:[05E3] (caller DX)
        AX = WORD PTR DS:[05DB] (caller AX)
        IF AX == 0x0000 JMP 3AD4h
        IF AX == 0x0001 JMP 3AE9h
        IF AX == 0x0010 JMP 3B1Ch
        IF AX == 0x0011 JMP 3B1Ch
    0ADC:3AD3:
        return
    0ADC:3AD4: (INT DCh CL=0x09 AX=0x0000)
        ES = WORD PTR DS:[05E1] (caller DS)
        DI = DX (caller DX)
        DS = WORD PTR CS:[0030] (DOS segment 60h)
        SI = 1DBBh
        _fmemcpy(ES:DI,DS:SI,8)
        return
    0ADC:3AE9: (INT DCh CL=0x09 AX=0x0001)
        WORD PTR DS:[05DB] = 0xFFFF (caller AX set to 0xFFFF)
        IF BYTE PTR DS:[1DC4] == 0 JMP 3B1Bh
        DL = DL AND 0x0F
        BX = 26Ah
        CX = (WORD)BYTE PTR DS:[1DC4] * 4
    0ADC:3B06:
        IF BYTE PTR DS:[BX+2] != DL JMP 3B15h
        BYTE PTR DS:[BX+1] = 2
        WORD PTR DS:[05DB] = 0x0000 (caller AX set to 0x0000)
        BX += WORD PTR DS:[0268]
        IF CX > 0 THEN CX--, JMP 3B06h
    0ADC:3B1B:
        return
    0ADC:3B1C: (INT DCh CL=0x09 AX=0x0010 or AX=0x0011)
        WORD PTR DS:[05DB] = 0xFFFF (caller AX set to 0xFFFF)
        IF BYTE PTR DS:[1DC4] == 0 JMP 3B84h
        DX = DX AND 0xFF
        CX = 0x1A
        BX = 2C86h
    0ADC:3B31:
        IF BYTE PTR DS:[BX+1] == DL JMP 3B3Fh
        BX += 2
        DH += 1
        IF CX > 0 THEN CX--, JMP 3B31h
        JMP 3B84h
    0ADC:3B3F:
        IF (BYTE PTR DS:[BX] AND 1) == 0 JMP 3B84h
        BYTE PTR DS:[0136] = DH (set last access drive number)
        IF AX != 0x0010 JMP 3B52h
        CALL 5B07h
        JMP 3B69h
    0ADC:3B52:
        WORD PTR DS:[05DB] = 0x0001 (caller AX set to 0x0001)
        BX = DL & 0x0F
        IF BYTE PTR [BX+03E9h] == 0 JMP 3B84h
        CALL 5B4Bh, RESULT = CARRY FLAG
    0ADC:3B69:
        WORD PTR DS:[05DB] = 0x0000
        IF RESULT == 0 (CF=0) JMP 3B84h
        WORD PTR DS:[05DB] = 0x0002
        IF BYTE PTR DS:[025B] == 2 JMP 3B84h
        WORD PTR DS:[05DB] = 0x0003
    0ADC:3B84:
        return

--

    Lookup table for CL=0Ch, AL=01h through 0FEh inclusive

    0ADC:00003DE0 01 02 03 04 05 06 07 08 09 0A 10 11 12 13 14 15  ................
    0ADC:00003DF0 16 17 18 19 1F 20 21 22 23 24 25 26 27 28 29 0B  ..... !"#$%&'().
    0ADC:00003E00 0C 0D 0E 0F 1A 1B 1C 1D 1E|1E 2E 8E 1E 30 00 80  .............0..

--

    Called from 0ADC:3180 (near)

    0ADC:4080:
        CLI
        PUSH DS
        DS = DOS kernel segment 60h from 0ADC:0030
        BYTE PTR DS:[00B4] = 0
    0ADC:408C:
        IF BYTE PTR DS:[00A5] != 01h JMP 40ACh
    0ADC:4093:
        IF BYTE PTR DS:[00A6] == 01h JMP 40A2h
    0ADC:409A:
        STI
        PUSH AX
        CALL 40AFh
        POP AX
        POP DS
        return
    0ADC:40A2:
        BYTE PTR DS:[00A5] = 0x00
        BYTE PTR DS:[00A6] = 0x00
    0ADC:40AC:
        STI
        POP DS
        return
    0ADC:40AF:
        IF WORD PTR DS:[2A7C] == 0h JMP 40C6h
    0ADC:40B6:
        PUSH DS
        WORD PTR DS:[2A7A] = 0x18
        CALL FAR 0060h:3C6Fh
        NOP
        NOP
        NOP
        POP DS
    0ADC:40C6:
        return

