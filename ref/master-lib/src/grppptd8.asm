; master library - graphic - packedpixel - put - hline - 8dot - PC98V
;
; Description:
;	�p�b�N�g4bit�s�N�Z���`���̋�`�O���t�B�b�N�p�^�[���\��
;
; Function/Procedures:
;	void graph_pack_put_down_8( int x, int y, void far * pat, int patwidth, int width_, int height ) ;
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
;	93/12/19 Initial: grppptd8.asm/master.lib 0.22

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	SCREEN_XBYTE equ 80
	SCREEN_XDOT equ 640

USE_GRCG = 0	; 1�ɂ����GRCG���g���������x���Ȃ�(RA21+VMM386)
USE_TABLE = 1

IFDEF ??version		; tasm check
	JUMPS
	WARN
ENDIF

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
	ret	7*2
	EVEN
endm

retfunc CLIPOUT
	MRETURN
endfunc

num_yloop dw ?

func GRAPH_PACK_PUT_DOWN_8	; graph_pack_put_down_8() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	x	= (RETSIZE+7)*2
	y	= (RETSIZE+6)*2
	pat	= (RETSIZE+4)*2
	patwidth = (RETSIZE+3)*2
	width_	= (RETSIZE+2)*2
	height	= (RETSIZE+1)*2

	mov	AX,[BP+y]
	sub	AX,ClipYT
	cmp	AX,ClipYH
	ja	short CLIPOUT

	mov	BX,[BP+height]
	add	BX,AX
	jc	short CLIPOUT
	cmp	BX,ClipYH
	ja	short CLIPOUT
	sub	BX,AX
	mov	num_yloop,BX

	mov	CX,[BP+width_]
	sar	CX,3		; 8dot�P�ʂɐ؂�̂Ă�
	jle	short CLIPOUT

	mov	SI,CX
	shl	SI,3
	sub	SI,[BP+patwidth]
	neg	SI
	shr	SI,1
	mov	CS:si_add,SI
	mov	SI,[BP+pat]

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

	mov	AX,SCREEN_XBYTE
	sub	AX,CX
	mov	CS:di_add,AX

	mov	CS:num_xloop,CX

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
	mov	DS,[BP+pat+2]

	CLD

	; DS,SI�𐳋K��
YLOOP_ADJUST:
	mov	AX,DS
	mov	BX,SI
	shr	BX,4
	add	AX,BX
	mov	DS,AX
	and	SI,0fh

YLOOP:
	JMOV	BP,num_xloop
	EVEN
XLOOP:

IF USE_TABLE
	mov	CL,2

	mov	BL,[SI]
	mov	BH,0
	shl	BX,CL
	mov	AX,word ptr CS:RotTbl[BX]
	mov	DX,word ptr CS:RotTbl[BX+2]
	IRP	addval,<1,2,3>
	shl	AX,CL
	shl	DX,CL
	mov	BL,[SI+addval]
	mov	BH,0
	shl	BX,CL
	or	AX,word ptr CS:RotTbl[BX]
	or	DX,word ptr CS:RotTbl[BX+2]
	ENDM
	add	SI,4
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
	add	DI,1234h
	org $-2
di_add	dw	?
	add	SI,1234h
	org $-2
si_add	dw	?
	dec	num_yloop
	jg	YLOOP_ADJUST	; (;_;)
YLOOP_END:

	pop	DS

IF USE_GRCG
	mov	AL,0
	out	GC_MODEREG,AL
ENDIF
	MRETURN
endfunc		; }

END
