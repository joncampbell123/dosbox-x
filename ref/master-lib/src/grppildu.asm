PAGE 98,120
;tab 8
; master library - Pi Format Graphic File Load
;
; Description:
;	16色のPi Format GraphicをFileからunpack形式で読み込みます。
;
; Function/Procedures:
;	void far *MASTER_RET graph_pi_load_unpack(char *filename,PiHeader *header);
;
; Parameters:
;	char *filename		ファイルネーム
;	PiHeader	*header		情報格納用引数
;
;	typedef struct PiHeader PiHeader ;
;	struct PiHeader {
;	unsigned char	far *comment; // この関数ではNULLがsetされるだけ
;	unsigned int	commentlen;
;	unsigned char	mode;
;	unsigned char	n; // aspect
;	unsigned char	m; // aspect
;	unsigned char	plane; // 通常は 4
;	unsigned char	machine[4];
;	unsigned int	maexlen; // machine extend data length
;	unsigned char	far *maex; // machine extend data
;	unsigned int	xsize;
;	unsigned int	ysize;
;	unsigned char	palette[48];
;	} ;
;
; Returns:
;	0		= Pi Load Not Successful
;	Others	= Buffer Address
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;
; Compiler/Assembler:
;	TASM 2.51
;
; Note:
;	誤ったデータやVA用データを読ませた場合の動作は保証しません。
;	コメントは、本関数を実行しただけでは読み込まれません。
;	返り値は、画像の始まりを指しています。メモリブロックの始まりは
;	そこより-(xsize*2)移動した位置にあります。
;
; Author:
;	SuCa
;
; Rivision History:
;	93/10/13 initial
;	93/11/05 加速と、ヘッダー読み込みの追加
;	93/11/21 Initial: grppildu.asm/master.lib 0.21
;	93/12/ 5 [M0.22] palette

	.186

	.MODEL SMALL
	include func.inc
	EXTRN	DOS_ROPEN:CALLMODEL
	EXTRN	HMEM_LALLOCATE:CALLMODEL
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL

	BUFF_LEN EQU 1024*16
	EOF EQU 26

;	debug = 1
IFDEF ??version		; tasm check
	JUMPS
	WARN
ENDIF

.DATA?
; for pi decode
	xwidth	DW ?
	ywidth	DW ?
	gbuffer	DD ?
	gblast	DD ?
	ctable	DB 256 DUP(?)
	mode	DB ?

IFDEF debug
.DATA
; for debug
	public _dss,_ess,_dii,_sii

	_dss	DW 0
	_ess	DW 0
	_dii	DW 0
	_sii	DW 0

	_where	DW -1
ENDIF

; for bit_load
.DATA?
	handle	DW ?
	buff_p	DD ?
	bit_buf	DB ?
	bit_len	DB ?

.CODE

_too	macro	regoff,regwrk,regseg
	local	_toolabel
	or	regoff,regoff
	jnz	short _toolabel
	mov	regwrk,regseg
	add	regwrk,1000h
	mov	regseg,regwrk
	even
	_toolabel:
endm

_tor	macro	regdst,regwrk,regseg,regoff,value
	local	_torlabel
	mov	regdst,regseg:[regoff-value]
	cmp	regoff,value
	jnc	short _torlabel
	push	es
	mov	regwrk,regseg
	sub	regwrk,1000h
	mov	regseg,regwrk
	mov	regdst,regseg:[regoff-value]
	pop	es
	even
	_torlabel:
endm

_toi	macro	label
	loop	short label
	sub	BP,1
	jnc	short label
endm

EVEN
retfunc _errorA
	pop	DI
	pop	SI
	pop	BP
	xor	AX,AX
	xor	DX,DX
	ret	(DATASIZE+DATASIZE)*2
endfunc

func GRAPH_PI_LOAD_UNPACK	; graph_pi_load_unpack() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; 引数
	filename = (RETSIZE+1+DATASIZE)*2
	header	 = (RETSIZE+1)*2

	cld
	mov	bit_buf,0
	mov	bit_len,0
; file open
	_push	[bp+filename+2]
	push	[bp+filename]
	call	DOS_ROPEN
	jc	_errorA
	mov	handle,AX

