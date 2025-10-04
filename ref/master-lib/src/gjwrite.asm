; master library - PC98
;
; Description:
;	外字の登録
;
; Function/Procedures:
;	void gaiji_write( unsigned short chr, const void far * pattern ) ;
;	void gaiji_write_all( const void far * patterns ) ;
;
; Parameters:
;	unsigned chr		外字コード( 0～255 )
;	void far * pattern 	登録するパターン ( 左、右の順に1byteずつ
;						　16ライン分並ぶ(計32bytes))
;	void far * patterns	パターンが連続して 256個ならぶ
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/24 Initial

	.MODEL SMALL
	include func.inc

	.CODE

;	lo byte = 左, high byte = 右半分
; IN:
;	AX = JIS - 2000h
;	DS:SI = read pointer
; BREAK:
;	AX,CX,BX
;	SI = next address
SETFONTW proc near
	out	0a1h,AL		; 文字コード下位
	mov	AL,AH
	out	0a3h,AL		; 文字コード上位

	mov	CX,16
	mov	BX,0
GLOOP:	mov	AL,BL		; キャラクタＲＯＭへ書き込む
	or	AL,20h
	out	0a5h,AL		; 左半分
	lodsw
	out	0a9h,AL
	mov	AL,BL
	out	0a5h,AL		; 右半分
	mov	AL,AH
	out	0a9h,AL
	inc	BX
	loop	short GLOOP
	ret
SETFONTW endp


func GAIJI_WRITE
	push	DS
	push	SI
	mov	DX,SP
	CLI
	add	SP,(RETSIZE+2)*2
	pop	SI	; pattern
	pop	DS	; FP_SEG(pattern)
	pop	AX	; chr
	mov	SP,DX
	STI

	mov	AH,0
	add	AX,5680h	; 外字コード生成
	and	AL,7fh

	push	AX
	mov	AL,0bh		; ＣＧドットアクセスにするん
	out	68h,AL
	pop	AX
	call	SETFONTW
	mov	AL,0ah		; ＣＧコードアクセスにするん
	out	68h,AL

	pop	SI
	pop	DS
	ret	6
endfunc

func GAIJI_WRITE_ALL
	push	DS
	push	SI

	; 引数
	patterns = (RETSIZE+2)*2

	mov	SI,SP
	lds	SI,SS:[SI+patterns]

	mov	AL,0bh		; ＣＧドットアクセスにするん
	out	68h,AL

	mov	DX,0
WLOOP:
	mov	AX,DX		; 外字コード生成
	adc	AX,5680h
	and	AL,7fh
	call	SETFONTW
	inc	DL
	jnz	short WLOOP

	mov	AL,0ah		; ＣＧコードアクセスにするん
	out	68h,AL

	pop	SI
	pop	DS
	ret	4
endfunc

END
