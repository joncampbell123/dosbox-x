; master library - DOS - file - size
;
; Description:
;	ファイルの大きさを得る
;
; Function/Procedures:
;	long dos_filesize( int fh ) ;
;
; Parameters:
;	fh  ファイルハンドル
;
; Returns:
;	InvalidHandle   (cy=1) ハンドルが無効
;	0～             (cy=0) ファイルの大きさ
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
; Assembly Language Note:
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
;	94/ 2/14 Initial: dosfsize.asm/master.lib 0.22a
;	95/ 1/21 [M0.23] BUGFIX

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_FILESIZE	; dos_filesize() {
	mov	BX,SP
	filehandle = (RETSIZE+0)*2
	mov	BX,SS:[BX+filehandle]

	mov	AX,4201h	; 現在位置を得る
	xor	CX,CX
	mov	DX,CX
	int	21h
	jc	short FAULT

	push	SI
	push	DI

	push	AX		; 現在位置をpush
	push	DX
	xor	DX,DX
	mov	AX,4202h	; 末尾へ (CX,DXは既に 0)
	int	21h
	mov	SI,AX		; 末尾位置を DI,SIに保存
	mov	DI,DX

	pop	CX
	pop	DX
	mov	AX,4200h	; 元の位置へ戻る
	int	21h

	mov	AX,SI
	mov	DX,DI
	pop	DI
	pop	SI
	ret	2
FAULT:
	neg	AX		; InvalidHandle
	sbb	DX,DX		; 必ず-1, cy=1
	ret	2
endfunc		; }

END