; load buffer allocate
	push	BUFF_LEN
	call	SMEM_WGET
	jc	_errorA
	mov	CX,BUFF_LEN
	mov	BX,handle
	xor	DX,DX
	mov	WORD PTR buff_p,DX
	mov	WORD PTR buff_p+2,AX
	push	DS
	mov	DS,AX
	mov	AH,3fh
	int	21h	; buffer load
	pop	DS

; color table initialize
	mov	AX,DS
	mov	ES,AX
	mov	AX,1
	mov	CX,16
	mov	DI,OFFSET ctable
even
_ctinit_loop:
	and	AL,15
	stosb
	inc	AL
	inc	AH
	test	AH,00001111b
	jnz	short _ctinit_loop
	inc	AL
	loop	short _ctinit_loop

; header read
; Pi CHECK
	mov	CH,8
	call	_bit_load
	cmp	DL,'P'
	jnz	_errorA
	call	_bit_load
	cmp	DL,'i'
	jnz	_errorA

	_les	DI,[bp+header]
	xor	AX,AX
	stosw
	stosw
	dec	AX
_comment_skip:
	inc	AX
	call	_bit_load
	cmp	DL,EOF
	jnz	_comment_skip
	stosw
_dummy_skip:
	call	_bit_load
	or	DL,DL
	jnz	_dummy_skip

; MODE
	call	_bit_load
	mov	AL,DL
	stosb
	mov	mode,AL
; ASPECT RATIO
	call	_bit_load
	or	DL,DL
	jnz	_errorA
	mov	AH,DL
	call	_bit_load
	or	DL,DL
	jnz	_errorA
	mov	AL,DL
	stosw
; COLOR
	call	_bit_load
	cmp	DL,4
	jnz	_errorA
	mov	AL,DL
	stosb
; MACHINE INFORMATION
	call	_bit_load
	mov	AL,DL
	call	_bit_load
	mov	AH,DL
	stosw
	call	_bit_load
	mov	AL,DL
	call	_bit_load
	mov	AH,DL
	stosw

	call	_bit_load
	mov	AH,DL
	call	_bit_load
	mov	AL,DL
	stosw
	mov	BX,AX	; BX = minfo length
	xor	AX,AX
	stosw
	push	AX
	push	BX
	call	HMEM_LALLOCATE
	stosw
	jc	short _minfo_skipB
	mov	ES,AX
	xor	DI,DI
even
_minfo_skipA:
	call	_bit_load
	mov	AL,DL
	stosb
	dec	BX
	jnz	short _minfo_skipA
	les	DI,[BP+header]
	add	DI,17
even
_minfo_skipB:
; PICTURE WIDTH
	call	_bit_load
	mov	AH,DL
	call	_bit_load
	mov	AL,DL
	stosw
	mov	xwidth,AX
	mov	BX,AX
	call	_bit_load
	mov	AH,DL
	call	_bit_load
	mov	AL,DL
	stosw
	mov	ywidth,AX

;GBUFFER ALLOCATE
	add	AX,2
	mul	BX
	push	DX
	push	AX
	shl	DX,12
	mov	WORD PTR gblast,AX
	mov	WORD PTR gblast+2,DX
	call	HMEM_LALLOCATE
	jc	_errorA
	mov	WORD PTR gbuffer,0
	mov	WORD PTR gbuffer+2,AX
	add	WORD PTR gblast+2,AX

;PALETTE
	shl	mode,1
	jc	short _palette_skip
	mov	BL,48
	mov	CH,8
even
_palette_load:
	call	_bit_load
	mov	AL,DL
	stosb
	dec	BL
	jnz	short _palette_load
even
_palette_skip:
;header read end


;decode pi data
	xor	BX,BX
	call	_read_color
	mov	BX,DX
	call	_read_color
	les	DI,gbuffer
	mov	CX,xwidth
even
_paint_gbuffer_2lines:
	mov	AL,BL
	stosb
	_too	DI,AX,ES
	mov	AL,DL
	stosb
	_too	DI,AX,ES
	loop	short _paint_gbuffer_2lines

	mov	CL,-1

IFDEF debug
	mov	[_where],127
	mov	ax,offset ctable
	call	_debug
