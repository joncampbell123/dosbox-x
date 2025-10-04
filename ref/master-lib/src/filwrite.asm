; master library - MS-DOS
;
; Description:
;	�t�@�C���ւ̏�������
;
; Function/Procedures:
;	int file_write( const char far * buf, unsigned wsize ) ;
;
; Parameters:
;	char far * buf	�������ރf�[�^�̐擪�A�h���X
;	unsigned wsize	�������ރo�C�g��(0�͋֎~)
;
; Returns:
;	1 = ����
;	0 = ���s
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
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
;	92/11/17 Initial
;	92/11/30 BufferSize = 0 �Ȃ�� DOS���ڂ�
;	92/12/08 ����bugfix
;	94/ 2/10 [0.22a] bugfix, �o�b�t�@����̂Ƃ��ɏ������݃|�C���^��1����
;			�i�܂Ȃ�����
;	94/ 3/12 [M0.23] bugfix, �o�b�t�@�T�C�Y���傫�ȏ������݂������Ƃ���
;			�Ō�̗]����o�b�t�@�ɏ������񂾂��ƃG���[��Ԃ��Ă���

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_Buffer:DWORD
	EXTRN file_BufferSize:WORD
	EXTRN file_BufferPos:WORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD
	EXTRN file_ErrorStat:WORD
	EXTRN file_Handle:WORD

	.CODE

func FILE_WRITE
	push	BP
	mov	BP,SP

	push	SI
	push	DI

	cmp	file_BufferSize,0
	je	short DIRECT

	; ����
	buf	= (RETSIZE+2)*2
	wsize	= (RETSIZE+1)*2

	mov	BX,[BP+wsize]		; BX = wsize
	mov	SI,[BP+buf]		; SI = buf
WLOOP:
	mov	CX,file_BufferSize
	sub	CX,file_BufPtr
	sub	CX,BX
	sbb	AX,AX			; AX : file�`���̗p����Ă��-1
	and	CX,AX			;      wsize���̗p����Ă�� 0
	add	CX,BX			; CX = writelen

	les	DI,file_Buffer
	add	DI,file_BufPtr
	sub	BX,CX			; wsize -= writelen
	add	file_BufPtr,CX		; BufPtr += writelen

	push	DS
	mov	DS,[BP+buf+2]		; �J�[�N���G���^�[�v���C�Y��
	shr	CX,1			; �͂��A�`���[���[�ł�
	rep	movsw			; �]��!
	adc	CX,CX
	rep	movsb			; buf += writelen
	pop	DS

	or	AX,AX
	jns	short LOOPEND

	; file�`���̗p����Ă����A���Ȃ킿�o�b�t�@�����t�ɂȂ���

	push	DS
	push	BX
	mov	CX,file_BufferSize
	mov	BX,file_Handle
	lds	DX,file_Buffer
	mov	AH,40h			; �t�@�C���̏�������
	int	21h
	pop	BX
	pop	DS
	jc	short WERROR
	cmp	file_BufferSize,AX
	jne	short WERROR

	mov	file_BufPtr,0		; �������񂾂̂Ńo�b�t�@�N���A
	add	WORD PTR file_BufferPos,AX
	adc	WORD PTR file_BufferPos+2,0

LOOPEND:
	or	BX,BX
	jne	short WLOOP

	; Success
	mov	AX,1
	jmp	short DONE
	EVEN

DIRECT:	; DOS����
	push	DS
	mov	CX,[BP+wsize]
	mov	BX,file_Handle
	lds	DX,[BP+buf]
	mov	AH,40h			; �t�@�C���̏�������
	int	21h
	pop	DS
	jnc	short EXIT

WERROR:
	mov	file_ErrorStat,1
	xor	AX,AX
EXIT:
	add	WORD PTR file_BufferPos,AX
	adc	WORD PTR file_BufferPos+2,0
	add	AX,-1
	sbb	AX,AX	; 0�ȊO�Ȃ�1�ɂ���
DONE:
	pop	DI
	pop	SI

	mov	SP,BP
	pop	BP
	ret	6
	EVEN
endfunc

END
