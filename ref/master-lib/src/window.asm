; superimpose & master library module
;
; Description:
;	パターンの表示 [y任意クリッピング]
;
; Functions/Procedures:
;	void super_set_window( int y1, int y2 ) ;
;	void super_put_window( int x, int y, int num ) ;
;
; Parameters:
;	y1,y2	クリッピング範囲  必ず y1 <= y2
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
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
;$Id: window.asm 0.04 92/05/29 20:44:57 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 9/20 [M0.21] WORD_MASK廃止
;	94/ 4/ 2 [M0.23] サイズ縮小 from superclp.asm
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	BYTE_MASK:BYTE

	.CODE

func SUPER_SET_WINDOW
	mov	BX,SP
	top	= (RETSIZE+1)*2
	bottom	= (RETSIZE+0)*2

	mov	AX,SS:[BX+top]
	mov	CS:WINDOW_TOP,AX
	mov	AX,SS:[BX+bottom]
	inc	AX
	mov	CS:WINDOW_BOTTOM1,AX
	mov	CS:WINDOW_BOTTOM2,AX
	ret	4
endfunc

MRETURN macro
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN
endm

retfunc RETURN
	MRETURN
endfunc

func SUPER_PUT_WINDOW	; super_put_window() {
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
	mov	DS,super_patdata[BX]
	mov	CH,DH			; xsize
	shr	CH,1

	xor	AX,AX
	mov	SI,AX

	mov	BX,0
	org $-2
WINDOW_TOP dw ?
	sub	BX,DI
	jl	short upper_full

	mov	AL,DL	; AH=0		; ysize
	sub	AX,BX			; 上クリップ
	jle	short RETURN

	add	DI,BX
	mov	DL,AL
	mov	AX,BX
	imul	DH

upper_full:
	mov	BX,AX			; 毎回描画前にSIに加算する値
	mov	AL,DL			; ysize
	mov	AH,0

	cmp	DI,400			; 上端が画面下を越えているか?
 org	$-2
WINDOW_BOTTOM1 dw ?
	jge	short RETURN

	add	AX,DI			; 下端が画面下を越えているか?
	sub	AX,400
 org	$-2
WINDOW_BOTTOM2 dw ?
	jle	short bottom_full

	sub	DL,AL			; DL(ysize)を下に出たぶん減らす
	imul	DH
	add	BX,AX
	sub	SI,AX			; 前もってSIは引いておく

bottom_full:
	mov	AX,DI	; AX = DI(y) * 80
	shl	AX,2
	add	AX,DI
	shl	AX,4
	add	BP,AX	; BP = draw address

clip_end:
	mov	CS:YLEN,DL
	mov	CS:SKIP_SOURCE,BX

	test	BP,1
	jnz	short odd_address

	; 描画アドレスが偶数
	mov	AL,80
	sub	AL,DH

	test	DH,1
	jnz	short odd_size1

	; 描画アドレスが偶数・長さも偶数

	mov	BYTE PTR CS:[count1],CH
	mov	BYTE PTR CS:[add_di1],AL
	mov	CS:DISP_ADDRESS,offset DISP1 - offset JUMP_ORIGIN
	jmp	short start
	EVEN

	; 描画アドレスが偶数・長さは奇数
odd_size1:
	mov	BYTE PTR CS:[count2],CH
	mov	BYTE PTR CS:[add_di2],AL
	mov	CS:DISP_ADDRESS,offset DISP2 - offset JUMP_ORIGIN
	jmp	short start
	EVEN

	; 描画アドレスが奇数
odd_address:
	dec	BP
	test	DH,1
	jnz	short odd_size2

	; 描画アドレスが奇数・長さは偶数
	dec	CH		;!!!!!!!!!!!!!!!!!!!!!
	mov	BYTE PTR CS:[count3],CH

	mov	AL,78		;word
	sub	AL,DH
	mov	BYTE PTR CS:[add_di3],AL
	mov	CS:DISP_ADDRESS,offset DISP3 - offset JUMP_ORIGIN
	jmp	short start
	EVEN

	; 描画アドレスが奇数・長さも奇数
odd_size2:
	mov	BYTE PTR CS:[count4],CH

	mov	AL,79		;word
	sub	AL,DH
	mov	BYTE PTR CS:[add_di4],AL
	mov	CS:DISP_ADDRESS,offset DISP4 - offset JUMP_ORIGIN
	EVEN
start:
	mov	AX,0a800h	; VRAM segment address
	mov	ES,AX

	CLD
	mov	AX,0c0h		;RMW mode, all plane
	out	7ch,AL
	mov	AL,AH
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	call	DISP		;originally cls_loop

	mov	AX,0ff00h + 11001110b
	out	7ch,AL		;RMW mode, blue plane
	mov	AL,AH		;AL==0ffh
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	call	DISP

	mov	AL,11001101b
	out	7ch,AL		;RMW mode, red plane
	call	DISP

	mov	AL,11001011b
	out	7ch,AL		;RMW mode, green plane
	call	DISP

	mov	AL,11000111b
	out	7ch,AL		;RMW mode, intensity plane
	call	DISP

	xor	AL,AL
	out	7ch,AL		;grcg stop
	MRETURN
endfunc			; }

DISP1	proc	near
	xor	DL,DL
	EVEN
put_loop1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,BH
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short put_loop1
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
	xor	DL,DL
	EVEN
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
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short put_loop2
skip2:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	stosw
	dec	DI
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

DISP3	proc near
put_loop3_1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	mov	DL,AH
	xchg	AH,AL
	xor	AL,AL
	stosw
	or	CH,CH
	jz	short no_word1
	EVEN
put_loop3_2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,BH
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short put_loop3_2
	EVEN
no_word1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	stosw
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di3		EQU	$-1
	mov	CH,11h	;dummy
count3		EQU	$-1
	dec	BL
	jnz	short put_loop3_1
	ret
	EVEN
DISP3	endp

DISP4	proc near
put_loop4_1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	mov	DL,AH
	xchg	AH,AL
	xor	AL,AL
	stosw
	or	CH,CH
	jz	short no_word2
	EVEN
put_loop4_2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,BH
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short put_loop4_2
	EVEN
no_word2:
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di4		EQU	$-1
	mov	CH,11h	;dummy
count4		EQU	$-1
	dec	BL
	jnz	short put_loop4_1
	ret
DISP4	endp

	; in:
	;   ES:BP = vram address
	;   DS:SI = data address
	;   CL = shift
DISP	proc	near
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