ENDIF
;主ループ	CL == 前回位置データ ES:DI == gbuffer
even
_main_decode_loopA:
;						position decode
	mov	CH,2
	call	_bit_load
	mov	BL,DL
	cmp	DL,3
	jnz	short _position_decode_skip
	mov	CH,1
	call	_bit_load
	add	BL,DL
even
_position_decode_skip:
	cmp	BL,CL
	jnz	short _decode_forkA

;分岐先A
even
_decode_loopA:
	_tor	BL,AX,ES,DI,1
	call	_read_color
	mov	BL,DL
	mov	AL,DL
	stosb
	_too	DI,AX,ES
	call	_read_color
	mov	AL,DL
	stosb
	_too	DI,AX,ES
;IFDEF debug
;	mov	[_where],0
;	call	_debug
;ENDIF
	mov	CH,1
	call	_bit_load
	or	DL,DL
	jnz	short _decode_loopA
	mov	CL,-1
	jmp	_end_checkB

; 分岐先B	BP:DX = length
even
_decode_forkA:
; BL = position
	xor	BH,BH
	xor	BP,BP
	mov	AX,1
	mov	CH,AL
even
loop_rlA:
	inc	BH
	call	_bit_load
	or	DL,DL
	jnz	loop_rlA
	dec	BH
	jz	short end_rlA
even
loop_rlB:
	mov	CH,BH
	sub	BH,8
	jc	short skip_rlB
	mov	CH,8
even
skip_rlB:
	call	_bit_load
	mov	CL,CH
	xor	CH,CH
even
loop_rlC:
	shl	AX,1
	rcl	BP,1
	loop	short loop_rlC
	or	AL,DL
	cmp	BH,0
	jg	short loop_rlB
even
end_rlA:
	mov	CX,AX

	test	BL,BL
	jz	short _position_zero
	mov	AX,xwidth
	cmp	BL,1
	jz	short _ssC
	cmp	BL,3
	jnz	short _ssA
	dec	AX
	jmp	short _ssC
even
_ssA:
	cmp	BL,4
	jnz	short _ssB
	inc	AX
	jmp	short _ssC
even
_ssB:
	shl	AX,1
even
_ssC:
	push	DS
	mov	SI,DI
	sub	SI,AX
	mov	DX,ES
	jnb	short _nbs0
	sub	DX,1000h
even
_nbs0:
	mov	DS,DX
even
_slA:
;IFDEF debug
;	mov	[SS:_where],1
;	call	_debug
;ENDIF
	lodsb
	mov	DL,AL
	stosb
	_too	DI,AX,ES
	_too	SI,AX,DS
	lodsb
	mov	DH,AL
	stosb
	_too	DI,AX,ES
	_too	SI,AX,DS
	_toi	_slA
	pop	DS
	jmp	_end_check

even
_position_zero:
	_tor	DL,AX,ES,DI,2
	_tor	DH,AX,ES,DI,1
	cmp	DL,DH
	jnz	short _length_four
	mov	AL,DL
even
_slB:
;IFDEF debug
;	mov	[_where],2
;	mov	dl,al
;	call	_debug
;ENDIF
	stosb
	_too	DI,DX,ES
	stosb
	_too	DI,DX,ES
	_toi	_slB
	jmp	short _end_check
even
_length_four:
	_tor	BH,AX,ES,DI,4
	_tor	AH,AX,ES,DI,3
even
_slC:
;IFDEF debug
;	mov	[_where],3
;	call	_debug
;ENDIF
	mov	AL,BH
	stosb
	_too	DI,AX,ES
	mov	AL,AH
	stosb
	_too	DI,AX,ES
	_toi	_slD
	jmp	short _end_check
even
_slD:
	mov	AL,DL
	stosb
	_too	DI,AX,ES
	mov	AL,DH
	stosb
	_too	DI,AX,ES
	_toi	_slC
even
_end_check:
	mov	CL,BL
even
_end_checkB:
	cmp	WORD PTR gblast,DI
	jnbe	_main_decode_loopA
	mov	AX,ES
	cmp	WORD PTR gblast+2,AX
	jnbe	_main_decode_loopA

