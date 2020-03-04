; EGC - PC98VX
;
; Function:
;	void _pascal egc_on( void ) ;
;	void _pascal egc_off( void ) ;
;	void _pascal egc_start( void ) ;
;	void _pascal egc_end( void ) ;
;
; Description:
;	EGCを拡張モード／GRCG互換モードに設定する／パラメータを初期設定する
;
; Parameters:
;	void
;
; Returns:
;	void
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	PC-9801VX Normal Mode
;
; Requiring Resources:
;	GRAPHICS ACCERALATOR: EGC
;
; Assembler:
;	OPTASM 1.6
;
; Assemble Option:
;	optasm /mx
;
; Notes:
;	EGCを搭載していない機種で実行するとハングアップする場合があります。
;	GDC描画中に呼び出すと誤動作します。
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/ 5/ 4 Initial
;	92/ 6/30 gc_poly.libに追加

EGC_ACTIVEPLANEREG	equ 04a0h
EGC_READPLANEREG	equ 04a2h
EGC_MODE_ROP_REG	equ 04a4h
EGC_FGCOLORREG		equ 04a6h
EGC_MASKREG		equ 04a8h
EGC_BGCOLORREG		equ 04aah
EGC_ADDRRESSREG		equ 04ach
EGC_BITLENGTHREG	equ 04aeh


	.MODEL SMALL
	.CODE
	include FUNC.INC

; EGCを拡張モードに設定
; 92/5/4
; void _pascal ecg_on() ;
FUNC EGC_ON
	mov	AL,00h	; GRCG OFF
	out	7ch,AL
	mov	AL,07h	; EGC Reg Write Enable
	out	6ah,AL
	mov	AL,05h	; EGC extended mode ( Ex bit on )
	out	6ah,AL
	mov	AL,80h	; GRCG ON
	out	7ch,AL
	mov	AL,06h	; EGC Reg Write Disable
	out	6ah,AL
	ret
ENDFUNC

; EGCをGRCG互換モードに設定
; 92/5/4
; void _pascal egc_off(void) ;
FUNC EGC_OFF
	mov	AX,0FFF0h
	mov	DX,04a0h
	out	DX,AX	; EGC Active plane = ALL

	mov	AX,0FFFFh ; mask reg
	mov	DX,04a8h
	out	DX,AX

	mov	AL,07h	; EGC Reg Write Enable
	out	6ah,AL
	mov	AL,04h	; GRCG compatible mode ( Ex bit off )
	out	6ah,AL
	mov	AL,00h	; GRCG OFF
	out	7ch,AL
	mov	AL,06h	; EGC Reg Write Disable
	out	6ah,AL

	ret
ENDFUNC

; EGCの初期設定
; 92/7/1
; void _pascal egc_start(void) ;
; void _pascal egc_end(void) ;
FUNC EGC_END
ENDFUNC
FUNC EGC_START
	call EGC_ON
	mov	DX,EGC_ACTIVEPLANEREG
	mov	AX,0FFF0h
	out	DX,AX
	mov	DX,EGC_READPLANEREG
	mov	AX,00FFh
	out	DX,AX
	mov	DX,EGC_MASKREG
	mov	AX,0FFFFh
	out	DX,AX
	mov	DX,EGC_ADDRRESSREG
	xor	AX,AX
	out	DX,AX
	mov	DX,EGC_BITLENGTHREG
	mov	AX,000Fh
	out	DX,AX
	call EGC_OFF
	ret
ENDFUNC

END
