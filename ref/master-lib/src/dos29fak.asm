; master library - MSDOS - int29h
;
; Description:
;	int29hがないDOSのための、エミュレーション設定
;
; Function/Procedures:
;	void dos29fake_start(void) ;
;	void dos29fake_end(void) ;
;
; Parameters:
;	none
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
;	MS-DOS: v2～
;
; Notes:
;	　int29hが存在しないときだけ組み込みます。
;	その判定にはCONデバイスのSPECLビットを見ています。
;	組み込んだ場合、ファイルハンドルをひとつ占有します。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 3/30 Initial

	.MODEL SMALL
	include func.inc
	EXTRN DOS_SETVECT:CALLMODEL

	.DATA

con	db	'CON',0

	.DATA?
last29	dd	?

	.CODE

handle	dw	1

int29 proc far
	push	DS
	push	AX
	push	BX
	push	CX
	push	DX
	mov	BX,SP
	lea	DX,[BX+6]
	mov	AX,SS
	mov	DS,AX
	mov	BX,CS:handle
	mov	AH,40h		; write handle
	mov	CX,1
	int	21h
	pop	DX
	pop	CX
	pop	BX
	pop	AX
	pop	DS
	iret
int29 endp


func DOS29FAKE_START	; dos92fake_start() {
	cmp	CS:handle,1
	jne	short	IGNORE	; house keeping

	mov	DX,offset con
	mov	AX,3d01h
	int	21h			; open handle for write
	jc	short	IGNORE	; failure...

	mov	BX,AX
	mov	AX,4400h		; read device information
	int	21h
	and	DX,0092h		; ISDEV,SPECL,ISCOT
	cmp	DX,0092h
	je	short END_FAKE

	mov	CS:handle,BX

	mov	AX,29h
	push	AX
	push	CS
	mov	AX,offset int29
	push	AX
	call	DOS_SETVECT
	mov	word ptr last29,AX
	mov	word ptr last29+2,DX
IGNORE:
	ret
endfunc			; }

func DOS29FAKE_END	; dos29fake_end() {
	mov	AX,1
	xchg	AX,CS:handle
	cmp	AX,1
	je	short	IGNORE	; house keeping

	mov	BX,AX

	mov	AX,29h
	push	AX
	push	word ptr last29+2
	push	word ptr last29
	call	DOS_SETVECT

END_FAKE:
	mov	AH,3eh
	int	21h			; close handle
	ret
endfunc			; }

END
