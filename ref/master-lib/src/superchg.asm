; superimpose & master library module
;
; Description:
;	消去パターンを変更する
;
; Functions/Procedures:
;	void super_change_erase_pat(int patnum, const void far * image_addr) ;
;
; Parameters:
;	int patnum		パターン番号
;	void far *image_addr	新しい消去パターンデータの先頭アドレス
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
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
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: superchg.asm 0.01 92/05/29 20:23:59 Kazumi Rel $
;
;	93/ 3/10 Initial: master.lib <- super.lib 0.22b
;       95/ 4/ 1 [M0.22k] 未登録のパターンを指定したときに無視するようにした
;

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD      ; superpa.asm
	EXTRN	super_patdata:WORD      ; superpa.asm

	.CODE

func SUPER_CHANGE_ERASE_PAT	; super_change_erase_pat() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	patnum		= (RETSIZE+3)*2
	image_addr	= (RETSIZE+1)*2

	mov	BX,[BP+patnum]
	shl	BX,1
	mov	AX,super_patsize[BX]
	mul	AH
	jz      short IGNORE
	mov	CX,AX
	mov	ES,super_patdata[BX]
	xor	DI,DI
	lds	SI,[BP+image_addr]
	CLD
	shr	CX,1
	rep	movsw
	adc	CX,0
	rep	movsb
IGNORE:
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc				; }

END
