; master library - vtext
;
; Description:
;	画面最下行のシステムラインのON/OFF
;
; Functions/Procedures:
;	void vtext_systemline_show()
;	void vtext_systemline_hide()
;	int vtext_systemline_shown()
;
; Parameters:
;	none
;
; Returns:
;	vtext_systemline_shown: 0=非表示, 1=表示
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	IBM DOS/V
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
;	94/ 4/13 Initial: vtsyslin.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	EXTRN FEP_EXIST:CALLMODEL
	EXTRN FEP_SHOW:CALLMODEL
	EXTRN FEP_SHOWN:CALLMODEL
	EXTRN FEP_HIDE:CALLMODEL

	.CODE
	EXTRN VTEXT_START:CALLMODEL

FEPTYPE_UNKNOWN equ 0
FEPTYPE_IAS	equ 1
FEPTYPE_MSKANJI	equ 2

func VTEXT_SYSTEMLINE_SHOWN	; vtext_systemline_shown() {
	call	FEP_EXIST
	cmp	AX,FEPTYPE_IAS
	jne	short SHOWN2
SHOWN_IAS:
	mov	AX,1402h
	int	16h
	cmp	AL,1		; AL=0なら1   AL=0以外なら0
	sbb	AX,AX
	neg	AX
	ret
SHOWN2:
	call	FEP_SHOWN
	ret
endfunc		; }

func VTEXT_SYSTEMLINE_SHOW	; vtext_systemline_show() {
	call	FEP_EXIST
	cmp	AX,FEPTYPE_IAS
	je	short IASSHOW
	call	FEP_SHOW
	jmp	short RETURN
IASSHOW:
	mov	AX,1400h
IASSET:
	int	16h
RETURN:
	_call	VTEXT_START		; 関連変数を読み取り直す
	ret
endfunc		; }

func VTEXT_SYSTEMLINE_HIDE	; vtext_systemline_hide() {
	call	FEP_EXIST
	cmp	AX,FEPTYPE_IAS
	jne	short HIDE2
	mov	AX,1401h
	jmp	short IASSET
HIDE2:
	call	FEP_HIDE
	jmp	short RETURN
endfunc		; }

END
