; master library - VGA 16Color
;
; Description:
;	
;
; Functions/Procedures:
;	void vga4_super_wave_put(int x,int y,int num,int len,char amp,int ph);
;
; Parameters:
;	
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	SS=DSÇ™ëOíÒ!!!!
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	iR
;	óˆíÀè∫ïF
;
; Revision History:
;	94/ 7/17 Initial: vg4wave.asm/master.lib 0.23

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize		: WORD
	EXTRN	super_patdata		: WORD
	EXTRN	EDGES			: WORD
	EXTRN	SinTable7		: BYTE
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.DATA?
wave_address	dw	64 dup (?)
wave_shift	dw	64 dup (?)
wave_mask	dw	64 dup (?)
count		db	?

	.CODE
func VGA4_SUPER_WAVE_PUT	; vga4_super_wave_put() {
	enter	2,0
	push	DS
	push	SI
	push	DI
	push	BP

	; à¯êî
	x	= (RETSIZE+6)*2
	y	= (RETSIZE+5)*2
	num	= (RETSIZE+4)*2
	len	= (RETSIZE+3)*2
	amp	= (RETSIZE+2)*2
	ph	= (RETSIZE+1)*2

	mov	ES,graph_VramSeg
	mov	BX,[BP+num]
	add	BX,BX
	mov	CX,super_patsize[BX]
	inc	CH
	mov	DGROUP:[count],CH
	mov	CS:S1$00DD,CL
	push	super_patdata[BX]

	mov	AX,graph_VramWidth
	imul	word ptr [BP+y]
	mov	DI,AX

	mov	AX,0FFFFh
	and	AX,[BP+len]
	mov	[BP-2],AH
	jns	short S1$0032
	neg	word ptr [BP+len]
S1$0032:
	xor	CH,CH
	xor	SI,SI
S1$0036:
	mov	AX,SI
	cwd
	shl	AX,7
	div	word ptr [BP+len]
	add	AX,[BP+ph]
	mov	BX,offset SinTable7
	xlat
	imul	byte ptr [BP+amp]
	sar	AX,7
	add	AX,[BP+x]
	mov	DX,AX
	and	AL,0Fh
	xor	DL,AL
	sar	DX,3
	add	DX,DI
	mov	AH,DGROUP:[count]
	test	AH,1
	jne	short S1$0065
	xor	AL,8			; 0000.1000
S1$0065:
	mov	DGROUP:wave_address[SI],DX
	mov	DGROUP:wave_shift[SI],AX
	xor	BH,BH
	mov	BL,AL
	add	BX,BX
	mov	DX,[BX+EDGES]
	mov	DGROUP:wave_mask[SI],DX
	test	byte ptr [BP-2],0FFh
	je	short S1$0084
	neg	byte ptr [BP+amp]
S1$0084:
	add	DI,graph_VramWidth
	add	SI,2
	loop	S1$0036
	pop	DS
	xor	SI,SI
	CLD

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_SET_RESET_REG or (0 shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX
	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	call	DISP		;originally cls_loop

	mov	DX,VGA_PORT
	mov	AX,VGA_SET_RESET_REG or (0fh shl 8)
	out	DX,AX

	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	call	DISP
	mov	AX,SEQ_MAP_MASK_REG or (2 shl 8)
	call	DISP
	mov	AX,SEQ_MAP_MASK_REG or (4 shl 8)
	call	DISP
	mov	AX,SEQ_MAP_MASK_REG or (8 shl 8)
	call	DISP

	pop	BP
	pop	DI
	pop	SI
	pop	DS
	leave
	ret	12
endfunc			; }

DISP	proc	near
	mov	DX,SEQ_PORT
	out	DX,AX

	mov	byte ptr SS:[count],0
	org $-1
S1$00DD	db	?
	xor	BP,BP
S1$00E0:
	xor	DX,DX
	mov	DI,DGROUP:[BP+wave_address]
	mov	CX,DGROUP:[BP+wave_shift]
	mov	BX,DGROUP:[BP+wave_mask]
	shr	CH,1
	jb	short S1$0100
	lodsb
	shl	AX,8
	test	CL,8			; BS
	je	short S1$0101
	ror	AX,CL
	jmp	short S1$010A

	even
S1$0100:
	lodsw
S1$0101:
	ror	AX,CL
	xor	DX,AX
	and	AX,BX
	xor	AX,DX
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2
S1$010A:
	xor	DX,AX
	dec	CH
	jne	S1$0100
	test	ES:[DI],DL
	mov	ES:[DI],DL
	test	ES:[DI+1],DH
	mov	ES:[DI+1],DH
	add	BP,2
	dec	byte ptr SS:[count]
	jne	S1$00E0
	ret
DISP	endp

END
