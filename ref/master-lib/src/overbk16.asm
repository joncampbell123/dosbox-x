; master library - PC-9801V GRCG supersfx
;
; Description:
;	�O���t�B�b�N��ʏ��(x*8, y)������Ƃ���16x16dot���N���A����
;	y�����̂݃N���b�s���O����
;	
;
; Functions/Procedures:
;	void over_blk16_8(int x, int y)
;
; Parameters:
;	
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
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
;	iR
;
; Revision History:
;	94/ 7/19 Initial: overbk16.asm/master.lib 0.23 from supersfx.lib(iR)

	.186
	.MODEL SMALL
	include func.inc

	.CODE

func OVER_BLK16_8	; over_blk16_8() {
	push	BP
	mov	BP,SP
	push	DI

	x = (RETSIZE+2)*2
	y = (RETSIZE+1)*2

	; �N���b�s���O
	mov	cx,16
	mov	di,[BP+y]
		
	cmp	di,400 - 16
	jbe	short _2
	mov	ax,di
	jg	short _0
	xor	di,di
	neg	ax
	jmp	short _1
_0:	sub	ax,400 - 16
_1:	sub	cx,ax
	jle	short return
_2:
	; �u���b�N��GVRAM�A�h���X�v�Z
	mov	ax,0a800h
	mov	es,ax
	imul	di,80
	add	di,[BP+x]
	; GRCG TDW ���[�h
	mov	al,080h
	out	7ch,al
	; �F�Z�b�g
	xor	al,al
	mov	dx,7eh
	out	dx,al
	out	dx,al
	out	dx,al
	out	dx,al
	cld
	even
_3:	stosw
	add	di,80 - 2
	loop	_3
	; GRCG - off
	out	7ch,al
return:
	pop	DI
	pop	BP
	ret	4
endfunc			; }

END
