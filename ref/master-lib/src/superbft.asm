; superimpose & master library module
;
; Description:
;	BFNT+ファイルからのパターンの登録
;
; Functions/Procedures:
;	int super_entry_bfnt( const char * file_name ) ;
;
; Parameters:
;	file_name	BFNT+ファイル名
;
; Returns:
;	
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
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: superbft.asm 0.06 93/01/16 14:17:53 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
header		bfnt_header	<>

	.CODE

MRETURN	macro
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	DATASIZE*2
	EVEN
	endm

	EXTRN	FONTFILE_OPEN:CALLMODEL
	EXTRN	BFNT_HEADER_READ:CALLMODEL
	EXTRN	BFNT_EXTEND_HEADER_ANALYSIS:CALLMODEL
	EXTRN	BFNT_PALETTE_SET:CALLMODEL
	EXTRN	BFNT_ENTRY_PAT:CALLMODEL
	EXTRN	FONTFILE_CLOSE:CALLMODEL

func SUPER_ENTRY_BFNT
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	file_name = (RETSIZE+1)*2

	_push	[BP+file_name+2]
	push	[BP+file_name]
	_call	FONTFILE_OPEN
	jc	short return

	mov	BX,AX			;file handle
	mov	CX,offset header
	push	BX
	push	CX
	push	BX	; handle
	_push	DS
	push	CX	; header
	_call	BFNT_HEADER_READ	; bfnt_header_read(handle, header)
	pop	CX
	pop	BX
	jc	short error

	mov	AL,header.col
	and	AL,7fh
	cmp	AL,3
	mov	AX,InvalidData
	jne	short error

	xor	SI,SI
	mov	AX,header.extSize
	or	AX,AX
	jz	short no_exthdr
	push	BX
	push	CX
	push	BX	; handle
	_push	DS
	push	CX	; header
	_call	BFNT_EXTEND_HEADER_ANALYSIS	; (handle, header)
	pop	CX
	pop	BX
	mov	SI,AX	; clear_color
	EVEN
no_exthdr:
	test	header.col,80h		;palette table check
	jz	short palette_end
	push	BX
	push	CX

	push	BX	; handle
	_push	DS
	push	CX	; header
	_call	BFNT_PALETTE_SET	; (handle, header)

	pop	CX
	pop	BX
	jc	short error
	EVEN
palette_end:
	push	BX

	push	BX	; handle
	_push	DS
	push	CX	; header
	push	SI	; clear_color
	_call	BFNT_ENTRY_PAT	; (handle, header, clear_color)

	pop	BX
	jc	short error

	push	BX
	_call	FONTFILE_CLOSE
	mov	AX,header.END_
	sub	AX,header.START
	inc	AX
	MRETURN

error:
	push	AX
	push	BX
	_call	FONTFILE_CLOSE
	pop	AX
	stc
return:
	MRETURN
endfunc

END
