; superimpose & master library module
;
; Description:
;	RGB�t�@�C���̓��e�� Palettes�ɓǂݍ���
;
; Functions/Procedures:
;	int palette_entry_rgb( const char * filename ) ;
;
; Parameters:
;	char * filename ;	RGB�t�@�C����
;
; Returns:
;	0 = ����
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
;	�n�[�h�E�F�A�p���b�g�ɂ͔��f���܂���B
;	���f����ɂ́A���̊֐����s�� palette_show�܂���palette_show100��
;	�Ăяo���ĉ������B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;
;$Id: rgbload.asm 0.04 93/02/19 20:10:00 Kazumi Rel $
;
;	93/ 3/ 9 Initial: master.lib <- super.lib 0.22b
;	93/ 9/15 [M0.21] bugfix �������ĂȂ�����(���܂���(^^;)
;	93/12/10 [M0.22] palette�� 4bit->8bit�ύX�ɑΉ�

	.186
	.MODEL SMALL
	include func.inc
	include	super.inc
	EXTRN	DOS_ROPEN:CALLMODEL

	.DATA
	EXTRN	Palettes:WORD

	.CODE

func PALETTE_ENTRY_RGB	; palette_entry_rgb() {
	mov	BX,SP

	file_name = (RETSIZE+0)*2

	_push	SS:[BX+file_name+2]
	push	SS:[BX+file_name]
	call	DOS_ROPEN
	jc	short RETURN

	mov	BX,AX			; file handle
	mov	DX,offset Palettes
	mov	CX,48
	mov	AH,03fh			; read handle
	int	21h
	sbb	CX,CX

	push	BX
	mov	BX,48-1
SHIFTLOOP:
	mov	AL,byte ptr Palettes[BX]
	shl	AL,4
	or	byte ptr Palettes[BX],AL
	dec	BX
	jns	short SHIFTLOOP
	pop	BX

	mov	AH,03eh			; close handle
	int	21h
	mov	AX,NoError
	jcxz	short RETURN
	mov	AX,InvalidData
	stc
RETURN:
	ret	DATASIZE*2
endfunc				; }

END
