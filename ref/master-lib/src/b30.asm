; master library - MS-DOS - 30BIOS
;
; Description:
;	30�sBIOS ( (c)lucifer ) ����
;	TT ( (C)���I ) �̑��݌���
;
; Procedures/Functions:
;	bios30_tt_exist();
;	bios30_push();
;	bios30_pop();
;
; Parameters:
;	
;
; Returns:
;	bios30_tt_exist:
;		00h = �s��
;		01h = ?
;		02h = TT(0.70�`0.80; 30bios�G�~�����[�V�����Ȃ�)������
;		03h = ?
;		06h = TT(1.50)������
;		80h = 30bios 0.20����
;		81h = 30bios 0.20�ȍ~
;		82h = TT(1.00)������
;		83h = TT(0.90�܂���1.05)������
;		89h = 30bios 1.20�ȍ~
;
;		���Ȃ킿�A
;		bit 1�� 1�Ȃ� TT�����݂���
;		2 �Ȃ炷�����Â�(��) TT �����݂���
;		80h�ȏ�Ȃ� 30bios API�����݂���
;		81h�ȏ�Ȃ�A30bios 0.20�ȍ~��API�����݂���
;		���Ă��ƂɂȂ�̂ł��[��
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios or TT)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	bios30_push, bios30_pop �́A30bios�̃o�[�W������ 0.07�����Â���
;			�����N����܂���B
;			TT�̃o�[�W������ 1.50���Â��Ƃ����������܂���B
;
;
; Assembly Language Note:
;	30bios_tt_exist �̏I���l�̖{���̈Ӗ��͉��̒ʂ�
;	  AL �̒l
;	    bit 0: 1=�����ȃ`�F�b�N�ʉ�
;	    bit 1: 1=TT API����
;	    bit 2: 1=TT 1.50�ȍ~
;	    bit 3: 1=30bios 1.20�ȍ~
;	    bit 7: 1=30bios API����
;	  z flag : AL = 0�̂Ƃ� 1
;	  c flag : (AL and 80h) = 0(=30bios API�Ȃ�) �̂Ƃ� 1
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;	����`��
;
; Advice & Check:
;	����������
;
; Revision History:
;	93/ 4/10 Initial: master.lib/b30.asm
;	93/ 4/25 bios30_setline/getline��b30line.asm�ɕ���
;	93/ 4/25 bios30_getversion��b30ver.asm�ɕ���
;	94/ 4/25 bios30_getmode��b30getmo.asm�ɕ���
;	93/ 8/30 [M0.21] TT 1.0�ɑΉ��c TT�̌Â��o�[�W�����ł͌듮�삷��?
;	93/ 9/13 [M0.21] bios30_exist�p�~, bios30_tt_exist�ǉ�
;	93/12/ 9 [M0.22] TT 1.50�ɑΉ�
;	94/ 8/ 8 30�sBIOS Ver1.20��27�i���݁j�ɑΉ�
;

	.MODEL SMALL
	include func.inc

	.DATA
check_string	db '30BIOS_EXIST='
check_byte	db '0'

	.CODE

func BIOS30_TT_EXIST	; bios30_tt_exist(void) {
	push	DI

CHECK_30BIOS:
	mov	check_byte,'0'
	mov	AH,0bh
	mov	BX,'30'+'�s'
	push	DS
	pop	ES
	mov	DI,offset check_string
	int	18h
	shl	AL,1		; bit 6 -> bit 7
	and	AL,80h
	jz	short NO_BIOS30	; 30�sBIOS��API������Ȃ�o�[�W�������擾
	mov	dl,al
	mov	ax,0ff00h
	int	18h
	xchg	ah,al
	cmp	ax,(1 SHL 8) OR 20
	cmc
	sbb	al,al
	and	al,08h		; bit 3 30BIOS 1.20�ȍ~
	or	al,dl
NO_BIOS30:
	or	check_byte,AL

CHECK_TT:
	mov	BX,'TT'		; TT 0.70�` Install Check
	mov	AX,1800h
	int	18h
	sub	AX,BX		; if exist: AX=0
	cmp	AX,1
	sbb	AX,AX
	jz	short NO_TT

	mov	BX,'TT'		; TT������̂ŁA 1.50�ȍ~���ǂ�������
	mov	AX,1810h
	int	18h
	shl	AH,1		; 1.50�ȍ~��MSB�������Ă���
	sbb	AX,AX
	and	AX,4		; ���ꂪ 0 �łȂ���� 1.50�ȍ~����
	or	AL,2		; ������ɂ��� TT API ����
	or	check_byte,AL

NO_TT:
	mov	AL,check_byte
	and	AL,8fh	; 0�Ȃ� zf=1
	rol	AL,1
	ror	AL,1
	cmc		; bit 7=0�Ȃ� cy=1

	pop	DI
	ret
endfunc			; }

func BIOS30_PUSH	; bios30_push(void) {
	call	BIOS30_TT_EXIST
	and	AL,84h	; 30bios API, TT 1.50 api
	jz	short PUSH_FAILURE
	mov	AX,0ff01h	; 30bios API push
	js	short PUSH_30BIOS
	mov	BX,'TT'
	mov	AX,180ah	; TT 1.50 API push
PUSH_30BIOS:
	int	18h
	and	AL,0fh	; -1, 1�`15�������B0,16�̓G���[
	cmp	AL,1
	sbb	AX,AX
	inc	AX	; success=1  failure=0
PUSH_FAILURE:
	ret
endfunc			; }

func BIOS30_POP		; bios30_pop(void) {
	call	BIOS30_TT_EXIST
	and	AL,84h	; 30bios API, TT 1.50 api
	jz	short POP_FAILURE
	mov	AX,0ff02h	; 30bios API pop
	js	short POP_30BIOS
	mov	BX,'TT'
	mov	AX,180bh	; TT 1.50 API pop
POP_30BIOS:
	int	18h	; 30bios: AX; -1=succes, 0=failure
			; TT1.50: AL; 15�`0=success, -1=failure
	and	AH,AL
	add	AH,1
	sbb	AH,AH	; ����� 30bios success= 0ffxxh, else 00xxh
	not	AL	;        TT1.50: failure=0000h
	cmp	AX,1
	sbb	AX,AX
	inc	AX	; success=1  failure=0
POP_FAILURE:
	ret
endfunc			; }

END
