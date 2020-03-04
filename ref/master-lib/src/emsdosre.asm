; master library - EMS FILE READ
;
; Description:
;	ファイルの内容をEMSメモリに読み込む
;
; Function/Procedures:
;	long ems_dos_read(int file_handle, unsigned short ems_handle, 
;				unsigned long ems_offs, long read_bytes)
;
; Parameters:
;	int		file_handle	ファイルハンドル
;	unsigned short	ems_handle	EMSハンドル
;	unsigned long	ems_offs	EMSメモリへのオフセット
;	long 		read_bytes	読み込むバイト数
;
; Returns:
;	long	読み込んだバイト数(エラーが発生した場合、-1)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	DOS
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;
; Assembly Language Note:
;
;
; Compiler/Assembler:
;	OPTASM 1.5
;
; Author:
;	benny(安井　勉)		: オリジナル・C言語版
;	benny(C6 /Fa :-))	: アセンブリ言語版
;
; Revision History:
;	93/07/16 Initial: emsdosre.c 
;	94/02/22	: emsdosre.asm

	.MODEL SMALL
	include func.inc
	include super.inc

LOCAL_BUFFER_SIZE	EQU	1024 * 16	;16K固定

FP		struc
FP_OFF		dw	?
FP_SEG		dw	?
FP		ends

	.CODE
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL
	EXTRN	EMS_WRITE:CALLMODEL
	EXTRN	DOS_READ:CALLMODEL

func EMS_DOS_READ	; ems_dos_read() {
	push	bp
	mov	bp, sp
	sub	sp, 12	;引数にポインタがないため固定値

	read_counter	= -10
	dosrbytes	= -12
	dosread_bytes	= -6
	local_buf	= -4
	file_handle	= (RETSIZE+6)*2
	ems_handle	= (RETSIZE+5)*2
	ems_offs	= (RETSIZE+3)*2
	read_bytes	= (RETSIZE+1)*2

	mov	ax, LOCAL_BUFFER_SIZE
	push	ax
	_call	SMEM_WGET
	jnc	short YESMEM
NOMEM:
	mov	ax, -1
	cwd
	mov	sp, bp
	pop	bp
	ret	12
YESMEM:
	mov	[bp + local_buf].FP_SEG, ax
	sub	ax, ax
	mov	[bp + local_buf].FP_OFF, ax
	mov	[bp + read_counter].FP_SEG, ax
	mov	[bp + read_counter].FP_OFF, ax
LOOP_TOP:
	mov	ax, [bp + read_bytes].FP_OFF
	mov	dx, [bp + read_bytes].FP_SEG
	sub	ax, [bp + read_counter].FP_OFF
	sbb	dx, [bp + read_counter].FP_SEG
	or	dx, dx
	jne	short BIGGER_THAN_BUFSIZE
	cmp	ax, LOCAL_BUFFER_SIZE
	jbe	short SMOLLER_THAN_BUFSIZE
BIGGER_THAN_BUFSIZE:
	mov	WORD PTR [bp + dosread_bytes], LOCAL_BUFFER_SIZE
	jmp	SHORT READ_FROM_FILE
	EVEN
SMOLLER_THAN_BUFSIZE:
	mov	ax, WORD PTR [bp + read_bytes]
	sub	ax, WORD PTR [bp + read_counter]
	mov	WORD PTR [bp + dosread_bytes], ax
READ_FROM_FILE:
	push	WORD PTR [bp + file_handle]
	push	[bp + local_buf].FP_SEG
	push	[bp + local_buf].FP_OFF
	push	WORD PTR [bp + dosread_bytes]
	_call	DOS_READ
	cmp	ax, WORD PTR [bp + dosread_bytes]
	jne	short NOMEM

	push	[bp + ems_handle].FP_OFF
	push	[bp + ems_offs].FP_SEG
	push	[bp + ems_offs].FP_OFF
	push	[bp + local_buf].FP_SEG
	push	[bp + local_buf].FP_OFF
	sub	dx, dx
	push	dx
	push	ax
	_call	EMS_WRITE
	or	ax, ax
	jne	short NOMEM

	mov	ax,WORD PTR [bp + dosread_bytes]
	sub	dx, dx
	add	[bp + ems_offs].FP_OFF, ax
	adc	[bp + ems_offs].FP_SEG, dx
	add	[bp + read_counter].FP_OFF, ax
	adc	[bp + read_counter].FP_SEG, dx
	mov	ax, [bp + read_bytes].FP_OFF
	mov	dx, [bp + read_bytes].FP_SEG
	cmp	[bp + read_counter].FP_SEG, dx
	jl	short LOOP_TOP
	jg	short LOOP_END
	cmp	[bp + read_counter].FP_OFF, ax
	jb	short LOOP_TOP
LOOP_END:
	push	[bp + local_buf].FP_SEG
	_call	SMEM_RELEASE
	mov	ax, [bp + read_counter].FP_OFF
	mov	dx, [bp + read_counter].FP_SEG
	mov	sp, bp
	pop	bp
	ret	12
endfunc			; }
END
