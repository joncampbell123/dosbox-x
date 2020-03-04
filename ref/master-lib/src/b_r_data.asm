; master library - BGM
;
; Description:
;	BGMファイルを読み込む
;
; Function/Procedures:
;	int bgm_read_data(const char *fname, int tempo, int mes);
;
; Parameters:
;	fname		ファイルネーム
;	tempo		テンポ(ファイル中にテンポ情報がない場合に有効)
;	mes		メッセージ表示フラグ
;
; Returns:
;	BGM_COMPLETE	正常終了
;	BGM_FILE_ERR	ファイルがない
;	BGM_FORMAT_ERR	フォーマットが異常
;	BGM_OVERFLOW	メモリが足りない
;	BGM_TOOMANY	登録できる最大曲数を超えた
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;
;
; Assembly Language Note:
;
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	femy(淀  文武)		: オリジナル・C言語版
;	steelman(千野  裕司)	: アセンブリ言語版
;
; Revision History:
;	93/12/19 Initial: b_r_data.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL
	EXTRN	DOS_READ:CALLMODEL
	EXTRN	DOS_CLOSE:CALLMODEL
	EXTRN	DOS_SEEK:CALLMODEL
	EXTRN	DOS_ROPEN:CALLMODEL
	EXTRN	DOS_PUTC:CALLMODEL

	.DATA
	EXTRN	glb:WORD	;SGLB
	EXTRN	part:WORD	;SPART

	.CODE
	EXTRN	_BGM_MGET:CALLMODEL
	EXTRN	_BGM_PINIT:CALLMODEL
	EXTRN	BGM_SET_TEMPO:CALLMODEL

MRETURN macro
	pop	SI
	pop	DI
	leave
	ret	(DATASIZE+2)*2
	EVEN
endm

func BGM_READ_DATA
	enter	20,0	; 14+PMAX*2
	push	DI
	push	SI

	fname	= (RETSIZE+3)*2
	tempo	= (RETSIZE+2)*2
	mes	= (RETSIZE+1)*2
	c	= -1
	tmp	= -4
	maxpart	= -8
	pcount	= -10
	bufp	= -14
	cnt	= bufp-(PMAX*2)
;	register SI = retcode
;	register BX = bptr
;	register ES = segmet bufp

	CLD

	;指定したファイルがなければファイルエラー
	_push	[BP+fname+2]
	push	[BP+fname]
	call	DOS_ROPEN
	jnc	short OPENED
	jmp	short RETURN
NOMEM:
	push	SI
	call	DOS_CLOSE
RETURN:
	MRETURN
OPENED:
	mov	SI,AX
	mov	DI,glb.bufsiz
	shl	DI,1
	add	DI,glb.bufsiz			;glb.bufsiz*3
	push	DI
	call	SMEM_WGET
	jc	short NOMEM
	mov	[BP+bufp+2],AX
	mov	word ptr [BP+bufp],0
	;bufp[dos_read(tmp,bufp,glb.bufsiz * PMAX)] = 255;
	push	SI
	push	AX
	push	0
	push	DI
	call	DOS_READ
	les	BX,[BP+bufp]
	mov	DI,AX
	mov	byte ptr ES:[BX][DI],0ffh	;終わりに0ffhを入れる
	push	SI
	call	DOS_CLOSE

READ_START:
	xor	AX,AX
	mov	CX,PMAX
	lea	DI,[BP+cnt]
	push	SS
	pop	ES
	rep	stosw
	mov	[BP+pcount],AX

	;ファイル解析
START_ANALISYS:
	les	BX,[BP+bufp]
	inc	word ptr [BP+bufp]
	mov	AL,ES:[BX]
	mov	[BP+c],AL
	cmp	AL,0ffh
	jne	short SWITCH
	jmp	END_ANALISYS

SWITCH:
	;AL = c;
	sub	AH,AH
	cmp	AX,'*'
	jne	short SWITCH1
	jmp	END_ANALISYS