;file close
	mov	AH,3eh
	mov	BX,handle
	int	21h

	push	WORD PTR [buff_p+2]
	call	SMEM_RELEASE

	mov	AX,xwidth
	shl	AX,1
	add	AX,WORD PTR gbuffer
	mov	DX,WORD PTR gbuffer+2

	pop	DI
	pop	SI
	pop	BP
	ret	(DATASIZE+DATASIZE)*2
endfunc			; }

;	IN
;	BL = いちどっと左の色
; OUT
;	DL = 色
; BREAK
;	DL
even
_read_color proc near
	push	AX
	push	BX
	push	CX
; 色符号の解読(前半)
	xor	AX,AX
	mov	CH,1
	call	_bit_load
	or	DL,DL
	jnz	short _skip_rc
	call	_bit_load
	add	AL,2
	or	DL,DL
	jz	short _skip_rc
	call	_bit_load
	add	AL,2
	inc	AH
	or	DL,DL
	jz	short _skip_rc
	add	AL,4
	inc	AH
even
_skip_rc:
; 色符号の解読(後半)
	add	CH,AH
	call	_bit_load
	add	DL,AL
	mov	AL,15
	sub	AL,DL
; 色表のアドレス算定
	xor	AH,AH
	xor	BH,BH
	shl	BX,4
	add	BX,OFFSET ctable
IFDEF debug
	mov	[_where],10
	call	_debug
ENDIF
	mov	CX,BX
	add	BX,AX
	xor	DX,DX
; 色表の更新と色の読みだし
	add	CX,15
IFDEF debug
	mov	[_where],11
	call	_debug
ENDIF
	jmp	short _loop_rcB
even
_loop_rcA:
	inc	BX
	mov	[BX],DL
	mov	[BX - 1],AL
even
_loop_rcB:
	mov	DL,[BX]
	mov	AL,[BX+1]
	cmp	CX,BX
	jnz	short _loop_rcA

IFDEF debug
	mov	[_where],12
	call	_debug
ENDIF
	pop	CX
	pop	BX
	pop	AX
	ret
_read_color endp


;	IN
;	CH = LOAD LENGTH
; OUT
;	DH = 0
;	DL = LOAD VALUE(Signed char)
; BREAK
;	DX SI
even
_bit_load proc near
	push	ES
	push	CX
	xor	DX,DX
	mov	DL,bit_buf
	mov	CL,bit_len
;if (bl < sz)
	cmp	CL,CH
	jnc	short _nbbl_bil
;shl bl : byteload : sz -= bl : bl = 8
	shl	DX,CL

	push	DS
	lds	SI,buff_p
	cmp	SI,BUFF_LEN
	jc	short _not_load_bilA
	pop	ES
	push	ES
	pusha	; (DX CX BX AX)
	mov	AH,3fh
	mov	BX,ES:handle
	mov	CX,BUFF_LEN
	xor	DX,DX
	int	21h
	popa
	xor	SI,SI
even
_not_load_bilA:
	mov	DL,[SI]
	inc	SI
	sub	CH,CL
	mov	CL,8
	pop	DS
	mov	WORD PTR buff_p,SI

even
_nbbl_bil:
;shl sz : bl -= sz
	xchg	CL,CH
	shl	DX,CL
	sub	CH,CL
	mov	bit_buf,DL
	mov	bit_len,CH
	mov	DL,DH
	xor	DH,DH
	pop	CX
	pop	ES
	ret
_bit_load endp

IFDEF debug
	EXTRN	C printf:proc

_debug proc near
	pusha
	push	ds
	push	es

	push	AX
	mov	AX,@data
	mov	DS,AX
	pop	AX
	call	printf c,offset dbg,WORD PTR [gblast],SI,AX,BX,CX,DX,WORD PTR [_where]
;	call	printf c,offset dbg,ES,DI,AX,SI,CX,BP,BX,DX,WORD PTR [_where]

	pop	es
	pop	ds
	popa
	ret
_debug endp

.data
	dbg	db "ese %04X SI %04X AX %04X BX %04X CX %04X DX %04X wh %d",10,0
;	dbg	db "ES %04X DI %04X DS %04X SI %04X CX %04X BP %04X BX %04X DX %04X wh %d",10,0

ENDIF

END
