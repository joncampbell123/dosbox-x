; master library - DOS
;
; Description:
;	起動パス名を得る
;
; Function/Procedures:
;	void dos_get_argv0( char * argv0 ) ;
;
; Parameters:
;	argv0	得たパス名を格納する先
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS 3.1 or later ( 2.1でも動くけど常に 0 文字の文字列になる )
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
;	Kazumi
;	恋塚昭彦
;
; Revision History:
;	93/ 7/16 Initial: dosargv0.asm/master.lib 0.20
;	93/ 7/21 [M0.20] small:bugfix

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_GET_ARGV0	; dos_get_argv0() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	; 引数
	buf	= (RETSIZE+1)*2

	mov	AH,62h		; get psp
	int	21h

	mov	ES,BX
	mov	ES,ES:[2ch]
	mov	DI,0

	mov	CX,8000h	; 環境変数領域は 32Kまで
SEARCH:
	mov	AL,0
	repne	scasb		; 探しまくり
	cmp	AL,ES:[DI]	; 0
	jne	short SEARCH	; 末尾を探す

	cmp	word ptr ES:[DI+1],1	; 比較してるけど条件分岐はあっち→

	lea	SI,[DI+1+2]
    s_ <push	DS>
	push	ES
	pop	DS

	_les	DI,[BP+buf]
    s_ <pop	ES>

	jne	short NAIJAN		; → DOSのばーぢょんが違うんでないの
					; ALは 0 だからね

	EVEN
COPY:	lodsb				; ここんとこ低速あるね
NAIJAN:	stosb
	or	AL,AL
	jnz	short COPY

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	DATASIZE*2
endfunc			; }

END
