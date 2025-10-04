page 60,120
; master library
;
; Description:
;	フォーマット指定して文字列を作成する
;
; Function/Procedures:
;	void str_printf( char * buf, const char * format, ... ) ;
;
; Parameters:
;	char * buf	書き込み先
;	char * format	書式文字列
;	...		引数
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	none
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	Ｃ言語stdioの sprintfに似ていますが、書式が違います。
;	書式:
;		%[0][桁][l]u	右詰め符号なし10進数
;		%[0][桁][l]x	右詰め16進数
;		%[0][桁][l]b	右詰め2進数
;		%[桁]s		左詰め文字列
;		%c		文字
;		%%		%自身
;
;		[0]は、指定すると余った左側をゼロで埋めます。
;		[桁]は省略すると 0 になります。
;		[l]は、shortではなくlong整数になります。
;
;		x,Xは、どちらも大文字として動作します。
;		x,s,u,c以外の文字は全てu扱いになります。
;		ただし、文字コードが2fh以下の記号はその文字自身を表します。
;
;		[桁]の意味:
;			%u,x,b	固定の桁数(フィールド幅)
;			%s	最小桁数
;			%c	無効
;			%%	無効
;
;	FAR版は、%sのポインタはfar pointer,
;	NEAR版はnear pointerのみになります。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/21 Initial
;	92/12/10 Large対応, '0'pad対応
;	92/12/15 %X対応(near 256bytes)
;	92/12/16 %xも%Xの動作,%c,%%対応(near 242bytes, far 252bytes)
;	92/12/17 %bの追加(near 250bytes, far 260bytes)
;	93/ 6/ 9 [M0.19] format と str の seg が異なるときの bug fix(+2bytes)

	.MODEL SMALL
	include func.inc

	.CODE

; DX:AX = DX:AX / CL
; CH    = DX:AX % CL
; 92/9/30
; 92/12/10
; 92/12/16 さらに縮小(22bytes)
	public ASM_LBDIV
ASM_LBDIV proc near
LLONG:
	push	BX
	xor	BX,BX
	mov	CH,BL	; 0
	test	DX,DX		; ←この２行を取れば4bytes減るが
	je	short LWORD	; ←それは避けたい…
	xchg	BX,AX	; BX <- AX ( 被除数の下位 )
	xchg	AX,DX	; AX <- DX ( 被除数の上位 )
			; DX = 0
	div	CX
	xchg	AX,BX	; BX = 解の上位16bit
LWORD:
	div	CX
	mov	CH,DL	; CH = 余り
	mov	DX,BX
	pop	BX
	ret
	EVEN
ASM_LBDIV endp


func _str_printf
	push	BP
	mov	BP,SP
	_push	DS

	; 引数
	buf	= (RETSIZE+1)*2
	format	= (RETSIZE+1+DATASIZE)*2
	param	= (RETSIZE+1+DATASIZE+DATASIZE)*2

	push	SI
	push	DI
 s_	<push	DS>
 s_	<pop	ES>

	mov	AL,0
	_les	SI,[BP+format]	; ES:SI = DI = format
	mov	DI,SI
	mov	CX,-1
	repne	scasb		; 終端を探す
	not	CX		; CX = 終端文字を含んだ長さ
	_lds	DI,[BP+buf]	; DS:DI = buf
	_push	DS		; push <segment buf>
	_push	ES
	_pop	DS		; DS = ES = seg format

	jmp	short SloopFirst

Schar:	; 潜ってるなあ。文字だ。
	mov	AL,[BP+param]
	inc	BP
	inc	BP
Snama:
	stosb

Sloop:
	_push	ES		; push <segment buf>
	_push	DS
	_pop	ES
SloopFirst:
	mov	DX,DI
	mov	DI,SI		; ES:DI = format
	mov	AL,'%'
	repne	scasb		; while ( AL != *DI++ ) {}
	lahf
	dec	DI
	sub	DI,SI
	sahf
	xchg	AX,DI		; AX = %の前までのlen
	mov	DI,DX
	_pop	ES		; pop <segment buf> to ES

	xchg	AX,CX		; %の前までの文字列を転送
	rep	movsb
	xchg	AX,CX		; CX 復帰, AX = 0
	jnz	short Sowari	; %がなかったら終わりへ
	inc	SI		; %を飛ばす

	; ES:DI = buf
	; DS:SI = format
	mov	BX,2000h ; BH = ' ', BL = 0

	; 桁数読み取り～
	lodsb
	dec	CX
	jz	short Sowari
	cmp	AL,'0'
	jne	short S2E
	mov	BH,AL		; 桁数が 0で始まるとき: BH = '0'
	jmp	short S2N

	; こんなところに…
	; ここに来るときは AL = 0 だぞ
