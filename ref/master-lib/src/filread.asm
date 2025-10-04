; master library - MS-DOS
;
; Description:
;	�t�@�C������̓ǂݍ���
;
; Function/Procedures:
;	unsigned file_read( char far * buf, unsigned bsize ) ;
;
; Parameters:
;	char far * buf	�ǂݍ��ݐ�( NULL�Ȃ�΁Absize���ǂݎ̂Ă� )
;	unsigned bsize	�ő�ǂݎ�蒷(�o�C�g��,
;			0 �Ȃ�� file_Buffer��K�v�Ȃ�΍X�V���邾�� )
;
; Returns:
;	�ǂݍ��񂾃o�C�g��
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
;	file_EOF�́A1�o�C�g���ǂ߂Ȃ������������ݒ肳���B
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
;	92/11/30 BufferSize = 0�Ȃ� DOS���ڂ�
;	92/12/06 ���Ŕ��������A[indirect:���񂺂񓮂���][direct:retcode�ُ�]
;		 ���C���B

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_Buffer:DWORD
	EXTRN file_BufferSize:WORD
	EXTRN file_BufferPos:DWORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD
	EXTRN file_ErrorStat:WORD
	EXTRN file_Handle:WORD

	.CODE

func FILE_READ
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	buf	= (RETSIZE+2)*2
	bsize	= (RETSIZE+1)*2

	cmp	file_BufferSize,0
	je	short DIRECT

	mov	BX,[BP+bsize]		; BX = rest
	les	DI,[BP+buf]
RLOOP:
	mov	AX,file_InReadBuf
	cmp	file_BufPtr,AX
	jb	short MADA_ARU

	add	WORD PTR file_BufferPos,AX
	adc	WORD PTR file_BufferPos+2,0

	push	BX
	push	DS
	mov	CX,file_BufferSize
	mov	BX,file_Handle
	lds	DX,file_Buffer
	mov	AH,3Fh			; �t�@�C���̓ǂݍ���
	int	21h
	pop	DS
	pop	BX
	cmc				; �G���[�Ȃ� AX = 0 -> EOF
	sbb	DX,DX
	and	AX,DX
	mov	file_InReadBuf,AX
	jz	short EOF

	mov	file_BufPtr,0		; �ǂ߂�
MADA_ARU:
	mov	SI,file_InReadBuf	; SI = len
	sub	SI,file_BufPtr
	sub	SI,BX
	sbb	AX,AX
	and	SI,AX
	add	SI,BX
	mov	AX,ES
	or	AX,DI
	je	short LOOPEND
	or	SI,SI
	je	short LOOPEND

	push	SI			; �]��
	push	DS
	mov	CX,SI
	mov	AX,file_BufPtr
	lds	SI,file_Buffer
	add	SI,AX
	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb
	pop	DS
	pop	SI
LOOPEND:
	add	file_BufPtr,SI
	sub	BX,SI
	jne	short RLOOP
	jmp	short EXITLOOP
	EVEN
	; �o�b�t�@�T�C�Y��0�Ȃ�DOS���ڃA�N�Z�X
DIRECT:
	push	DS
	mov	CX,[BP+bsize]		; BX = rest

	mov	BX,file_Handle
	lds	DX,[BP+buf]
	mov	AH,3Fh			; �t�@�C���̓ǂݍ���
	int	21h
	pop	DS
	add	WORD PTR file_BufferPos,AX
	adc	WORD PTR file_BufferPos+2,0
	mov	BX,CX
	sub	BX,AX
	je	short EXITLOOP
EOF:
	mov	file_Eof,1
EXITLOOP:
	mov	AX,[BP+bsize]
	sub	AX,BX

	pop	DI
	pop	SI
	pop	BP
	ret	6
	EVEN
endfunc

END
