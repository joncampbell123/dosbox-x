; master library - MS-DOS - 30BIOS
;
; Description:
;	�R�O�sBIOS ( (c)lucifer ) ����
;	���[�h�擾
;
; Procedures/Functions:
;	unsigned bios30_getmode();
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
;	MS-DOS (with 30bios)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	30bios API�����݂���΁A��ɓ��삵�܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;	����`��
;
; Advice:
;	KEI SAKAKI.
;
; Revision History:
;	93/ 4/10 Initial: master.lib/b30.asm
;	93/ 4/25 bios30_setline/getline��b30line.asm�ɕ���
;	93/ 4/25 bios30_getversion��b30ver.asm�ɕ���
;	93/ 4/25 Initial: master.lib 0.16/b30getmo.asm
;	93/ 9/13 [M0.21] 30bios_exist -> 30bios_tt_exist
;	93/12/ 9 [M0.22] TT API�Ή�
;	94/ 8/ 8 30�sBIOS Ver1.20��27�i���݁j�Ή�
;

	.MODEL SMALL
	include func.inc
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETMODE	; unsigned bios30_getmode(void) {
	_call	BIOS30_TT_EXIST		; 30bios API, TT 1.50 API
	mov	dx,ax			; dl ���݌��ʂ̒l, dh 0
	and	al,84h
	jz	short GET_FAILURE
	jns	short GET_TT

	mov	ah,0bh			; al CRT���[�h
	mov	bx,'30'+'�s'
	int	18h
	mov	ah,NOT 0c0h		; �m�[�}�����[�h
	test	al,10h			; al��bit6,7���N���A
	jz	short BIOS30_RETURN

	mov	ah,NOT 40h		; �g�����[�h
	test	dl,08h			; al��bit6���N���A
	jz	short BIOS30_RETURN

	mov	dh,al			; 30�sBIOS 1.20�ȍ~
	and	dh,NOT 0c0h		; dh bit6,7���N���A����CRT���[�h
	xor	bl,bl			; al �r�b�g����t�����
	mov	ax,0ff09h
	int	18h
	mov	ah,0c0h			; al�̉�ʏ�ԈȊO���N���A
BIOS30_RETURN:
	and	al,ah
	or	al,dh
TT_RETURN:
	mov	ah,0ffh
GET_FAILURE:
	ret

;		30bios   �� 1.20			TT
;		x..0.... xx.0.... normal		.xx.0...
;		0..1.... 00.1.... special (View)	.01.1...
;		1..1.... 11.1.... vga     (All)		.1x.1...
;		........ 01.1.... rational(Layer)	.00.1...
;
;		..0..... ..0..... function key		........
;		..1..... ..1..... CW			........
;
;		.......0 .......0 normalline		.......0
;		.......1 .......1 wideline		.......1

GET_TT:
	mov	BX,'TT'		; Get ExtMode
	mov	AX,1802h
	int	18h
	mov	BL,AL
	and	AL,1

	and	bl,68h		; ��ʏ�Ԃ�30�sBIOS�݊��ɂ���
;	jz	short TT_RETURN	; ��������normal �� �����ŋA��Ȃ��Ă����Ȃ�
	add	bl,bl
	or	al,bl
	xor	al,40h
	test	al,10h
	jz	short TT_NORMAL
	test	al,80h
	jz	short TT_RETURN
	and	al,NOT 40h
	jmp	short TT_RETURN

TT_NORMAL:
	and	al,NOT 0c0h
	jmp	short TT_RETURN
endfunc			; }

END