Sowari:			; 終わり
	; mov	AL,0
	stosb
	pop	DI
	pop	SI
	_pop	DS
	pop	BP
	ret

	; 桁数読み取りの続き
S2:	and	AL,0fh	; 省略
	aad		; AX = AH*10+AL
	mov	AH,AL
S2N:	lodsb
	dec	CX
	jz	short Sowari
	cmp	AL,'0'
S2E:	jb	short Snama	; 記号は生で書き込む(%%に対応するため)
	cmp	AL,'9'
	jbe	short S2

	cmp	AL,'s'
	je	short Sstr
	cmp	AL,'c'
	je	short Schar

	cmp	AL,'l'	; %<桁>l<文字> = long
	jne	short S4
	inc	BX	; long: BL=1  int:BL=0
	lodsb
	dec	CX
	jz	short Sowari
S4:
	xchg	BL,AH	; BL = 桁数, AH = long(1) or short(0)

	; %[0]<wid>[l]<flag>

	push	CX
	; 数字格納

	mov	CL,2
	cmp	AL,'b'		; ２進数
	je	short S4d

	mov	CL,10
	and	AL,0dfh	; 大文字に
	cmp	AL,'X'
	jne	short S4d
	mov	CL,16
S4d:
				; DI = buf  BH=pad BL = 桁  AH=long or short
	mov	CH,AH		; CH=long(1) or short(0)
	cwd			; DX = 0(AHは0か1だから)
	mov	AX,[BP+param]
	inc	BP
	inc	BP
	test	CH,CH
	jz	short S4a
	mov	DX,[BP+param]		; long
	inc	BP
	inc	BP
S4a:
	push	BX		; padchar + 桁を BX から保存
	xor	BH,BH
	dec	BX		; 0桁ならなにもせず
	jl	short S4q

	; 数字列を書き込んでゆく～
		; DXAX = input value   CH = unuse CL = radix
S4l:		; DS:SI = format   ES:DI = buf   BX = 桁
	call	ASM_LBDIV
	cmp	CH,10
	jl	short S4dec
	add	CH,7
S4dec:
	add	CH,'0'
 l_	<mov	ES:[DI+BX],CH>
 s_	<mov	[DI+BX],CH>
	dec	BX
	js	short S4q
	test	AX,AX
	jnz	short S4l
	cmp	AX,DX
	jne	short S4l

	; 余った桁を padcharで埋めてゆく～
	pop	AX		; padchar + 桁を AXに復帰
S4pad:
 l_	<mov	ES:[DI+BX],AH>	; pad char
 s_	<mov	[DI+BX],AH>
	dec	BX
	jns	short S4pad
	push	AX
S4q:
	pop	BX
	pop	CX		; CX = format len   SI = format
	xor	BH,BH
	add	DI,BX		; DI = buf
	jmp	near ptr Sloop

Sstr:	; 文字列(%s) 左詰めだぞ
				; DI = buf  BH = pad AH = 桁  AH=long or short
	push	CX
	push	SI
	mov	CH,0
	mov	CL,AH		; CX=桁

	_push	DS
	_lds	SI,[BP+param]
	s_inc	BP
	s_inc	BP
	_add	BP,4
	jmp	short SStrs
Sstrloop:
	dec	CX
	stosb			; ああ手抜き。これじゃあすげえ遅いよなあ
SStrs:
	lodsb			; でもサイズが小さいのでいいのだ
	test	AL,AL
	jne	short Sstrloop
Sstrfill:
	test	CX,CX
	jle	short Sstrend
	mov	AL,' '	; space
	rep	stosb
Sstrend:
	_pop	DS
	pop	SI
	pop	CX
	jmp	near ptr Sloop
endfunc

END