SWITCH1:
	;switch (c) {
	cmp	AX,';'
	je	short COMMENTLINE
	ja	short SKIP_SPACE
	cmp	AL,'#'
	je	short COMMENTLINE
	jg	short SWITCH2
	sub	AL,10	;\n
	je	short LINEEND
	sub	AL,'"' - 10
	je	short MESSAGELINE

	;空白を飛ばす
SKIP_SPACE:
	cmp	byte ptr [BP+c],' '
	je	short BREAK
	cmp	byte ptr [BP+c],9	;TAB
	je	short BREAK

	;デフォルト
	;if ((glb.buflast + cnt[pcnt]) < glb.bufsiz) {
	mov	SI,[BP+pcount]
	shl	SI,1
	lea	BX,[BP+cnt][SI]
	mov	SI,BX
	mov	AX,SS:[BX]
	add	AX,glb.buflast
	cmp	AX,glb.bufsiz
	jl	short PUTBUFFER
	jmp	short BREAK
PUTBUFFER:
	;part[pcnt].mbuf[glb.buflast + cnt[pcnt]++] = (uchar)c;
	mov	AL,[BP+c]
	imul	BX,word ptr [BP+pcount],type SPART
	les	BX,part[BX].mbuf
	add	BX,SS:[SI]
	mov	DI,glb.buflast
	mov	ES:[BX][DI],AL
	inc	word ptr SS:[SI]
	;part[pcnt].mbuf[glb.buflast + cnt[pcnt]] = 0;
	mov	byte ptr ES:[BX][DI+1],0

	;1キャラ解析終了
BREAK:
	cmp	byte ptr [BP+c],0ffh
	je	short END_ANALISYS		;1曲読んだぜ!!
	jmp	START_ANALISYS

SWITCH2:
	sub	AL,','
	je	short PARTEND
	sub	AL,14
	jne	short SKIP_SPACE
	jmp	short PARTEND

	;1行終わり
LINEEND:
	mov	word ptr [BP+pcount],0
	jmp	short BREAK

	;コメント
COMMENTLINE:
	push	DS
	lds	SI,[BP+bufp]
COMMENTLOOP:
	lodsb
	cmp	AL,0ffh
	je	short COMMENTEND
	cmp	AL,10		;\n
	jne	short COMMENTLOOP
COMMENTEND:
	pop	DS
	mov	[BP+c],AL
	mov	[BP+bufp],SI
	jmp	short BREAK

	;メッセージ
MESSAGELINE:
	mov	DI,[BP+mes]
	push	DS
	lds	SI,[BP+bufp]
MESSAGELOOP:
	lodsb
	cmp	AL,0ffh
	je	short MESSAGELINEEND
	cmp	AL,10		;\n
	je	short MESSAGELINEEND
	cmp	AL,'"'
	je	short MESSAGELINEEND
	cmp	DI,BGM_MES_ON
	jne	short MESSAGELOOP
	sub	AH,AH
	push	AX
	call	DOS_PUTC
	jmp	short MESSAGELOOP
MESSAGELINEEND:
	pop	DS
	mov	[BP+c],AL
	mov	[BP+bufp],SI
	cmp	DI,BGM_MES_ON
	jne	short BREAK
	push	13		;\r
	call	DOS_PUTC
	push	10		;\n
	call	DOS_PUTC
	jmp	short BREAK

	;1パート終わり
PARTEND:
	inc	word ptr [BP+pcount]
	cmp	word ptr [BP+pcount],PMAX
	jne	short BREAK
	mov	word ptr [BP+pcount],PMAX-1
	jmp	short BREAK




	;BREAKから飛んで来る
	;1曲読み込みおわったんだね
END_ANALISYS:
	mov	word ptr [BP+maxpart],0
	mov	word ptr [BP+tmp],offset part.mbuf
	lea	SI,[BP+cnt]

	;曲の長さを求める
GETMUSICLENGTH:
	;for (pcnt = 0; pcnt < PMAX; pcnt++) {
	mov	AX,SS:[SI]
	cmp	[BP+maxpart],AX
	jge	short LESSTHANMAXPART
	mov	[BP+maxpart],AX
	;bptr = part[pcnt].mbuf + glb.buflast;
LESSTHANMAXPART:
	;while (*bptr) {
	mov	DI,[BP+tmp]
	mov	AX,[DI]
	mov	DX,[DI+2]
	add	AX,glb.buflast
	mov	BX,AX
	mov	ES,DX
	mov	DI,AX
	cmp	byte ptr ES:[DI],0
	je	short BPTRISZERO
	mov	[BP-6],SI
STRUPPER:
	;if (*bptr >= 'a' && *bptr <= 'z') {
	cmp	byte ptr ES:[BX],'a'
	jb	short OOMOJI
	cmp	byte ptr ES:[BX],'z'
	ja	short OOMOJI
	sub	byte ptr ES:[BX],('a' - 'A')
OOMOJI:
	;bptr++;
	inc	BX
	cmp	byte ptr ES:[BX],0
	jne	short STRUPPER
	mov	SI,word ptr [BP-6]
	;for (pcnt = 0; pcnt < PMAX; pcnt++) {
BPTRISZERO:
	add	word ptr [BP+tmp],type SPART
	add	SI,2
	lea	AX,[BP+bufp]
	cmp	SI,AX
	jb	short GETMUSICLENGTH

	;曲数オーバーチェック
	inc	glb.mnum
	cmp	glb.mnum,MMAX
	jle	short NO_OVERMUSIC
	mov	SI,BGM_TOOMANY
	jmp	END_READ

NO_OVERMUSIC:
	mov	BX,[BP+maxpart]
	;楽譜バッファオーバーフローチェック
	;if (glb.buflast + maxpart == glb.bufsiz) {
	mov	AX,glb.buflast
	add	AX,BX
	cmp	AX,glb.bufsiz
	jne	short NO_OVERFLOW
	mov	SI,BGM_OVERFLOW
	jmp	END_READ
NO_OVERFLOW:
	;glb.track[glb.mnum - 1] = glb.buflast;
	mov	AX,glb.buflast
	mov	SI,glb.mnum
	dec	SI
	shl	SI,1
	mov	glb.track[SI],AX
	;glb.buflast += (maxpart + 1);
	lea	AX,[BX+1]
	add	glb.buflast,AX

	;パート初期化
	;for (pcnt = 0; pcnt < PMAX; pcnt++) {
	xor	SI,SI
	mov	DI,offset part
	mov	[BP+tmp],DI
INITPART:
	;bgm_pinit(part + pcnt);
	push	DI
	call	_BGM_PINIT
	;part[pcnt].mask = ((glb.mask & (1 << pcnt)) ? ON : OFF);
	mov	CX,SI
	mov	AX,1
	shl	AX,CL
	and	AX,glb.pmask
	cmp	AX,1
	sbb	AX,AX
	inc	AX
	mov	[DI].msk,AX
	inc	SI
	add	DI,type SPART
	cmp	DI,offset part+(type SPART*3)
	jb	short INITPART

	;音符セット
	push	offset part
	call	_BGM_MGET
	mov	SI,BGM_FORMAT_ERR		; 先にいれとく(笑)
	or	AX,AX
	je	short END_READ
	push	offset part+type SPART
	call	_BGM_MGET
	push	offset part+(type SPART*2)
	call	_BGM_MGET

	;テンポ設定
	mov	DI,[BP+tempo]
	cmp	DI,TEMPOMIN
	jl	short INVALID_TEMPO
	cmp	DI,TEMPOMAX
	jg	short INVALID_TEMPO
	;glb.mtp[glb.mnum - 1] = glb.tp = tempo;
	mov	glb.tp,DI
	mov	BX,glb.mnum
	dec	BX
	shl	BX,1
	mov	glb.mtp[BX],DI
	;glb.tval = (uint)(glb.clockbase / (ulong)glb.tp);
	mov	DX,word ptr glb.clockbase+2
	mov	AX,word ptr glb.clockbase
	div	DI
	mov	glb.tval,AX
INVALID_TEMPO:
	xor	SI,SI			; BGM_COMPLETE
	cmp	byte ptr [BP+c],0ffh
	je	short END_READ
	jmp	READ_START		; まだ次の曲がある

END_READ:
	push	[BP+bufp+2]
	call	SMEM_RELEASE
	mov	AX,SI
	MRETURN
endfunc
END
