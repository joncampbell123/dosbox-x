PAGE 98,120
;tab 8
; master library - Pi Format Comment Read
;
; Description:
;	Pi Format FileのCommentを読み込みます。
;
; Function/Procedures:
;	int MASTER_RET graph_pi_comment_load(char *filename,PiHeader *header);
;
; Parameters:
;	const char *filename	ファイルネーム
;	PiHeader	*header		情報格納用引数
;
;	typedef struct PiHeader PiHeader ;
;	struct PiHeader {
;	   unsigned char	far *comment; // ここが変更される。
;	   unsigned int	commentlen; // ここが必要。
;	   unsigned char	mode;
;	   unsigned char	n;
;	   unsigned char	m;
;	   unsigned char	plane;
;	   unsigned char	machine[4];
;	   unsigned int	maexlen;
;	   unsigned char	far *maex;
;	   unsigned int	xsize;
;	   unsigned int	ysize;
;	   unsigned char	palette[48];
;	} ;
;
; Returns:
;	NoError		(cy=0) 成功, またはコメントがない
;	FileNotFound	(cy=1) ファイルがない
;	InsufficientMemory (cy=1) メモリが足りない
;	InvalidData	(cy=1) サイズが PiHeaderより小さい, PIファイルじゃない
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
; Compiler/Assembler:
;	TASM 2.51
;
; Note:
;	誤ったデータやVA用データを読ませないこと、暴走する可能性があります。
;	あらかじめ構造体にひつような情報を格納しておく必要があります。
;
; Author:
;	恋塚, SuCa
;
; Rivision History:
; 	93/11/09 initial
;	93/11/21 Initial: grppicom.asm/master.lib 0.21
;	95/ 2/14 [M0.22k] mem_AllocID対応

	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN	DOS_ROPEN:CALLMODEL
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL

	.DATA
	EXTRN mem_AllocID:WORD		; mem.asm

	EOF EQU 26

IFDEF ??version
	JUMPS
	WARN
ENDIF

.CODE
PiHeader STRUC
comment_offset	dw	?
comment_segment	dw	?
commentlen	dw	?
_mode		db	?
_n		db	?
_m		db	?
_plane		db	?
_machine	db	4 dup (?)
maexlen		dw	?
maex_offset	dw	?
maex_segment	dw	?
_xsize		dw	?
_ysize		dw	?
_palette	db	48 dup (?)
PiHeader ENDS



func GRAPH_PI_COMMENT_LOAD	; graph_pi_comment_load() {
	push	BP
	mov	BP,SP
	push	DI
	filename = (RETSIZE+DATASIZE+1) * 2
	header = (RETSIZE+1) * 2

	s_mov	AX,DS
	s_mov	ES,AX
	_les	DI,[BP+header]
	cmp	(PiHeader ptr ES:[DI]).commentlen,0
	je	short _exit

; file open
	push	ES
	_push	[BP+filename+2]
	push	[BP+filename]
	call	DOS_ROPEN
	pop	ES
	jc	short _errorExit
	mov	BX,AX		; BX = file handle

; header read
; Pi CHECK
	mov	mem_AllocID,MEMID_pi
	mov	CX,(PiHeader ptr ES:[DI]).commentlen
	inc	CX
	inc	CX
	push	CX
	call	HMEM_ALLOCBYTE
	jc	short _InsufficientMemory

	mov	(PiHeader ptr ES:[DI]).comment_offset,2
	mov	(PiHeader ptr ES:[DI]).comment_segment,AX

	push	DS
	mov	DS,AX
	mov	AH,3fh	; read
	xor	DX,DX
	int	21h
	pop	DS
	jc	short _InvalidData
	cmp	AX,CX
	jne	short _InvalidData

	mov	ES,(PiHeader ptr ES:[DI]).comment_segment
	cmp	word ptr ES:0,'iP'
	jne	short _InvalidData

	mov	AH,3eh	; close
	int	21h

_exit:
	pop	DI
	pop	BP
	xor	AX,AX	; NoError, clc
	ret	(DATASIZE+DATASIZE)*2

_InsufficientMemory:
	mov	AX,InsufficientMemory
	jmp	short _errorA
_InvalidData:
	push	(PiHeader ptr ES:[DI]).comment_segment
	call	HMEM_FREE
	xor	AX,AX
	stosw			; comment_offset
	stosw			; comment_segment
	mov	AX,InvalidData
_errorA:
	push	AX
	mov	AH,3eh	; close
	int	21h
	pop	AX
_errorExit:
	stc

	pop	DI
	pop	BP
	ret	(DATASIZE+DATASIZE)*2
endfunc				; }

END
