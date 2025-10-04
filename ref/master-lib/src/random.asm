; master library - random
;
; Description:
;	簡易乱数
;
; Function/Procedures:
;	function IRand : Integer ;
;
; Parameters:
;	none
;
; Returns:
;	IRand: 0～32767の乱数
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
;	Borland C++ 3.1のランタイムライブラリを参考にしています。
;
;	一回の処理時間は、PC-9801RA21(Cx486DRx2 20/40MHz)で 2.1μsecです。
;	ちなみに、同一条件で Visial C++ 1.0Jの rand()は 3.8μsec,
;	                     Borland C++3.1          は 2.9μsecですので、
;	標準ライブラリのものよりは高速ということになります。
;	処理的には、BC3.1のものを高速化しただけにすぎませんので、
;	完全に代用できます。
;
; Assembly Language Note:
;	CX,DXレジスタとフラグを破壊します。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	94/ 1/ 6 Initial: random.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	random_seed:DWORD

	.CODE

	HI_MUL	equ 015ah
	LO_MUL	equ 4e35h


func IRAND	; IRand() {
	mov	AX,LO_MUL
	mul	word ptr random_seed+2
	mov	CX,AX
	mov	AX,HI_MUL
	mul	word ptr random_seed
	add	CX,AX
	mov	AX,LO_MUL
	mul	word ptr random_seed
	add	AX,1
	adc	DX,CX
	mov	word ptr random_seed,AX
	mov	AX,DX
	mov	word ptr random_seed+2,AX
	and	AH,7fh
	ret
endfunc		; }

END
