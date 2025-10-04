; master library - BGM
;
; Description:
;
;
; Function/Procedures:
;	int _bgm_play(int sp, int ep, int rp);
;
; Parameters:
;
;
; Returns:
;
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
;	恋塚
;
; Revision History:
;	93/12/19 Initial: b_play.asm / master.lib 0.22 <- bgmlibs.lib 1.12
;	94/ 4/11 [M0.23] AT互換機対応

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB
	EXTRN	part:WORD	;SPART
	EXTRN	note_dat:WORD

	EXTRN	Machine_State:WORD

	.CODE
	EXTRN	_BGM_MGET:CALLMODEL
	EXTRN	_BGM_PINIT:CALLMODEL

BGM_BELL:
	push	SI
	mov	SI,BX

	;マスクON ならリターン
	cmp	[SI].pmask,ON
	je	short EXIT
	;spval = (ulong)(note_dat[part2->note - 'A'] * 2);
	mov	BX,[SI].note
	shl	BX,1
	mov	BX,note_dat[BX-('A'*2)]

	cmp	[SI].oct,1
	jne	short NOTOCT1
	shl	BX,1
	jmp	short OCT1
NOTOCT1:
	;spval = (ulong)(note_dat[part2->note - 'A'] / octdat[part2->oct - 1]);
	mov	CX,[SI].oct
	dec	CX
	dec	CX
	shr	BX,CL
OCT1:
	test	Machine_State,10h	; PC/AT
	jz	short PC98
PCAT:	in	AL,61h	;ビープON
	or	AL,3
	out	61h,AL			; AT
	mov	AX,BX
	mov	BX,TVALATORG/2
	mov	CX,BEEP_CNT_AT
	jmp	short DO_SCALE
	EVEN
PC98:
	;ビープON	(上にもってきたけど大丈夫かなあ)
	mov	AL,BEEP_ON
	out	BEEP_SW,AL		; 98

	mov	CX,BEEP_CNT
	;BX = spval
	;8MHz なら 998/1229倍する
	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[0501H],80h
	jz	short CLOCK10MHZ
	;spval = spval * 998UL / 1229UL;
	mov	AX,BX
	mov	BX,TVAL8ORG/2
DO_SCALE:
	mul	BX
	mov	BX,TVAL10ORG/2
	div	BX
	mov	BX,AX
CLOCK10MHZ:
	;タイマカウント値設定
	mov	AL,BL
	mov	DX,CX
	out	DX,AL			; 98,AT
	mov	AL,BH
	out	DX,AL			; 98,AT
	;ビープON
;	mov	AL,BEEP_ON
;	out	BEEP_SW,AL
EXIT:
	pop	SI
	ret

func _BGM_PLAY
	enter	4,0
	push	DI
	push	SI
	_sp	= (RETSIZE+3)*2
	_ep	= (RETSIZE+2)*2
	_rp	= (RETSIZE+1)*2
;	register DI = pcnt
;	register SI = mute

	mov	AX,glb.pcnt
	imul	BX,AX,type SPART
	cmp	part[BX].lcnt,MINLCNT
	jne	short MUTEISFALSE
	cmp	part[BX].tnt,ON
	je	short MUTEISFALSE
	cmp	part[BX].len,MINNOTE
	je	short MUTEISFALSE
	mov	SI,TRUE
	jmp	short MUTEISTRUE
MUTEISFALSE:
	xor	SI,SI		; FALSE
MUTEISTRUE:
	;switch (glb.pcnt - sp) {
	mov	CX,[BP+_sp]
	sub	AX,CX
	je	short PART0
	dec	AX
	je	short PART1
	dec	AX
	je	short PART2
	jmp	short NOBELL

	;パート0
even
PART0:
	imul	BX,CX,type SPART
	cmp	byte ptr part[BX].note,REST
	je	short PART0_NOT_BELL_PART0
	or	SI,SI
	je	short BELL_PART0
PART0_NOT_BELL_PART0:
	cmp	byte ptr part[BX+type SPART].note,REST
	je	short PART0_NOT_BELL_PART1
	cmp	part[BX+type SPART].lcnt,MINLCNT
	jg	short BELL_PART1
PART0_NOT_BELL_PART1:
	cmp	byte ptr part[BX+type SPART*2].note,REST
	je	short BELL_PART0
	cmp	part[BX+type SPART*2].lcnt,MINLCNT
	jg	short BELL_PART2

	;パート0のベルを鳴らす
even
BELL_PART0:
	add	BX,offset part
	jmp	short BELL


	;パート1
even
PART1:
	imul	BX,CX,type SPART
	cmp	byte ptr part[BX+type SPART].note,REST
	je	short PART1_NOT_BELL_PART1
	or	SI,SI
	je	short BELL_PART1
PART1_NOT_BELL_PART1:
	cmp	byte ptr part[BX].note,REST
	jne	short NOBELL
	cmp	byte ptr part[BX+type SPART*2].note,REST
	je	short NOBELL
	cmp	part[BX+type SPART*2].lcnt,MINLCNT
	jle	short NOBELL
	jmp	short BELL_PART2

	;パート1のベルを鳴らす
even
BELL_PART1:
	add	BX,offset part+type SPART
	jmp	short BELL


	;パート2
even
PART2:
	imul	BX,CX,type SPART
	cmp	byte ptr part[BX+type SPART*2].note,REST
	je	short NOBELL
	or	SI,SI
	jne	short NOBELL

	;パート2のベルを鳴らす
even
BELL_PART2:
	add	BX,offset part+(type SPART*2)
even
BELL:
	call	BGM_BELL
even
NOBELL:


	;音長カウンタデクリメント
	imul	BX,glb.pcnt,type SPART
	dec	part[BX].lcnt
	jne	short NOTPARTEND
	;パート終了チェック
	add	BX,offset part
	push	BX
	call	_BGM_MGET
	or	AX,AX
	jne	short NOTPARTEND
	mov	CL,byte ptr glb.pcnt
	inc	AX
	shl	AX,CL
	or	glb.fin,AX
NOTPARTEND:

	;次のパートに移す
	mov	AX,word ptr [BP+_ep]
	inc	glb.pcnt
	cmp	glb.pcnt,AX
	jne	short NOTALLPARTEND
	mov	AX,word ptr [BP+_rp]
	mov	glb.pcnt,AX

	;全てのパートが終了したらパート初期化
	cmp	glb.fin,ALL_PART
	jne	short NOTALLPARTEND

	xor	DI,DI
	mov	glb.fin,DI
	mov	SI,offset part
even
PARTINITLOOP:
	push	SI
	call	_BGM_PINIT
	;part[pcnt].mask = ((glb.mask & (1 << pcnt)) ? ON : OFF);
	mov	CX,DI
	mov	AX,1
	shl	AX,CL
	and	AX,glb.pmask
	cmp	AX,1
	sbb	AX,AX
	inc	AX
	mov	[SI].msk,AX
	push	SI
	call	_BGM_MGET
	inc	DI
	add	SI,type SPART
	cmp	SI,offset part+(type SPART*PMAX)
	jb	short PARTINITLOOP

	mov	AX,FINISH
	pop	SI
	pop	DI
	leave
	ret	6
even
NOTALLPARTEND:
	xor	AX,AX			;NOTFIN
	pop	SI
	pop	DI
	leave
	ret	6
endfunc
END
