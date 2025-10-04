; master library - PC98
;
; Description:
;	�O���t�B�b�N��� ��8dot�P�ʂ̉�ʗ̈�ۑ�/����
;
; Function/Procedures:
;	�ۑ�
;	void graph_byteget( int cx1,int y1, int cx2,int y2, void far *buf ) ;
;
;	����
;	void graph_byteput( int cx1,int y1, int cx2,int y2, const void far *buf ) ;
;
; Parameters:
;	int cx1,cx2 : 0�`79
;	int y1,y2 : 0�`399
;
; Returns:
;	None
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
;	�N���b�s���O���ĂȂ��悤
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 2/ 5 Initial
;
	.186
	.MODEL SMALL
	include func.inc
	.CODE

func GRAPH_BYTEGET	; {
	push	BP
	mov	BP,SP

	; ����
	cx1	= (RETSIZE+6)*2
	y1	= (RETSIZE+5)*2
	cx2	= (RETSIZE+4)*2
	y2	= (RETSIZE+3)*2
	buf	= (RETSIZE+1)*2

	push	SI
	push	DI
	push	DS

	mov	SI,[BP+cx1]	; ���� cx1 > cx2 �Ȃ�A
	mov	CX,[BP+cx2]	;	cx1 <-> cx2
	cmp	SI,CX		;
	jle	short GET_SKIP1	;
	xchg	SI,CX		;
GET_SKIP1:			; SI = cx1
	sub	CX,SI		; CX = cx2 - SI + 1
	inc	CX		;

	mov	AX,[BP+y1]	; AX = y1
	mov	BX,[BP+y2]
	cmp	BX,AX
	jg	short GET_SKIP2
	xchg	AX,BX
GET_SKIP2:
	sub	BX,AX		; BX = y2 - y1 + 1
	inc	BX		;
	mov	DX,AX		; SI += y1 * 80
	shl	AX,2		;
	add	AX,DX		;
	shl	AX,4		;
	add	SI,AX		;

	mov	DX,0a800h
	les	DI,[BP+buf]

GET_LOOP:	; �v���[���̃��[�v
	push	SI
	mov	DS,DX

	; ES:DI�̐��K��
	mov	AX,DI
	and	DI,0fh
	shr	AX,1
	shr	AX,1
	shr	AX,1
	shr	AX,1
	mov	DX,ES
	add	DX,AX
	mov	ES,DX

	mov	AX,BX
	mov	DX,CX
GET_LOOP2:	; �c�̃��[�v
	shr	CX,1
	rep movsw
	adc	CX,CX
	rep movsb
	mov	CX,DX
	sub	SI,DX
	add	SI,80
	dec	AX
	jnz	short GET_LOOP2

	mov	DX,DS

	pop	SI
	add	DH,8
	or	DH,20h
	cmp	DH,0e8h
	jne	short GET_LOOP

	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	6*2
endfunc		; }

func GRAPH_BYTEPUT ; {
	push	BP
	mov	BP,SP

	; ����
	cx1	= (RETSIZE+6)*2
	y1	= (RETSIZE+5)*2
	cx2	= (RETSIZE+4)*2
	y2	= (RETSIZE+3)*2
	buf	= (RETSIZE+1)*2

	push	SI
	push	DI
	push	DS

	mov	DI,[BP+cx1]	; ���� cx1 > cx2 �Ȃ�A
	mov	CX,[BP+cx2]	;	cx1 <-> cx2
	cmp	DI,CX		;
	jle	short PUT_SKIP1	;
	xchg	DI,CX		;
PUT_SKIP1:
	sub	CX,DI		; DI = cx1
	inc	CX		; CX = cx2 - DI + 1

	mov	AX,[BP+y1]	;
	mov	BX,[BP+y2]	;
	cmp	AX,BX		;
	jle	short PUT_SKIP2	; ���� y1 > y2 �Ȃ�A
	xchg	AX,BX		;	y1 <-> y2
PUT_SKIP2:
	sub	BX,AX		; BX = y2 - y1 + 1
	inc	BX		;
	mov	DX,AX		; DI += y1 * 80
	shl	AX,2		;
	add	AX,DX		;
	shl	AX,4		;
	add	DI,AX		;

	mov	DX,0a800h
	lds	SI,[BP+buf]
PUT_LOOP:
	push	DI

	mov	ES,DX

	; DS:SI�̐��K��
	mov	AX,SI
	and	SI,0fh
	shr	AX,1
	shr	AX,1
	shr	AX,1
	shr	AX,1
	mov	DX,DS
	add	DX,AX
	mov	DS,DX

	mov	AX,BX
	mov	DX,CX
PUT_LOOP2:
	shr	CX,1
	rep movsw
	adc	CX,CX
	rep movsb
	mov	CX,DX
	sub	DI,DX
	add	DI,80
	dec	AX
	jne	short PUT_LOOP2

	mov	DX,ES
	pop	DI
	add	DH,8
	or	DH,20h
	cmp	DH,0e8h
	jne	short PUT_LOOP

	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	6*2
endfunc	; }

END
