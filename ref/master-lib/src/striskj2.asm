; master library - 
;
; Description:
;	������̒��̓���ʒu������2�o�C�g�ڂ��ǂ������ׂ�
;
; Function/Procedures:
;	int str_iskanji2( const char * kanjistr, int n ) ;
;
; Parameters:
;	kanjistr	������̐擪
;	n		���肷��ʒu
;
; Returns:
;	1 ... kanjistr[n]�̕����͊���2�o�C�g�ڂł���   (zflag=0)
;	0 ... kanjistr[n]�̕����͊���2�o�C�g�ڂł͂Ȃ� (zflag=1)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
;
; Assembly Language Note:
;	�j�󃌃W�X�^�� BX�̂�
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/12/22 Initial: striskj2.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.CODE

func STR_ISKANJI2	; str_iskanji2() {
	push	SI			; ����:AX   �j�󃌃W�X�^: BX�̂�
	mov	SI,SP
	kanjistr= (RETSIZE+2)*2
	n	= (RETSIZE+1)*2
	mov	AL,0
	mov	BX,SS:[SI+n]
	dec	BX
	js	short NO
	_push	DS
	_lds	SI,SS:[SI+kanjistr]
	mov	AX,BX
SEARCHLOOP:
	test	byte ptr [BX+SI],0e0h
	jns	short BREAK	; �傴���ςȊ�������
	jp	short BREAK	; 80..9f, e0..ff
	dec	BX
	jns	short SEARCHLOOP
BREAK:	sub	AX,BX
	_pop	DS
NO:	pop	SI
	and	AX,1			; yes�Ȃ�AX=1,zf=0.  no�Ȃ�AX=0,zf=1
	ret	(DATASIZE+1)*2
endfunc			; }

END
