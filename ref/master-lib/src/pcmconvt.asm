; master library - PC98V - PCM
;
; Description:
;	pcm_play用のPCMデータの準備
;
; Function:
;	void pcm_convert( void far * dest , const void far * src,unsigned rate,unsigned long size)
;
; Parameters:
;	void far *dest	= 格納場所の先頭アドレス
;	const void far *src	= PCMデータの先頭アドレス
;	unsigned rate	= 演奏レート (1パルスの大きさ)(1~65535)
;	unsigned long size	= パルス数(0なら演奏しにゃい)

; Returns:
;	none

; Binding Target:
;	MSC/TC/BC/TP

; Running Target:
;	MS-DOS

; Requiring Resources:
;	CPU: 8086

; Compiler/Assembler:
;	OPTASM 1.6
;	TASM
;
; Notes:
;	srcデータ形式は、ベタの8bit PCMデータです。
;	destには、sizeバイトのメモリが必要です。
;	srcとdestは一致できます。
;
; Author:
;	新井俊一 (SuCa: pcs28991 fem20932)
;
; Rivision History:
;	92/10/26 Initial
;	92/12/03 ぐう
;	92/12/09 8bit固定, 1の補数化(koizuka)
;	92/12/14 1の補数って意味なかったぜ
;	95/ 3/15 [M0.22k] BUGFIX: src,destの順序が逆だった

	.MODEL SMALL
	include func.inc

	.CODE

func PCM_CONVERT	; pcm_convert() {
	push	BP
	mov	BP,SP

	; 引数
	dst_seg	= (RETSIZE+7)*2
	dst_off	= (RETSIZE+6)*2
	src_seg	= (RETSIZE+5)*2
	src_off	= (RETSIZE+4)*2
	rate	= (RETSIZE+3)*2
	pcm_lh	= (RETSIZE+2)*2
	pcm_ll	= (RETSIZE+1)*2

	push	DS
	push	SI
	push	DI

	lds	SI,[BP+src_off]	; DS:SI = 元データ
	xor	AX,AX
	les	DI,[BP+dst_off]	; ES:DI = 先データ

	mov	CX,[BP+pcm_ll]	; BPCX = データ長
	mov	BX,[BP+rate]	; BX = 演奏レート
	mov	BP,[BP+pcm_lh]

	inc	BP
	jcxz	short SEGCHK

	EVEN
OKAY1:
	mov	AL,[SI]		; 1バイト取る
				; scale max from 255 to BL
	mul	BL		; AH = AL * BL / 256
	mov	ES:[DI],AH	; ↑を書き込む
	inc	DI
	jnz	short SEND

	mov	AX,ES
	add	AX,1000h
	mov	ES,AX

SEND:
	inc	SI
	jnz	short OKAY2

	mov	AX,DS
	add	AX,1000h
	mov	DS,AX

	; BPCXの回数のループ
OKAY2:
	loop	short OKAY1
SEGCHK:
	dec	BP
	jnz	short OKAY1

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	14
endfunc			; }

END
