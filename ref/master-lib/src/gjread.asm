; master library - PC98
;
; Description:
;	�O���̓ǂ݂���
;
; Function/Procedures:
;	void gaiji_read( unsigned short chr, void far * pattern ) ;
;	void gaiji_read_all( void far * patterns ) ;
;
; Parameters:
;	unsigned chr		�O���R�[�h( 0�`255 )
;	void far * pattern 	�p�^�[���i�[�� ( ���A�E�̏���1byte����
;						�@16���C��������(�v32bytes))
;	void far * patterns	�p�^�[�����A������ 256�������܂��
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
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
;	92/11/24 Initial

	.MODEL SMALL
	include func.inc

	.CODE

;	lo byte = ��, high byte = �E����
; IN:
;	AX = JIS - 2000h
;	ES:DI = read pointer
; BREAK:
;	AX,CX,BX
;	DI = next address
GETFONTW proc near
	out	0a1h,AL		; JIS�R�[�h����
	mov	AL,AH
	out	0a3h,AL		; JIS�R�[�h���

	mov	CX,16
	mov	BX,0
GLOOP:	mov	AL,BL		; �L�����N�^�q�n�l����ǂݍ���
	out	0a5h,AL		; �E����
	in	AL,0a9h
	mov	AH,AL
	mov	AL,BL
	or	AL,20h
	out	0a5h,AL		; ������
	in	AL,0a9h
	stosw
	inc	BX
	loop	short GLOOP
	ret
GETFONTW endp

func GAIJI_READ
	push	DI
	mov	DX,SP
	CLI
	add	SP,(RETSIZE+1)*2
	pop	DI	; pattern
	pop	ES	; FP_SEG(readto)
	pop	AX	; chr
	mov	SP,DX
	STI

	mov	AH,0
	add	AX,5680h	; �O���R�[�h����
	and	AL,7fh

	push	AX
	mov	AL,0bh		; �b�f�h�b�g�A�N�Z�X�ɂ����
	out	68h,AL
	pop	AX
	call	GETFONTW
	mov	AL,0ah		; �b�f�R�[�h�A�N�Z�X�ɂ����
	out	68h,AL

	pop	DI
	ret	6
endfunc

func GAIJI_READ_ALL
	push	DI

	; ����
	patterns = (RETSIZE+1)*2
	mov	DI,SP
	les	DI,SS:[DI+patterns]

	mov	AL,0bh		; �b�f�h�b�g�A�N�Z�X�ɂ����
	out	68h,AL

	mov	DX,0
RLOOP:
	mov	AX,DX		; �O���R�[�h����
	adc	AX,5680h
	and	AL,7fh
	call	GETFONTW
	inc	DL
	jnz	short RLOOP

	mov	AL,0ah		; �b�f�R�[�h�A�N�Z�X�ɂ����
	out	68h,AL

	pop	DI
	ret	4
endfunc

END
