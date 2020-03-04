; master library - MS-DOS
;
; Description:
;	ディスクの残りバイト数の読み出し
;
; Function/Procedures:
;	long dos_getdiskfree( int drive ) ;
;
; Parameters:
;	int drive	0=Cur, 1=A:, 2=B:..
;			'a'や'A'も可
;
; Returns:
;	-1 = エラー
;	0～ = 残りサイズ
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
;	恋塚昭彦
;
; Revision History:
;	92/11/17 Initial
;	93/1/17  bugfix
;	95/ 2/11 [M0.22k] ハードエラー割り込みをフック
;	95/ 2/12 [M0.22k] 'a'や'A'も認識
;	95/ 3/11 [M0.22k] int 2Fh AX=4A00h CX=0 もフックして交換を無視させる

	.MODEL SMALL
	include func.inc

	.CODE
	EXTRN	DOS_SETVECT:CALLMODEL

harderr_func:
	mov	AL,3
	iret

org_change	dd	0

skip_change_func:
	cmp	AX,4a00h
	jne	short int2f_resume
	cmp	CX,0
	jne	short int2f_resume
	not	CX		; CX = 0ffffh
	iret
int2f_resume:
	jmp	CS:org_change


func DOS_GETDISKFREE	; dos_getdiskfree() {
	mov	BX,SP
	; 引数
	drive	= RETSIZE*2

	mov	AX,24h
	push	AX
	push	CS
	mov	AX,offset harderr_func
	push	AX
	call	DOS_SETVECT
	push	DX		; 復元用
	push	AX		; 復元用

	push	BX
	push	CX
	mov	AX,2Fh
	push	AX
	push	CS
	mov	AX,offset skip_change_func
	push	AX
	call	DOS_SETVECT
	mov	word ptr org_change,AX
	mov	word ptr org_change+2,DX
	pop	CX
	pop	BX

	mov	DL,SS:[BX+drive]
	and	DL,1fh
	mov	AH,36h
	int	21h

	cmp	AX,-1
	je	short ERROR

	mul	BX
	mov	BX,AX		;
	mov	AX,DX		; kDX kAX = hi(iAX * iBX) * CX
	mul	CX		; AX = lo(iAX * iBX)
	xchg	AX,BX		;
	mul	CX		; DX AX = lo(iAX * iBX) * CX
	add	DX,BX		; DX += kAX

DONE:
	pop	BX		; ofs
	pop	CX		; seg

	push	AX		; save
	push	DX		; save

	push	BX
	push	CX
	mov	AX,2Fh
	push	AX
	push	word ptr org_change+2
	push	word ptr org_change
	call	DOS_SETVECT
	pop	CX
	pop	BX

	mov	AX,24h
	push	AX
	push	CX		; seg
	push	BX		; ofs
	call	DOS_SETVECT
	pop	DX		; restore
	pop	AX		; restore
	ret	2
	EVEN
ERROR:
	mov	DX,AX
	jmp	short DONE
endfunc			; }

END
