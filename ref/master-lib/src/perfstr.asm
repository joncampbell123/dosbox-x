; ��Ԃ̏���Ԃ𑪒肵�����Ԃ𕶎���ɕϊ�����
;
; Function:
;	void perform_str( char * buf, unsigned long perform ) ;
;
; Parameters:
;	char; buf		�������ݐ�B
;	unsigned long perform	perform_stop()�̖߂�l
;
; Description:
;	perform_stop()�ɂ���ē����A��������Ԃ𕶎���ɕϊ�����B
;	buf�Ɏ��Ԃ��A11�����̎���������Ƃ��Ċi�[����B
;	�i�[���鏑���́Astr_printf�� "%6ld.%04ld"�̌`�B
;	"1.0000"���P�~���b��\���B
;
; Prototype:
;	#include "perform.h"
;
; �Q�Ɗ֐�:
;	perform_start()
;	perform_stop()
;
; �Ώۋ@��:
;	NEC PC-9801series
;	PC/AT
;
; �v�����x:
;	refer perform_start()
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/5/18 Initial(perf_str.c)
;	92/5/25 perform_start/stop��performa.asm�Ɉړ�
;	92/12/19 perfstr.asm ( master.lib )
;	93/ 5/29 [M0.18] .CONST->.DATA
;	93/12/ 7 [M0.22] add variable perform_Rate
;
	.MODEL SMALL
	include func.inc
	EXTRN _str_printf:CALLMODEL

	.DATA
	EXTRN perform_Rate:WORD
formatstr db '%6ld.%04ld',0

	.CODE

; DXAX / CX = DXAX ... BX
; 92/12/19
	public ASM_LWDIV
ASM_LWDIV	proc near
LLONG:
	xor	BX,BX
	test	DX,DX		; �����̂Q�s������4bytes���邪
	je	short LWORD	; ������͔��������c
	xchg	BX,AX	; BX <- AX ( �폜���̉��� )
	xchg	AX,DX	; AX <- DX ( �폜���̏�� )
			; DX = 0
	div	CX
	xchg	AX,BX	; BX = ���̏��16bit
LWORD:
	div	CX
	xchg	DX,BX	; BX = �]��
	ret
	EVEN
ASM_LWDIV	endp

func PERFORM_STR
	push	BP
	mov	BP,SP
	push	SI
	; ����
	buf	= (RETSIZE+3)*2
	perform = (RETSIZE+1)*2

	mov	CX,perform_Rate
	shr	CX,1
	mov	AX,[BP+perform]
	mov	DX,[BP+perform+2]

	mov	SI,AX	; DXAX��5�{
	mov	BX,DX
	shl	AX,1
	rcl	DX,1
	shl	AX,1
	rcl	DX,1
	add	AX,SI
	adc	DX,BX

	call	ASM_LWDIV	; tim / rate
	push	DX
	mov	SI,AX

	mov	AX,10000	; tim % rate; 10000 / rate
	mul	BX
	call	ASM_LWDIV
	pop	BX
	push	DX
	push	AX
	push	BX
	push	SI

	_push	DS
	mov	AX,offset formatstr
	push	AX
	_push	[BP+buf+2]
	push	[BP+buf]

	call	_str_printf
	add	SP,(4+DATASIZE*2)*2
	pop	SI
	pop	BP
	ret	(2+DATASIZE)*2
endfunc

END
