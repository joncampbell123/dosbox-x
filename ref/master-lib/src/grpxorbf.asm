; master library - grpahics - xor - boxfill - PC98V
;
; Description:
;	�O���t�B�b�N��ʂ� XOR �ɂ�锠�h��
;
; Function/Procedures:
;	void graph_xorboxfill( int x1, int y1, int x2, int y2, int color ) ;
;
; Parameters:
;	x1,y1	���㒸�_���W
;	x2,y2	�E�����_���W
;	color	��ʏ�̗̈�ɑ΂��� xor ����J���[�R�[�h
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-98V
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
;	���ˏ��F
;
; Revision History:
;	93/ 1/27 Initial: grpxorbf.asm
;	93/ 6/ 7 [M0.19] x���W�������}�C�i�X�������Ƃ���bug fix
;	93/ 9/ 1 [M0.21] ���W�͈͂̃}�W�b�N�i���o�[���[���ϐ��ɂ��ꂽ
;	93/10/23 [M0.21] graph_VramSeg,graph_VramLines���g�p

	.186
	.MODEL SMALL
	include func.inc
	.DATA
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramLines:WORD

	.CODE

XDOT	= 640
XBYTES	= (XDOT/8)

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	5*2
	EVEN
	endm

retfunc OWARI
	MRETURN
endfunc

; initial 90/6/7
func GRAPH_XORBOXFILL
	push	BP
	mov	BP,SP

	; ����
	x1 = (RETSIZE+5)*2
	y1 = (RETSIZE+4)*2
	x2 = (RETSIZE+3)*2
	y2 = (RETSIZE+2)*2
	color = (RETSIZE+1)*2

	push	SI
	push	DI

	;************************** view clipping
	mov	AX,[BP+x1]	; �܂��Ax���W�̃N���b�v**********
	mov	BX,[BP+x2]
	test	AX,BX
	js	short OWARI	; �����}�C�i�X�Ȃ�I���

	cmp	AX,BX
	jle	short Lskip1	; if ( AX > BX )
		xchg	AX,BX	;	swap AX,BX;
	Lskip1:
	cmp	AX,8000h	; if ( AX < 0 )
	sbb	DX,DX		;	AX = 0
	and	AX,DX

	sub	BX,(XDOT-1)		; if ( BX >= XDOT )
	sbb	DX,DX		;	BX = (XDOT-1) ;
	and	BX,DX
	add	BX,(XDOT-1)

	cmp	AX,BX		; if ( AX > BX )
	jg	short OWARI	;	goto OWARI;


	mov	CX,AX		; CX = x(L) & 15,
	and	CX,0fh		;
	mov	SI,AX		; SI = x(L) / 16 * 2,
	sar	SI,3		;
	and	SI,3feh		;
	sub	BX,AX		; DX = x(H)-x(L)+1.
	inc	BX		;
	mov	DX,BX		;

	mov	AX,[BP+y1]	; ���ɁAy���W�̃N���b�v************
	mov	BX,[BP+y2]
	cmp	AX,BX
	jle	short Lskip4	; if ( AX > BX )
	xchg	AX,BX		;	swap AX,BX;
Lskip4:
	test	AX,AX
	jns	short Lskip5	; if ( AX < 0 )
	xor	AX,AX		;	AX = 0;
Lskip5:
	mov	DI,graph_VramLines
	cmp	BX,DI
	jl	short Lskip6	; if ( BX >= YDOT )
	lea	BX,[DI-1]	;	BX = YDOT - 1 ;
Lskip6:
	cmp	AX,BX		; if ( AX > BX )
	jg	short OWARI	;	goto OWARI;

	push	CX
	push	DX
	mov	CX,XBYTES
	sub	BX,AX
	mul	CX
	add	SI,AX		; SI += (BX-AX)* XBYTES
	mov	AX,BX
	mul	CX
	mov	DI,AX		; DI = BX * XBYTES
	pop	DX
	pop	CX

;   ���݂̏��	    CL..���ē_�̃h�b�g�A�h���X
;		    SI..���ē_�̃��[�h�A�h���X
;		    DX..���ޓ_�܂ł̉��h�b�g��
;		    DI..���ޓ_�܂ł̏c�A�h���X��

;************************** draw boxfill ***************************

	mov	BX,[BP+color]
	mov	CH,BL
	mov	AX,graph_VramSeg
	push	DS
PLANE_START:
	shr	CH,1
	jnb	short Lplane_c
	push	SI
	push	DI
	mov	DS,AX
	mov	ES,CX
	mov	BP,DX

	mov	BX,8000h	; �ŏ��̃h�b�g����
	shr	BX,CL		;
	mov	AX,BX		;
	mov	CX,DX
Lloop2:				; �ŏ��̃��[�h����
	or	AX,BX		;
	dec	CX		;
	je	short Llast	;
	shr	BX,1		;
	jnb	short Lloop2	;
	mov	BX,DI
	xchg	AH,AL
Lloop3:				; �ŏ��̉��P���[�h���c�S��
	xor	[BX+SI],AX	;
	sub	BX,XBYTES	;
	jns	Lloop3		;
	inc	SI
	inc	SI
	cmp	CX,16		; �c��h�b�g����15�ȉ��Ȃ�
	jb	short Llastword	;	Llastword��
	mov	DX,CX		; DX = �c��h�b�g�� & 15
	and	DX,0fh		;
	sar	CX,4		; CX = �c��h�b�g�� / 16
	mov	AX,XBYTES	;
Lloop4:				; ���̉������[�h���c�S��
	mov	BX,DI		;      2
Lloop5:				;
	not	word ptr [BX+SI]; 16+8=24
	sub	BX,AX		;      3
	jns	short Lloop5	; 16/4 -> (27+16)*ys-12
	inc	SI		;      2
	inc	SI		;      2
	loop	short Lloop4	; 17/5 ->
	mov	CL,DL		; CL = DL( CH��loop�̌��޶� 0 )
Llastword:
	dec	CL		; �c��h�b�g�����O�Ȃ�
	js	short Lq	;	Lq��
	mov	AX,8000h	; �Ō�̃��[�h����
	sar	AX,CL		;
Llast:
	mov	BX,DI
	xchg	AH,AL
Lloop6:				; �Ō�̂P���[�h���c�S��
	xor	[BX+SI],AX	;
	sub	BX,XBYTES	;
	jns	short Lloop6	;

Lq:
	pop	DI
	pop	SI
	mov	DX,BP
	mov	CX,ES
	mov	AX,DS
Lplane_c:
	add	AH,8
	or	AH,20h
	cmp	AH,0e8h
	jb	short PLANE_START

	pop	DS
	MRETURN
endfunc

END
