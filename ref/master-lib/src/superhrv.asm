; master library
;
; Description:
;	�p�^�[���f�[�^�����E���]����
;
; Functions/Procedures:
;	void super_hrev( int patnum )
;
; Parameters:
;	patnum	���E���]�������p�^�[���ԍ�
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	80186/V30
;
; Requiring Resources:
;	CPU: 86186/V30
;
; Notes:
;	�E�o�^����Ă��Ȃ��p�^�[�����肾�Ɖ������܂���B
;	�E�����`���ɕϊ����ꂽ�p�^�[�����肾�ƁA�p�^�[����������܂��B
;	  �������A������x���s����ƌ��ɂ��ǂ�܂�(��)
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
;	94/ 7/27 Initial: superhrv.asm/master.lib 0.23

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN table_hreverse:BYTE
	EXTRN super_patsize:WORD
	EXTRN super_patdata:WORD
	EXTRN super_patnum:WORD

	.CODE

func SUPER_HREV	; super_hrev() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ������
	patnum = (RETSIZE+1)*2

	mov	BX,[BP+patnum]
	cmp	BX,super_patnum
	jae	short IGNORE			; foolproof
	shl	BX,1
	mov	DX,super_patsize[BX]
	test	DX,DX
	jz	short IGNORE			; foolproof

	mov	ES,super_patdata[BX]

	mov	CL,DL
	mov	CH,0
	mov	AX,CX
	shl	CX,2
	add	CX,AX		; CX = ylen * 5
	mov	DL,DH
	mov	DH,0		; DX = xlen

	mov	BX,offset table_hreverse
	mov	BP,0	; pattern offset

YLOOP:
	mov	SI,BP
	add	BP,DX		; BP = nextline
	lea	DI,[BP-1]	; DI = SI(lx) + wid - 1

XLOOP:
	mov	AL,ES:[SI]
	xlatb
	xchg	ES:[DI],AL
	xlatb
	mov	ES:[SI],AL

	inc	SI
	dec	DI
	cmp	SI,DI
	jb	short XLOOP
	jne	short NEXTLINE
	mov	AL,ES:[SI]
	xlatb
	mov	ES:[SI],AL
NEXTLINE:
	loop	short YLOOP

IGNORE:
	pop	DI
	pop	SI
	pop	BP
	ret	2
endfunc		; }

END
