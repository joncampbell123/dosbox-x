Entry point (MS-DOS 5.00) 1.44MB disk image (on my hard drive, boot144.dsk). Configuration menu option "nothing". 0ADC segment may change location based on configuration.

--

    0060:0124 WORD ACFh BX value (?))
    0060:0128 BYTE (?)
    0060:014E BYTE some sort of flag
    0060:0214 WORD:WORD 16-bit far pointer (0ADC:3126)
    0060:05E1 WORD stored DS value from caller
    0060:05DB WORD stored AX value from caller
    0060:05DD WORD stored SS value from caller
    0060:05DF WORD stored SP value from caller (after INT DCh int frame and PUSH DS)
    0060:05E3 WORD stored DX value from caller
    0060:05E5 WORD stored BX value from caller
    0060:0767 Stack pointer (from DOS segment), stack switches to on entry to procedure
    0060:36B3 INT DCh entry point
    0060:3B30 Subroutine called on INT DCh if 0060:014E is nonzero

--

INT DC = 60:36B3

    0060:36B3:
        if BYTE PTR cs:[014E] == 0 jmp 0060:36BE
        call 3b30
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

    0ADC:0A00 Subroutine lookup table
        Referred from 0ADC:0AAC, CL is single char or first byte of kanji

                  .        .        .        .        .        .
    0ADC:00000A00 07 E7 10 08 19 11 09 F8 10 0A 49 11 0B 3A 11 0C  ..........I..:..
                        .        .        .        .        .
    0ADC:00000A10 8C 11 0D 5E 11 1A 7D 11 1B C1 10 1E 6B 11 00 B3  ...^..}.....k...
                    |END
    0ADC:00000A20 11 5B B2 0B 3D E4 0A 2A 84 0B 28 77 0B 44 90 0B  .[..=..*..(w.D..

    CL value    |   Subroutine address
    CL = 0x07       0x10E7                  ; CTRL+G / BEL
    CL = 0x08       0x1119                  ; CTRL+H / BACKSPACE
    CL = 0x09       0x10F8                  ; CTRL+I / TAB
    CL = 0x0A       0x1149                  ; CTRL+J / LINEFEED
    CL = 0x0B       0x113A                  ; CTRL+K / VERTICAL TAB
    CL = 0x0C       0x118C                  ; CTRL+L / FORM FEED
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

    0ADC:00000A7C E2 12 F2 12 E2 12 CA 12 BC 12 F2 12 C3 12 D1 12  ................
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

    0ADC:11B3: (CL=10h AH=00h, at this time CL == caller's DL and DS = DOS segment 60h)
        IF BYTE PTR DS:[011C] < 0x50 JMP 11C7h ; (60:11C cursor X position)
        IF BYTE PTR DS:[0117] == 0 JMP 11C2h ; (60:117 line wrap flag)
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
        (TODO finish this later, at 0ADC:11FA)
    0ADC:1201:
        CH = 0
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
        return

--

    0ADC:12E2 (DS = DOS segment 60h, ES = Text VRAM segment A000h, AX = character code, DI = memory offset)
        WORD PTR ES:[DI] = AX ; write character code
        DI += 0x2000
        WORD PTR ES:[DI] = WORD PTR DS:[013C] (60:13C display attribute in extended attribute mode) ; write attribute code
        AL = 1 (this indicates to caller to move cursor X position 1 unit to the right)
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

--

    0ADC:3A5C array of WORD values, offsets of procedures for each value of CL.

    0ADC:3A5C table contents.
    Note the INT DCh code maps:
        CL = 0x09..0x15 to table index 0x00..0x0C
        CL = 0x80..0x82 to table index 0x0D..0x0F

    0ADC:00003A5C B8 3A 5B 35 A4 31 A5 31 DF 32 1B 36 44 37 8E 37
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

    0ADC:00003A7C A7 37 9C 0A AC 37 00 00 C3 37 00 00 CC 37 FA 0A  .7...7...7...7..
    0ADC:00003A8C D7 37 90 0B D7 37 99 0B DA 37 52 0C DA 37 75 0C  .7...7...7R..7u.
    0ADC:00003A9C DA 37 9C 0C DA 37 C3 0C E1 37 2E 0D E1 37 72 0D  .7...7...7...7r.
    0ADC:00003AAC DA 37 4E 0E DA 37 72 0E E8 37 2D 0B 8B 16 E3 05  .7N..7r..7-.....
    0ADC:00003ABC A1 DB 05 3D 00 00 74 10 3D 01 00 74 20 3D 10 00  ...=..t.=..t =..
    0ADC:00003ACC 74 4E 3D 11 00 74 49 C3 8E 06 E1 05 8B FA 2E 8E  tN=..tI.........

    0ADC:3A7C Call table (address, param)

    Calling convention on entry: AX = subroutine address, BX = param
    
    AH = 0x00    0x37A7    0x0A9C  ; 0ADC:00003A7C
    AH = 0x01    0x37AC    0x0000
    AH = 0x02    0x37C3    0x0000
    AH = 0x03    0x37CC    0x0AFA
    AH = 0x04    0x37D7    0x0B90  ; 0ADC:00003A8C
    AH = 0x05    0x37D7    0x0BDA
    AH = 0x06    0x37DA    0x0B99
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

    0ADC:3AB8: (INT DCh CL=0x09)
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
        BYTE PTR DS:[0136] = DH
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

    0ADC:4080:
        DS = DOS kernel segment 60h from 0ADC:0030
        BYTE PTR DS:[00B4] = 0
        (other cleanup, not yet traced)
        CALL 0060:3C6F
        RET

