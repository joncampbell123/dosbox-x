; master library - graphic - packedpixel - put - hline - 8dot - PC98V
;
; Description:
;	�p�b�N�g4bit�s�N�Z���`���̐����O���t�B�b�N�p�^�[���\��
;
; Function/Procedures:
;	void graph_pack_put_8( int x, int y, void far * linepat, int len ) ;
;
; Parameters:
;	x	�`��J�n x ���W( 0�`639, ������8�h�b�g�P�ʂɐ؂�̂� )
;	y	�`�� y ���W( 0 �` 399(��400���C���\���̂Ƃ�)
;	linepat	�p�^�[���f�[�^�̐擪�A�h���X
;	len	�p�^�[���f�[�^�̉��h�b�g��
;		���������ۂɂ�8dot�P�ʂɐ؂�l�߂܂��B
;
; Returns:
;	none
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
;	�N���b�s���O�͏c�������� gc_poly �����̏������s���Ă��܂��B
;	���͉�ʕ��ŃN���b�s���O�ł��܂��B
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
;	93/ 6/23 Initial: grppput8.asm/master.lib 0.19
;	93/ 7/ 3 [M0.20] large model�ł�bugfix
;	93/ 7/ 3 [M0.20] �N���b�v�g�ŉ��s���O�Ɣ��肵�Ă���(^^; fix
;	93/ 7/28 [M0.20] ������Ɖ���
;	93/11/22 [M0.21] linepat��far�Œ艻
;	93/12/ 8 [M0.22] RotTbl�� rottbl.asm�ɕ���

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	SCREEN_XBYTE equ 80

USE_GRCG = 0	; 1�ɂ����GRCG���g���������x���Ȃ�(RA21+VMM386)
USE_TABLE = 1


	.CODE

if USE_TABLE
	EXTRN RotTbl:DWORD
endif

ROTATE	macro	wreg,lh,ll
	rept	2
	rol	wreg,1
	RCL	DH,1
	rol	wreg,1
	RCL	DL,1
	rol	wreg,1
	RCL	lh,1
	rol	wreg,1
	RCL	ll,1
	endm
endm
GC_MODEREG equ 7ch
GC_TILEREG equ 7eh


MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	5*2
	EVEN
endm

retfunc CLIPOUT
	MRETURN
endfunc

func GRAPH_PACK_PUT_8	; graph_pack_put_8() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	x	= (RETSIZE+5)*2
	y	= (RETSIZE+4)*2
	linepat	= (RETSIZE+2)*2
	len	= (RETSIZE+1)*2

	mov	AX,[BP+y]
	sub	AX,ClipYT
	cmp	AX,ClipYH
	ja	short CLIPOUT

	mov	CX,[BP+len]
	sar	CX,3		; 8dot�P�ʂɐ؂�̂Ă�
	jle	short CLIPOUT

	mov	SI,[BP+linepat]

	mov	DI,[BP+x]
	sar	DI,3		; x��8�h�b�g�P�ʂɕ␳
	jns	short XNORMAL
	add	CX,DI
	jle	short CLIPOUT
	shl	DI,2
	add	SI,DI
	xor	DI,DI
XNORMAL:
	cmp	DI,SCREEN_XBYTE
	jge	short CLIPOUT

	add	CX,DI
	cmp	CX,SCREEN_XBYTE
	jl	short RIGHTCLIPPED
	mov	CX,SCREEN_XBYTE
RIGHTCLIPPED:
	sub	CX,DI
	imul	AX,AX,SCREEN_XBYTE
	add	DI,AX		; DI = draw address

IF USE_GRCG
	pushf
	CLI
	mov	AL,80h		; GC_TDW
	out	GC_MODEREG,AL
	popf
ENDIF

	; �`��J�n
	push	DS
	mov	ES,ClipYT_seg
	mov	DS,[BP+linepat+2]

	mov	BP,CX
	CLD
	EVEN
XLOOP:

IF USE_TABLE
	mov	CL,2

	mov	BL,[SI]
	mov	BH,0
	shl	BX,CL
	mov	AX,word ptr CS:RotTbl[BX]
	mov	DX,word ptr CS:RotTbl[BX+2]
	inc	SI
	REPT 3
	shl	AX,CL
	shl	DX,CL
	mov	BL,[SI]
	mov	BH,0
	shl	BX,CL
	or	AX,word ptr CS:RotTbl[BX]
	or	DX,word ptr CS:RotTbl[BX+2]
	inc	SI
	ENDM
ELSE
	mov	CX,2
LOOP8:
	lodsw
	ROTATE	AL,BH,BL
	ROTATE	AH,BH,BL
	loop	short LOOP8
	mov	AX,BX
ENDIF

IF USE_GRCG
	out	GC_TILEREG,AL
	mov	AL,AH
	out	GC_TILEREG,AL
	mov	AX,DX
	out	GC_TILEREG,AL
	mov	AL,AH
	out	GC_TILEREG,AL
	stosb
ELSE
	mov	ES:[DI],AL		; 0a800h
	mov	BX,ES
	mov	ES:[DI+8000h],AH	; 0b000h
	add	BH,10h
	mov	ES,BX
	mov	ES:[DI],DL		; 0b800h
	add	BH,28h
	mov	ES,BX
	mov	ES:[DI],DH		; 0e000h
	sub	BH,38h
	mov	ES,BX
	inc	DI
ENDIF

	dec	BP
	jnz	short XLOOP

	pop	DS

IF USE_GRCG
	mov	AL,0
	out	GC_MODEREG,AL
ENDIF
	MRETURN
endfunc		; }

END
