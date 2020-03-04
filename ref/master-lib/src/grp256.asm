; master library - 9821
;
; Description:
;	9821A/Cの256色モードの取得・切り替え
;
; Function/Procedures:
;	int graph_is256color(void);
;	void graph_256color(void);
;	void graph_16color(void);
;
; Parameters:
;	none
;
; Returns:
;
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9821A/C
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	9821A/Cじゃないのに実行しても保証しないよーん
;
; Assembly Language Note:
;
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	steelman(千野  裕司)
;
; Revision History:
;	94/01/09 Initial: grp256.asm / master.lib 0.22

	.MODEL SMALL
	include func.inc

	.CODE
func GRAPH_IS256COLOR	; graph_is256color() {
	xor	AX,AX
	mov	ES,AX
	mov	AL,byte ptr ES:[054dh]
	and	AL,10000000b
	ret
endfunc			; }

func GRAPH_256COLOR	; graph_256color() {
	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[045ch],40h
	jz	short SET256_NOTMATE

	mov	AL,07h
	out	6ah,AL
	mov	AL,21h
	out	6ah,AL		;256
	mov	AL,06h
	out	6ah,AL

	or	byte ptr ES:[054dh],10000000b
SET256_NOTMATE:
	ret
endfunc			; }

func GRAPH_16COLOR	; graph_16color() {
	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[045ch],40h
	jz	short SET16_NOTMATE

	mov	AL,07h
	out	6ah,AL
	mov	AL,20h
	out	6ah,AL		;16
	mov	AL,06h
	out	6ah,AL

	and	byte ptr ES:[054dh],01111111b
SET16_NOTMATE:
	ret
endfunc			; }

END
