; master library - MSDOS
;
; Description:
;	メインメモリの確保
;
; Function/Procedures:
;	unsigned dos_allocate( unsigned para ) ;
;
; Parameters:
;	unsigned para	確保するパラグラフサイズ
;
; Returns:
;	unsigned 確保されたDOSメモリブロックのセグメント
;		 0 ならメモリが足りないので失敗でやんす
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	DOS: 2.0 or later
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
;	93/ 1/31 Initial dosalloc.asm

	.MODEL SMALL
	include func.inc
	.CODE

func DOS_ALLOCATE
	mov	BX,SP
	mov	BX,SS:[BX+RETSIZE*2]	; para
	mov	AH,48h		; 確保する
	int	21h
	cmc
	sbb	CX,CX
	and	AX,CX		; CX=seg, 失敗なら 0
	ret	2
endfunc

END
