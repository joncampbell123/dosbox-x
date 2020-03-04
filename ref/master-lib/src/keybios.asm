; master library - PC-9801
;
; Description:
;	PC-9801の KEY BIOS を用いて、キーバッファの先頭の値を取り出す
;
; Function/Procedures:
;	unsigned key_wait_bios(void) ;
;	unsigned key_sense_bios(void) ;
;
; Parameters:
;	none
;
; Returns:
;	上位8bit:キーコード
;	下位8bit:キーデータ(キャラクタコード)
;
;	key_wait_bios:	キー入力が無ければ押されるまで待ち、バッファから
;			取り除く。
;	key_sense_bios:	キー入力が無ければ、0を返す。キー入力がある場合も
;			バッファの更新は行わない。
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
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
;	93/ 8/17 Initial: keybios.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc

	.CODE

func KEY_WAIT_BIOS	; key_wait_bios() {
	mov	AH,0
	int	18h
	ret
endfunc		; }

func KEY_SENSE_BIOS	; key_sense_bios() {
	mov	AH,1
	int	18h
	shr	BH,1
	sbb	BX,BX
	and	AX,BX
	ret
endfunc		; }

END
