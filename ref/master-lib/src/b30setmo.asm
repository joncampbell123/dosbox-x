; master library - MS-DOS - 30BIOS
;
; Description:
;	30�sBIOS ( (c)lucifer ) ���쐧��
;
; Function/Procedures:
;	void bios30_setmode( int mode );
;		mode ... BIOS30_NORMAL, BIOS30_SPECIAL, BIOS30_RATIONAL, BIOS30_VGA
;			 BIOS30_FKEY, BIOS30_CW
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
;	MS-DOS (with 30bios or TT)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	30bios/TT API�����݂���Γ��삵�܂��B���݂��Ȃ���΂Ȃɂ����܂���B
;	30�sBIOS��Rational���[�h���T�|�[�g�̏ꍇ�͐ݒ肪�����ɂȂ�B
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
;	93/ 9/13 Initial: b30setmo.asm/master.lib 0.21
;	93/12/ 9 [M0.22] TT API�Ή�
;	94/ 8/ 8 30�sBIOS Ver1.20��27�i���݁j�ɑΉ�

	.MODEL SMALL
	include func.inc
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
;		mode ... {BIOS30_NORMAL,BIOS30_SPECIAL,BIOS30_RATIONAL,BIOS30_VGA}
;			|{BIOS30_FKEY, BIOS30_CW}
;			|{BIOS30_WIDELINE, BIOS30_NORMALLINE}
; ��ʃo�C�g��modify mask�Ł[��
; �������A0�̂Ƃ��͉��ʃo�C�g�𒼐ڐݒ肵�܂���B
; 30bios�����݂��Ȃ���Ή������܂���B
func BIOS30_SETMODE	; void bios30_setmode( int mode ) {
	_call	BIOS30_TT_EXIST
	and	AL,84h			; 30bios API, TT 1.50 API
	mov	BX,SP
	mode = (RETSIZE+0)*2
	mov	AX,SS:[BX+mode]
	jz	short SET_FAILURE
	jns	short SET_TT

	mov	BX,'30'+'�s'
	not	AH
	test	AH,AH
	je	short SET_NOMODIFY
	mov	CX,AX
	mov	AH,0bh	; getmode
	int	18h
	and	AL,CH
	or	AL,CL
SET_NOMODIFY:
	mov	AH,0ah
TT_RETURN:
	int	18h
SET_FAILURE:
	ret	2

;		30bios   �� 1.20			TT
;		0..0.... 00.0.... normal		.00.0...
;		0..1.... 00.1.... special (View)	.01.1...
;		1..1.... 11.1.... vga     (All)		.10.1...
;		........ 01.1.... rational(Layer)	.00.1...
;
;		..0..... ..0..... function key		........
;		..1..... ..1..... CW			........
;
;		.......0 .......0 normalline		.......0
;		.......1 .......1 wideline		.......1

;
; �ȉ��̎菇�́Abit7(vga)�����𗎂Ƃ����Ƃ���(->special)�A8000h���n����Ă�
; normal mode�ɐݒ肵�Ă��܂���肪����B
; normal mode�̂Ƃ��Ɏ��s�����̂Ȃ炢���̂����B�܂��A���̂Ƃ��̓����
; 30bios�ł͒�`����ĂȂ��̂ŁA9010h�Őݒ肵�Ă����Ȃ��ƍ��邩�炢�����B
;
SET_TT:
	mov	dx,ax		; dx ���[�h
	and	dx,0101h

	and	ax,0d0d0h	; ��ʏ�Ԃ�TT�p�ɂ���
	jz	short TT_NO_MODE
	shr	ax,1
	xor	al,20h
	or	dx,ax
;	test	dl,40h		; | ��`����Ă���l���O��
;	jz	TT_NO_ALL	; | �����ȉ�ʏ�Ԃ��ݒ肳���\��������Ȃ�
;	and	dl,NOT 20h	; | �����̃R�����g���͂���
TT_NO_ALL:
TT_NO_MODE:
	mov	bx,'TT'		; ���[�h�ݒ�
	mov	ax,1802h
	int	18h
	not	dh
	and	al,dh
	or	al,dl
	jmp	short TT_RETURN
endfunc			; }

END
