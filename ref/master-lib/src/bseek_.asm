; master library - (pf.lib)
;
; Description:
;	ファイルポインタの移動
;
; Functions/Procedures:
;	int bseek_(bf_t bf,long offset,int whence);
;
; Parameters:
;	bf	bファイルハンドル
;	offset	相対量
;	whence	基準位置
;
; Returns:
;	0   成功
;	-1  失敗
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 186
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
;	iR
;	恋塚昭彦
;
; Revision History:
; BSEEK_.ASM         497 94-06-05   20:17
;	95/ 1/10 Initial: bseek_.asm/master.lib 0.23
;	95/ 4/20 [M0.23] BUGFIX whence=1にとりあえず対応

	.model SMALL
	include func.inc
	include pf.inc

	.CODE

func BSEEK_	; bseek_() {
	push	BP
	mov	BP,SP

	;arg	bf:word,ofst:dword,whence:word
	bf	= (RETSIZE+4)*2
	ofst	= (RETSIZE+2)*2
	whence	= (RETSIZE+1)*2

	mov	ES,[BP+bf]		; BFILE構造体のセグメント
	mov	BX,0
	xchg	ES:[b_left],BX

	mov	AL,[BP+whence]
	mov	DX,[BP+ofst]
	mov	CX,[BP+ofst+2]
	cmp	AL,1
	jne	short ABSOLUTE_ADR
	sub	DX,BX
	sbb	CX,0
ABSOLUTE_ADR:
	mov	BX,ES:[b_hdl]
	msdos	MoveFilePointer

	sbb	AX,AX			; AX←carry
	pop	BP
	ret	(4)*2
endfunc		; }

END
