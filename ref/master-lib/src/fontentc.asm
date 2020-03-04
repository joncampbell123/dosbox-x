; master library - font - cgrom - vsync
;
; Description:
;	VSYNC割り込みを使用して半角フォントを読み込む
;
; Function/Procedures:
;	int font_entry_cgrom( int firstchar, int lastchar ) ;
;
; Parameters:
;	firstchar	読み込む最初のキャラクタコード(0～255)
;	lastchar	読み込む最後のキャラクタコード(0～255)
;
; Returns:
;	NoError			(cy=0)	成功
;	InsufficientMemory	(cy=1)	メモリ不足
;	InvalidData		(cy=1)	サイズが8x16dotじゃない
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801 Normal Mode
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	実行前に vsync_start()を実行する必要があります。
;	終了後プログラム終了前に vsync_end()を実行する必要があります。
;
;	firstchar > lastchar の場合、何もしません。
;	font_ReadChar に firstcharが、font_ReadEndChar に lastcharが
;	そのまま転写されて起動します。
;	font_ReadCharは、1文字読み込む毎に 1増えます。
;
;	この読み込み処理が完了したかどうかを判定するには、
;	グローバル変数 font_ReadChar が font_ReadEndChar より大きいかどうか
;	(大きければ完了)で判断します。
;
;	　実行前に vsync_proc_set されたものは復元されないので、終了を判断
;	した後、再度 vsync_proc_set して復旧してください。
;
;	　vsync１回に1文字だけ読み込むので、256文字読むとかなり時間が
;	かかります。できるだけ、必要最小限の範囲を指定するようにして下さい。
;
;	　また、プログラム起動一回めのみこの処理を実行してファイルに保存し、
;	2回目以降は font_read_bfnt を用いるのが合理的です。
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
;	93/11/20 Initial: fontentc.asm/master.lib 0.21
;	94/ 1/26 [M0.22] font_AnkSizeチェック
;	95/ 2/14 [M0.22k] mem_AllocID対応
;	95/ 3/30 [M0.22k] font_AnkParaも設定

	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN	HMEM_ALLOC:CALLMODEL

	.DATA
	EXTRN	font_AnkSeg:WORD		; font.asm
	EXTRN	font_AnkSize:WORD		; font.asm
	EXTRN	font_AnkPara:WORD		; font.asm
	EXTRN	font_ReadChar:WORD		; fontcg.asm
	EXTRN	font_ReadEndChar:WORD		; fontcg.asm
	EXTRN	vsync_Proc : DWORD		; vs.asm
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE
font_read_proc proc far
	mov	AX,font_ReadChar
	cmp	AX,font_ReadEndChar
	jg	short READ_RET
	mov	BX,font_AnkSeg
	add	BX,AX
	mov	ES,BX
	xor	BX,BX
	mov	CX,AX

	mov	AL,0bh
	out	68h,AL		; CG ROM dot access

	jmp	$+2
	mov	AL,0
	out	0a1h,AL
	jmp	$+2
	jmp	$+2
	mov	AL,CL
	out	0a3h,AL		; set character code

READLOOP:
	mov	AL,BL
	out	0a5h,AL
	jmp	$+2
	jmp	$+2
	in	AL,0a9h
	mov	ES:[BX],AL
	inc	BL
	cmp	BL,16
	jl	short READLOOP

	mov	AL,0ah
	out	68h,AL		; CG ROM code access

	inc	font_ReadChar
READ_RET:
	retf
font_read_proc endp

func FONT_ENTRY_CGROM ; font_entry_cgrom() {
	cmp	font_AnkSize,0
	je	short GO
	cmp	font_AnkSize,0110h
	mov	AX,InvalidData
	stc
	jne	short IGNORE
GO:

	push	BP
	mov	BP,SP
	; 引数
	firstchar = (RETSIZE+2)*2
	lastchar = (RETSIZE+1)*2

	mov	AX,[BP+firstchar]
	mov	BX,[BP+lastchar]
	mov	font_readchar,AX
	mov	font_readendchar,BX

	mov	DX,font_AnkSeg
	test	DX,DX
	jnz	short CONT
	; font_AnkSegが設定されてないとき
	mov	mem_AllocID,MEMID_font
	mov	AX,256		; 256char * 16 bytes
	push	AX
	call	HMEM_ALLOC
	jc	short DAME
	mov	font_AnkSeg,AX
	mov	font_AnkPara,1
	mov	font_AnkSize,0110h
	mov	DX,AX
DAME:
	mov	AX,InsufficientMemory
	jc	short ENOMEM
CONT:
	pushf
	CLI
	mov	WORD PTR vsync_Proc,offset font_read_proc
	mov	WORD PTR vsync_Proc + 2,CS
	popf

	xor	AX,AX	; NoError
ENOMEM:
	pop	BP
IGNORE:
	ret	4
endfunc		; }

END
