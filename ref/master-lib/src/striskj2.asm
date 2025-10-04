; master library - 
;
; Description:
;	文字列の中の特定位置が漢字2バイト目かどうか調べる
;
; Function/Procedures:
;	int str_iskanji2( const char * kanjistr, int n ) ;
;
; Parameters:
;	kanjistr	文字列の先頭
;	n		判定する位置
;
; Returns:
;	1 ... kanjistr[n]の文字は漢字2バイト目である   (zflag=0)
;	0 ... kanjistr[n]の文字は漢字2バイト目ではない (zflag=1)
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
; Assembly Language Note:
;	破壊レジスタは BXのみ
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/12/22 Initial: striskj2.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.CODE

func STR_ISKANJI2	; str_iskanji2() {
	push	SI			; 結果:AX   破壊レジスタ: BXのみ
	mov	SI,SP
	kanjistr= (RETSIZE+2)*2
	n	= (RETSIZE+1)*2
	mov	AL,0
	mov	BX,SS:[SI+n]
	dec	BX
	js	short NO
	_push	DS
	_lds	SI,SS:[SI+kanjistr]
	mov	AX,BX
SEARCHLOOP:
	test	byte ptr [BX+SI],0e0h
	jns	short BREAK	; 大ざっぱな漢字検査
	jp	short BREAK	; 80..9f, e0..ff
	dec	BX
	jns	short SEARCHLOOP
BREAK:	sub	AX,BX
	_pop	DS
NO:	pop	SI
	and	AX,1			; yesならAX=1,zf=0.  noならAX=0,zf=1
	ret	(DATASIZE+1)*2
endfunc			; }

END
