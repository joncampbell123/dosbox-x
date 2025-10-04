; master library - EMS - GETSEGMENT
;
; Description:
;	�����y�[�W�̃Z�O�����g�A�h���X�̔z��̎擾
;
; Function/Procedures:
;	int ems_getsegment( unsigned * segments, int maxframe )
;
; Parameters:
;	segments	�����y�[�W�Z�O�����g�̔z��̊i�[��
;	maxframe	�i�[�\�ȍő吔
;
; Returns:
;	0		�G���[
;	1�`		�i�[�ł����Z�O�����g�̐�
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	EMS: LIM EMS 4.0
;
; Notes:
;	���ʁAsegments�̔z��Y�����������y�[�W�ԍ��A���̓��e���Z�O�����g
;	�A�h���X�ɂȂ�܂��B
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
;	93/ 7/19 Initial: emsgetsg.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_GETSEGMENT	; ems_getsegment() {
	push	SI
	push	DI
	push	BP
	mov	BP,SP

	; ����
	segments = (RETSIZE+4)*2
	maxframe = (RETSIZE+3)*2

	mov	AX,5801h	; �G���g�����擾
	int	67h
	test	AH,AH
	mov	AX,0
	jne	short ERROR_EXIT

	push	SS
	pop	ES
	mov	AX,CX
	shl	AX,1
	shl	AX,1
	sub	SP,AX
	mov	DI,SP	; ES:DI= �i�[��
	mov	AX,5800h
	int	67h
	test	AH,AH
	mov	AX,0
	jne	short ERROR_EXIT

	mov	SI,DI
    s_ <push	DS>
    s_ <pop	ES>
	_les	DI,[BP+segments]

	mov	AX,[BP+maxframe]
	cmp	AX,CX
	jl	short TOBI	; cx�͎��ۂ̌�
	mov	AX,CX		; ax�� min( �w�萔, ���ۂ̌� )
TOBI:
	test	AX,AX
	jz	short ERROR_EXIT ; 0�Ȃ炨��肾�[���̂��

	xor	DX,DX
NLOOP:
	mov	CH,CL
	xor	BX,BX
ILOOP:
	cmp	SS:[SI+BX+2],DX
	je	short FOUND
	add	BX,4
	dec	CH
	jge	short ILOOP
	xor	BX,BX		; ������Ȃ��Ƃ��͍ŏ��̃G���g���ő�p(��)!
FOUND:
	mov	BX,SS:[SI+BX]
	mov	ES:[DI],BX
	inc	DI
	inc	DI

	inc	DX
	cmp	DX,AX
	jb	short NLOOP

ERROR_EXIT:
	mov	SP,BP
	pop	BP
	pop	DI
	pop	SI
	ret	(1+DATASIZE)*2
endfunc		; }

END
