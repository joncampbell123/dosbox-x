; master library - BGM
;
; Description:
;
;
; Function/Procedures:
;	int _bgm_mget(PART near *part2);
;
; Parameters:
;
;
; Returns:
;
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;
;
; Assembly Language Note:
;
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	femy(淀  文武)		: オリジナル・C言語版
;	steelman(千野  裕司)	: アセンブリ言語版
;
; Revision History:
;	93/12/19 Initial: b_mget.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	lcount:WORD

	.CODE
	EXTRN	BGM_SET_TEMPO:CALLMODEL

GETNUMBER:
	les	BX,[SI].pptr
	mov	AL,ES:[BX]
	sub	AL,'0'
	cmp	AL,9
	ja	short GETNUMBERLOOPOUT
	sub	AH,AH
even
GETNUMBERLOOP:
	;temp = temp * 10 + (*part2->ptr - '0');
	mov	DX,CX
	shl	DX,3
	add	DX,CX
	add	CX,DX
	add	CX,AX
	inc	BX
	mov	AL,ES:[BX]
	sub	AL,'0'
	cmp	AL,9
	jbe	short GETNUMBERLOOP
even
GETNUMBERLOOPOUT:
	mov	word ptr [SI].pptr,BX
	ret

func _BGM_MGET
	mov	BX,SP
	push	SI
	part2	= (RETSIZE+0)*2

	mov	SI,SS:[BX+part2]

	;音符が決まるか最後まで調べるまで繰り返す
	jmp	WHILEEND
even
WHILESTART:
	xor	CX,CX			;temp = 0;
	;switch (part2->note = *part2->ptr++) {
	mov	AL,ES:[BX]
	mov	byte ptr [SI].note,AL
	inc	word ptr [SI].pptr
	cbw
	cmp	AX,'O'
	jne	short SWITCH1
	jmp	OCTAVE
even
SWITCH1:
	sub	AX,'<'
	test	AL,1
	jne	short ONPU
	cmp	AX,'T' - '<'
	ja	short ONPU
	xchg	AX,BX
	jmp	word ptr CS:SWITCHTABLE[BX]
even
SWITCHTABLE:
	DW	DECOCTAVE
	DW	INCOCTAVE
	DW	ONPU
	DW	ONPU
	DW	ONPU
	DW	ONPU
	DW	ONPU
	DW	ONPU
	DW	SETLENGTH
	DW	TENOOT
	DW	ONPU
	DW	KYUUFU
	DW	SETTEMPO

	;休符 
even
KYUUFU:
	mov	byte ptr [SI].note,REST
	jmp	short NOTELENGTH

	;音符
even
ONPU:
	mov	AL,byte ptr [SI].note
	sub	AL,'A'
	cmp	AL,'G' - 'A'
	ja	short WHILEEND
even
ONPUCHECK:
	les	BX,[SI].pptr
	mov	AL,ES:[BX]
	cmp	AL,'+'
	je	short SHARP
	cmp	AL,'#'
	jne	short NOTSHARP
SHARP:
	;シャープだった
	add	byte ptr [SI].note,DIVNUM
	inc	word ptr [SI].pptr
NOTSHARP:
	les	BX,[SI].pptr
	cmp	byte ptr ES:[BX],'-'
	jne	short NOTELENGTH
	;フラットだった
	add	byte ptr [SI].note,DIVNUM*2
	inc	word ptr [SI].pptr

	;以下音符と休符共通
even
NOTELENGTH:
	call	GETNUMBER
	mov	[SI].len,CX
	or	CX,CX
	jle	short NOTECHECK1
	cmp	CX,MINNOTE
	jle	short NOTECHECK2
NOTECHECK1:
	mov	CX,[SI].dflen
	mov	[SI].len,CX
NOTECHECK2:
	;part2->lcnt = lcnt[part2->len * 2 - 1];
	mov	BX,CX
	shl	BX,2
	mov	AX,lcount[BX-2]
	mov	[SI].lcnt,AX
	;付点があったら音長を 1.5倍する
	les	BX,[SI].pptr
	cmp	byte ptr ES:[BX],'.'
	jne	short EXITTRUE
	mov	BX,AX
	sal	AX,1
	adc	AX,BX
	sar	AX,1
	mov	[SI].lcnt,AX
	inc	word ptr [SI].pptr
EXITTRUE:				;鳴らす音が決まった
	mov	AX,TRUE	
	pop	SI
	ret	2

WHILEEND:				;まだ鳴らす音が決まってない
	les	BX,[SI].pptr
	cmp	byte ptr ES:[BX],0
	je	short EXITFALSE
	jmp	WHILESTART

EXITFALSE:				;決まってないけど曲の最後だからー
	xor	AX,AX
	pop	SI
	ret	2

	;音長
even
SETLENGTH:
	call	GETNUMBER
	cmp	CX,MINNOTE
	ja	short WHILEEND
	mov	[SI].dflen,CX
	jmp	short WHILEEND

	;テンポ
even
SETTEMPO:
	call	GETNUMBER
	push	CX
	call	BGM_SET_TEMPO
	jmp	short WHILEEND

	;オクターブ
even
OCTAVE:
	les	BX,[SI].pptr
	mov	AL,ES:[BX]
	sub	AL,'1'
	cmp	AL,7
	ja	short WHILEEND
	sub	AH,AH
	inc	AX
	mov	[SI].oct,AX
	inc	word ptr [SI].pptr
	jmp	short WHILEEND

	;オクターブ--
even
DECOCTAVE:
	dec	[SI].oct
	cmp	[SI].oct,0
	jne	short WHILEEND
	inc	[SI].oct
	jmp	short WHILEEND

	;オクターブ++
even
INCOCTAVE:
	inc	[SI].oct
	cmp	[SI].oct,9
	jne	short WHILEEND
	dec	[SI].oct
	jmp	short WHILEEND

	;テヌート
even
TENOOT:				;ってスペルは適当(笑)
	les	BX,[SI].pptr
	mov	[SI].tnt,OFF
	cmp	byte ptr ES:[BX],'1'
	jne	short WHILEEND
	mov	[SI].tnt,ON
	jmp	short WHILEEND
endfunc
END

