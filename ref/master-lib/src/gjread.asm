; master library - PC98
;
; Description:
;	外字の読みだし
;
; Function/Procedures:
;	void gaiji_read( unsigned short chr, void far * pattern ) ;
;	void gaiji_read_all( void far * patterns ) ;
;
; Parameters:
;	unsigned chr		外字コード( 0～255 )
;	void far * pattern 	パターン格納先 ( 左、右の順に1byteずつ
;						　16ライン分並ぶ(計32bytes))
;	void far * patterns	パターンが連続して 256個書き込まれる
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
;	ES:DI = read pointer
; BREAK:
;	AX,CX,BX
;	DI = next address
GETFONTW proc near
	out	0a1h,AL		; JISコード下位
	mov	AL,AH
	out	0a3h,AL		; JISコード上位

	mov	CX,16
	mov	BX,0
GLOOP:	mov	AL,BL		; キャラクタＲＯＭから読み込む
	out	0a5h,AL		; 右半分
	in	AL,0a9h
	mov	AH,AL
	mov	AL,BL
	or	AL,20h
	out	0a5h,AL		; 左半分
	in	AL,0a9h
	stosw
	inc	BX
	loop	short GLOOP
	ret
GETFONTW endp

func GAIJI_READ
	push	DI
	mov	DX,SP
	CLI
	add	SP,(RETSIZE+1)*2
	pop	DI	; pattern
	pop	ES	; FP_SEG(readto)
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
	call	GETFONTW
	mov	AL,0ah		; ＣＧコードアクセスにするん
	out	68h,AL

	pop	DI
	ret	6
endfunc

func GAIJI_READ_ALL
	push	DI

	; 引数
	patterns = (RETSIZE+1)*2
	mov	DI,SP
	les	DI,SS:[DI+patterns]

	mov	AL,0bh		; ＣＧドットアクセスにするん
	out	68h,AL

	mov	DX,0
RLOOP:
	mov	AX,DX		; 外字コード生成
	adc	AX,5680h
	and	AL,7fh
	call	GETFONTW
	inc	DL
	jnz	short RLOOP

	mov	AL,0ah		; ＣＧコードアクセスにするん
	out	68h,AL

	pop	DI
	ret	4
endfunc

END
