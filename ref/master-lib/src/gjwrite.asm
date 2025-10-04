; master library - PC98
;
; Description:
;	�O���̓o�^
;
; Function/Procedures:
;	void gaiji_write( unsigned short chr, const void far * pattern ) ;
;	void gaiji_write_all( const void far * patterns ) ;
;
; Parameters:
;	unsigned chr		�O���R�[�h( 0�`255 )
;	void far * pattern 	�o�^����p�^�[�� ( ���A�E�̏���1byte����
;						�@16���C��������(�v32bytes))
;	void far * patterns	�p�^�[�����A������ 256�Ȃ��
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
;	DS:SI = read pointer
; BREAK:
;	AX,CX,BX
;	SI = next address
SETFONTW proc near
	out	0a1h,AL		; �����R�[�h����
	mov	AL,AH
	out	0a3h,AL		; �����R�[�h���

	mov	CX,16
	mov	BX,0
GLOOP:	mov	AL,BL		; �L�����N�^�q�n�l�֏�������
	or	AL,20h
	out	0a5h,AL		; ������
	lodsw
	out	0a9h,AL
	mov	AL,BL
	out	0a5h,AL		; �E����
	mov	AL,AH
	out	0a9h,AL
	inc	BX
	loop	short GLOOP
	ret
SETFONTW endp


func GAIJI_WRITE
	push	DS
	push	SI
	mov	DX,SP
	CLI
	add	SP,(RETSIZE+2)*2
	pop	SI	; pattern
	pop	DS	; FP_SEG(pattern)
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
	call	SETFONTW
	mov	AL,0ah		; �b�f�R�[�h�A�N�Z�X�ɂ����
	out	68h,AL

	pop	SI
	pop	DS
	ret	6
endfunc

func GAIJI_WRITE_ALL
	push	DS
	push	SI

	; ����
	patterns = (RETSIZE+2)*2

	mov	SI,SP
	lds	SI,SS:[SI+patterns]

	mov	AL,0bh		; �b�f�h�b�g�A�N�Z�X�ɂ����
	out	68h,AL

	mov	DX,0
WLOOP:
	mov	AX,DX		; �O���R�[�h����
	adc	AX,5680h
	and	AL,7fh
	call	SETFONTW
	inc	DL
	jnz	short WLOOP

	mov	AL,0ah		; �b�f�R�[�h�A�N�Z�X�ɂ����
	out	68h,AL

	pop	SI
	pop	DS
	ret	4
endfunc

END
