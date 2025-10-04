; master library - PC-9801
;
; Description:
;	�O���t�B�b�N��ʂ��n�[�h�E�F�A��X�N���[������
;
; Function/Procedures:
; 	void graph_scrollup( unsigned line ) ;
;
; Parameters:
;	line	��ɂ��炷�h�b�g��( 0 �` ��ʃ��C����-1 )
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
;	CPU: 8086
;	GDC
;
; Notes:
;	line���͈͊O���ƁAline=0�Ƃ��ĐU�镑���܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 2/ 4 Initial: grpscrol.asm
;	93/ 5/11 [M0.16] GDC 5MHz�̎��Ɉُ킾����(^^; bugfix
;	93/ 7/19 [M0.20] ����ǂ�GDC 2.5MHz�̂Ƃ��Ɂcsz�
;	93/ 7/19 Initial: grpscrlu.asm/master.lib 0.20
;	94/ 1/22 [M0.22] graph_VramZoom�Ή�

	.MODEL SMALL
	include func.inc

	.DATA
	extrn graph_VramLines:WORD
	extrn graph_VramZoom:WORD

	.CODE

	EXTRN	gdc_outpw:NEAR

GDCSTATUS	equ 0a0h
GDCCMD		equ 0a2h
GDCPARAM	equ 0a0h

func GRAPH_SCROLLUP	; graph_scrollup() {
	push	BP
	mov	BP,SP
	; ����
	line	= (RETSIZE+1) * 2

	mov	BX,[BP+line]
	mov	DX,graph_VramLines

	sub	BX,DX
	sbb	AX,AX
	and	BX,AX
	add	BX,DX	; BX = min( BX, DX )
	sub	DX,BX	; DX = graph_VramLines - BX
	mov	BP,BX

	mov	CX,graph_VramZoom
	shl	BX,CL
	shl	DX,CL

	mov	CL,4
WAITEMPTY:
	jmp	$+2
	in	AL,GDCSTATUS
	test	AL,CL	; 4
	jz	short WAITEMPTY

	mov	AL,70h
	out	GDCCMD,AL

	; adr1
	mov	AX,BP
	shl	AX,1
	shl	AX,1
	add	AX,BP		; AX = BP*5
	shl	AX,1
	shl	AX,1
	shl	AX,1		; *8    AX = BP*5*8 = BP*40
	call	gdc_outpw

	; line1
	mov	AX,DX
	shl	AX,CL	; CL = 4
	or	AH,CH
	call	gdc_outpw

	; adr2
	xor	AX,AX
	call	gdc_outpw

	; line2
	mov	AX,BX
	shl	AX,CL	; CL = 4
	or	AH,CH
	call	gdc_outpw

	pop	BP
	ret	2
endfunc			; }

END
