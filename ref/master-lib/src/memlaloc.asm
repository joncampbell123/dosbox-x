; master library - MSDOS
;
; Description:
;	メインメモリの上位からの確保(バイト数指定)
;
; Function/Procedures:
;	unsigned mem_lallocate( unsigned long bytesize ) ;
;
; Parameters:
;	unsigned bytesize	確保するバイト数( 0～0FFFFFh )
;
; Returns:
;	unsigned 確保されたDOSメモリブロックのセグメント
;		 0 ならメモリが足りないので失敗でやんす
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;	DOS: 3.0 or later
;
; Notes:
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
;	92/12/11 Initial
;	93/ 2/23 加速,縮小
;	93/ 3/ 4 bugfix

	.MODEL SMALL
	include func.inc
	.CODE

	EXTRN MEM_ALLOCATE:CALLMODEL

func MEM_LALLOCATE
	push	BP
	mov	BP,SP
	push	BX
	push	CX
	; 引数
	sizehigh = (RETSIZE+2)*2
	sizelow	 = (RETSIZE+1)*2

	mov	AX,[BP+sizelow]
	mov	BX,[BP+sizehigh]
	add	AX,0fh		; 16byte未満は切り上げ
	adc	BX,0
	cmp	BX,10h
	jnb	short FAULT
	and	AX,0fff0h
	or	AX,BX
	mov	CL,4
	ror	AX,CL
	push	AX
	call	MEM_ALLOCATE
RETURN:
	pop	CX
	pop	BX
	pop	BP
	ret	4
FAULT:
	xor	AX,AX
	jmp	short RETURN
endfunc

END
