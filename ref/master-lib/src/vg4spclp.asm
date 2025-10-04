; superimpose & master library module
;
; Description:
;	パターンの表示 [yクリッピング]
;
; Functions/Procedures:
;	void vga4_super_put_clip( int x, int y, int num ) ;
;
; Parameters:
;	x	x座標 0～(640-パターン幅)
;	y	y座標
;	num	パターン番号
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA 16Color
;
; Requiring Resources:
;	CPU: 186
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
;$Id: superclp.asm 0.03 92/05/29 20:26:40 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 9/20 [M0.21] WORD_MASK廃止
;	94/ 4/ 1 [M0.23] 下クリップするときに描画が崩れていた
;	94/ 6/ 6 Initial: vg4spclp.asm/master.lib 0.23
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc
	.DATA

	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	BYTE_MASK:BYTE
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD, graph_VramLines:WORD

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN
endm

func VGA4_SUPER_PUT_CLIP	; vga4_super_put_clip() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BX,[BP+num]
	mov	BP,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	mov	SI,CX
	mov	AL,BYTE_MASK[SI]
	mov	CS:MIDMASK,AL
	shr	BP,3		;BP=x/8
	shl	BX,1
	mov	DX,super_patsize[BX]
	push	super_patdata[BX]			;*******
	mov	CH,DH			; xsize
	shr	CH,1

	xor	AX,AX
	mov	SI,AX
	mov	BX,AX			; 毎回描画前にSIに加算する値

	mov	AL,DL	; AH=0		; ysize

	test	DI,DI
	jns	short upper_full

	neg	DI			; 上クリップ
	sub	AX,DI
	jle	short RETURN

	mov	DL,AL
	mov	AX,DI
	imul	DH
	mov	BX,AX
	jmp	short clip_end
	EVEN

RETURN:
	pop	AX					;*******
	MRETURN

upper_full:
	cmp	DI,graph_VramLines	; 上端が画面下を越えているか?
	jge	short RETURN

	add	AX,DI			; 下端が画面下を越えているか?
	sub	AX,graph_VramLines
	jle	short bottom_full

	sub	DL,AL			; DL(ysize)を下に出たぶん減らす
	imul	DH
	add	BX,AX
	sub	SI,AX			; 前もってSIは引いておく

bottom_full:
	mov	AX,graph_VramWidth
	push	DX
	mul	DI	; AX = DI(y) * graph_VramWidth
	pop	DX
	add	BP,AX	; BP = draw address

clip_end:
	mov	CS:YLEN,DL
	mov	CS:SKIP_SOURCE,BX

	mov	AL,byte ptr graph_VramWidth
	sub	AL,DH

	test	DH,1
	jnz	short odd_len

	; 長さ偶数

	mov	BYTE PTR CS:[count1],CH
	mov	BYTE PTR CS:[add_di1],AL
	mov	CS:DISP_ADDRESS,offset DISP1 - offset JUMP_ORIGIN
	jmp	short start
	EVEN

	; 長さ奇数
odd_len:
	mov	BYTE PTR CS:[count2],CH
	mov	BYTE PTR CS:[add_di2],AL
	mov	CS:DISP_ADDRESS,offset DISP2 - offset JUMP_ORIGIN
	EVEN

start:
	mov	ES,graph_VramSeg
	pop	DS					;*******

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

	MRETURN
endfunc			; }

DISP1	proc	near
put_loop1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,BH
	xor	DH,AL
	or	AL,DL

	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2

	mov	DL,DH
	dec	CH
	jnz	short put_loop1
	test	ES:[DI],DL
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di1		EQU	$-1
	mov	CH,11h	;dummy
count1		EQU	$-1
	dec	BL
	jnz	short put_loop1
	ret
	EVEN
DISP1	endp

DISP2	proc near
single_check2:
	or	CH,CH
	jz	short skip2
	EVEN
put_loop2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,BH
	xor	DH,AL
	or	AL,DL

	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AL
	mov	ES:[DI+1],AH
	add	DI,2

	mov	DL,DH
	dec	CH
	jnz	short put_loop2
skip2:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL

	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AL
	mov	ES:[DI+1],AH
	inc	DI

	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di2		EQU	$-1
	mov	CH,11h	;dummy
count2		EQU	$-1
	dec	BL
	jnz	short single_check2
	ret
	EVEN
DISP2	endp

	; in:
	;   ES:BP = vram address
	;   DS:SI = data address
	;   CL = shift
DISP	proc	near
	mov	DX,SEQ_PORT
	out	DX,AX
	xor	DL,DL
	mov	DI,BP
	add	SI,1234h
	org $-2
SKIP_SOURCE dw ?
	mov	BX,1234h
	org $-2
YLEN	db ?	; BL
MIDMASK	db ?	; BH

	jmp	near ptr DISP1	; ダミーで遠いところを
	org	$-2
DISP_ADDRESS	dw	?
JUMP_ORIGIN	label	word
DISP	endp

END
