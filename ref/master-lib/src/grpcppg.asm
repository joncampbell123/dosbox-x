; master library - PC-9801
;
; Description:
;	�O���t�B�b�N��ʂ̓��e�𑼂̃y�[�W���畡�ʂ���
;
; Function/Procedures:
; 	int pascal graph_copy_page( int to_page ) ;
;
; Parameters:
;	to_page		�]����( 0 or 1 )
;
; Returns:
;	int	0(FALSE)	���s(�������s��)
;		1(TRUE)		����
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
;	���s��A�A�N�Z�X�y�[�W�� to_page �ɂȂ�܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/ 2/ 4 Initial
;	93/ 3/21 smem_wget�g�p
;	93/ 3/29 bugfix
;	95/ 3/20 [M0.22k] ���S�̂��ߊJ�n����grcg_off����ꂽ

	.186
	.MODEL SMALL
	include func.inc
	EXTRN SMEM_WGET:CALLMODEL
	EXTRN SMEM_RELEASE:CALLMODEL

APAGE equ 0a6h

	.DATA
	EXTRN graph_VramWords:WORD

	.CODE

; copy a plane
; in:
;	AL = vram page�̔���
;	BX = from seg
;	DX = to seg
;	CX = word number of vram size
;	dflag = 0
; break:
;	AL �͔��΂ɂȂ�܂�( 0<->1 )
;	BX <-> DX
;	SI,DI,DS,ES
copy_plane proc near
	xor	AL,1
	out	APAGE,AL
	mov	DS,BX
	mov	ES,DX
	xor	DI,DI
	mov	SI,DI
	rep	movsw
	mov	CX,DI
	shr	CX,1
	xchg	BX,DX
	ret
copy_plane endp

;
func GRAPH_COPY_PAGE	; graph_copy_page() {
	xor	DX,DX
	mov	CX,graph_VramWords
	mov	BX,CX
	shl	BX,1
	push	BX
	call	SMEM_WGET
	xchg	AX,DX
	jc	short ERROR

	xor	AL,AL		; GRCG OFF
	out	7ch,AL

	; ����
	G_TO_PAGE = RETSIZE * 2
	mov	BX,SP
	mov	AX,SS:[BX+G_TO_PAGE]
	and	AL,1

	push	SI
	push	DI
	push	DS

	mov	BX,0a800H
	call	copy_plane	; from
	call	copy_plane	; to

	mov	BX,0b000H
	call	copy_plane	; from
	call	copy_plane	; to

	mov	BX,0b800H
	call	copy_plane	; from
	call	copy_plane	; to

	mov	BX,0e000H
	call	copy_plane	; from
	call	copy_plane	; to

OWARI:
	pop	DS
	pop	DI
	pop	SI

	push	DX
	call	SMEM_RELEASE
	mov	AX,1

ERROR:
	ret	2
endfunc				; }

END
