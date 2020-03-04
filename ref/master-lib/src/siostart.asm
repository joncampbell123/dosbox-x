; master library - PC98
;
; Description:
;	RS-232Cライブラリの初期化／終了、割り込み処理
;
; Function/Procedures:
;	void sio_start( int port, int speed, int param, int flow ) ;
;	void sio_end(int port) ;
;	void sio_leave(int port) ;
;	void sio_setspeed( int port, int speed ) ;
;	void sio_enable( int port, ) ;
;	void sio_disable( int port ) ;
;
; Parameters:
;	speed	ボーレート。0=非設定, SIO_150～SIO_38400=設定
;	param	通信パラメータ。SIO_N81など
;	flow	フロー制御の方法。0 = none, 1 = RS/CS, 2 = XON/XOFF
;
; Returns:
;	none
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
;	sio_startを実行したら、アプリケーション終了前にsio_endを必ず呼ぶこと。
;	sio_startを、sio_endを実行する前に再実行すると、パラメータ類の変更を
;	行うのみとなる。
;	sio_endの代わりにsio_leaveを使うと各信号線OFFを行わない。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/12/07 Initial
;	93/ 1/31 sio_leave追加。75bps廃止。
;	93/ 2/12 sio_check_cs廃止(ってここじゃないけど,割り込みが変わったから)
;	93/ 2/17 割り込みマスクの退避/復元
;	93/ 2/19 sio_start, modeもいじらないモードつけた
;	93/ 7/19 割り込み許可(STI)を排除してpushf->cli->popfにした。
;	93/ 7/23 [M0.20] 引数port追加。中身はまだない。
;	93/ 9/11 [M0.21] 速度にに SIO_MIDI (SIO_MIDISPEED, 31250bps)追加。
;	93/ 9/11 [M0.21] ハイレゾに対応したつもり。未確認げろ
;			↑ダメ。こんどはどうだ?
;	93/10/28 [M0.21] sio_bit_offをbugfixしたら、こっちがたまたまうごいて
;			たのが動かなくなってた(^^; ので修正
;	95/5/29  [M0.23] 8MHz系ではこっそり上限を20800bpsにした
;			(SIO_19200を指定すると20800bpsになる)
;
	.MODEL SMALL
	include func.inc
	include sio.inc

	.DATA?
old_intvect	dd	?
old_intmask	db	?
old_intmask_pic	db	?

	.CODE
	EXTRN DOS_SETVECT:CALLMODEL
	EXTRN SIO_BIT_OFF:CALLMODEL


SIO_INT proc far	; {
	push	AX
	in	AL,SYSTEM_PORTC
	push	BX
	push	DS

	push	AX
	and	AL,not SIO_INTMASKS
	out	SYSTEM_PORTC,AL		; 通信割り込み禁止

	mov	AX,seg DGROUP
	mov	DS,AX

	in	AL,SIO_STAT
	test	AL,SIO_STAT_RXRDY
	jz	short SI_SENDF

	; 受信割り込み -----------------------------

	in	AL,SIO_DATA		; AL = read data

	cmp	sio_flow_type,SIO_FLOW_SOFT
	jne	short SIREC
	; ソフトウェアフロー制御のときは、XOFF/OFFの受信によって…
	cmp	AL,XOFF
	je	short REC_STOP

	cmp	sio_send_stop,1		; 止められていないなら、普通の受信へ
	jne	short SIREC
		; ｢動け｣
	cmp	AL,XON			; XOFFを受けて止まってるときは
	jne	short SI_SEND		; XON以外の受信文字は無視する

	pop	AX
	or	AL,SIO_TXRE_BIT		; 送信割り込み許可
	push	AX
	dec	sio_send_stop		; 送信可
	jmp	short SI_SEND
	EVEN

REC_STOP:	; ｢止まれ｣
	pop	AX
	and	AL,not SIO_TXRE_BIT	; 送信割り込み禁止
	push	AX
	mov	sio_send_stop,1		; 相手に止められた
	jmp	short SI_SEND
	EVEN

SIREC:	; 受信バッファ格納前の検査
	mov	BX,sio_ReceiveLen
	cmp	BX,FLOW_STOP_SIZE	; フロー開始?
	jb	short REC_STORE		; 少ないなら無条件に格納だい

	; 停止要求送出
	cmp	sio_flow_type,1		; SIO_FLOW_SOFT:2
	je	short REC_HARD		; SIO_FLOW_HARD:1
	jb	short REC_CHECKFULL	; SIO_FLOW_NO:0
		; ソフトフロー
		mov	sio_rec_stop,XOFF		; 停止要求
		jmp	short REC_CHECKFULL
		EVEN
REC_HARD:
		; ハードフロー
		push	AX
		xor	AX,AX
		push	AX
		mov	AL,SIO_CMD_RS
		push	AX
		call	SIO_BIT_OFF
		pop	AX
		mov	sio_rec_stop,1 ; 向こうに送信停止を要求したぞflag

REC_CHECKFULL:
	cmp	BX,RECEIVEBUF_SIZE
	je	short SI_SEND		; 満杯なら無視するんだ

REC_STORE:
	mov	BX,sio_receive_wp
	mov	[BX+sio_receive_buf],AL	; 格納
	RING_INC BX,RECEIVEBUF_SIZE,AX	; (受信)書き込みポインタ更新
	mov	sio_receive_wp,BX
	inc	sio_ReceiveLen

		; ここでまたステータス読んでるのは、ここまでの間に
		; 送信可能に変わったときにあわよくば…というわけ
SI_SEND:
	in	AL,SIO_STAT
SI_SENDF:
	test	AL,SIO_STAT_TXRDY	; 送信?
	jz	short SI_EXIT

	; 送信割り込み ----------------------
	cmp	sio_flow_type,SIO_FLOW_SOFT
	jne	short SI_SEND_1

	; ソフトフロー制御
SI_SEND_SOFT:
	mov	AX,sio_rec_stop
	cmp	AX,1
	jle	short SI_SEND_1
	; 割り込みたい文字があるなら強制送信
SI_XSEND:
	out	SIO_DATA,AL		; XON/XOFF送信だよん
	mov	sio_rec_stop,1
	cmp	AL,XOFF			; XOFFなら受信停止フラグを立てて、
	je	short SI_EXIT
	mov	sio_rec_stop,0		; XONならば受信許可なのよん
	jmp	short SI_EXIT
	EVEN

	; 送信バッファが空になったとき
SI_EMPTY:
	mov	sio_send_stop,2		; バッファ空
SI_STOP:
	mov	AL,20h			; EOI
	out	0,AL
	pop	AX
	and	AL,not SIO_TXRE_BIT	; 送信割り込み禁止
	out	SYSTEM_PORTC,AL		; 送信割り込みフラグ設定

	pop	DS
	pop	BX
	pop	AX
	iret

	EVEN
SI_SEND_1:
	xor	AX,AX
	cmp	sio_send_stop,AX
	jnz	short SI_STOP		; 停止中なら送らない
	cmp	sio_SendLen,AX
	je	short SI_EMPTY		; バッファが空ならおわりだぜ
	mov	BX,sio_send_rp		; 送信バッファから取ってくる
	mov	AL,[BX+sio_send_buf]	; バッファからALに取る
	out	SIO_DATA,AL		; 送信しちゃう
	RING_INC BX,SENDBUF_SIZE,AX	; ポインタ更新
	mov	sio_send_rp,BX
	dec	sio_SendLen
	je	short SI_EMPTY
	pop	AX
	or	AX,SIO_TXRE_BIT		; 送信許可
	push	AX
SI_EXIT:
	mov	AL,20h			; EOI
	out	0,AL
	pop	AX
	out	SYSTEM_PORTC,AL		; 送信割り込みフラグ設定

	pop	DS
	pop	BX
	pop	AX
	iret
SIO_INT endp		; }

func SIO_START		; sio_start() {
	push	BP
	mov	BP,SP
	pushf

	; 引数
	port	= (RETSIZE+4)*2
	speed	= (RETSIZE+3)*2
	param	= (RETSIZE+2)*2
	flow	= (RETSIZE+1)*2

	in	AL,SYSTEM_PORTC		; システムポートを読む
	and	AL,SIO_INTMASKS
	mov	CX,AX			; CL = 元の割り込みマスク

	push	[BP+port]
	call	CALLMODEL PTR SIO_DISABLE

	; ワークエリアをクリア
	xor	AX,AX
	mov	sio_send_rp,AX
	mov	sio_send_wp,AX
	mov	sio_SendLen,AX
	mov	sio_receive_rp,AX
	mov	sio_receive_wp,AX
	mov	sio_ReceiveLen,AX
	mov	sio_send_stop,2
	mov	sio_rec_stop,AX

	cmp	byte ptr sio_cmdbackup,AL
	je	short START_FIRST
	; ２度目以降

	; 速度の設定
	push	[BP+port]
	push	[BP+speed]
	call	CALLMODEL PTR SIO_SETSPEED
	jmp	short START_RESET
	EVEN

START_FIRST:
	; 割り込みベクタの登録
	mov	old_intmask,CL		; 割り込みマスクの退避
	in	AL,02h
	or	AL,not 00010000b	; 8251Aの割り込みマスクを保存
	mov	old_intmask_pic,AL

	mov	AX,SIO_INTVECT
	push	AX
	push	CS
	mov	AX,offset SIO_INT
	push	AX
	call	DOS_SETVECT
	mov	word ptr old_intvect,AX
	mov	word ptr old_intvect+2,DX

	; 速度の設定
	push	[BP+port]
	push	[BP+speed]
	call	CALLMODEL PTR SIO_SETSPEED

	; フロー制御方法の設定
	mov	AX,[BP+flow]
	mov	sio_flow_type,AX

	mov	AL,[BP+param]
	and	AL,11111100b		; モードワード
	jz	short SKIP_MODE
	mov	BL,AL

	; 8251をリセットする
	; 手順
	CLI
	xor	AX,AX
	out	SIO_CMD,AL
	jmp	$+2
	out	SIO_CMD,AL
	jmp	$+2
	out	SIO_CMD,AL
	jmp	$+2
START_RESET:
	CLI
	mov	AL,40h			; RESET
	out	SIO_CMD,AL
	jmp	$+2

	; モード
	mov	AL,BL
	or	AL,2			; x16
	out	SIO_MODE,AL
	jmp	$+2

SKIP_MODE:
	; コマンド
	mov	AL,00110111b		; ? RESET RS ERRCLR  SBRK RXEN ER TXEN
	out	SIO_CMD,AL
	mov	byte ptr sio_cmdbackup,AL

	in	AL,02h
	and	AL,not 00010000b	; 8251Aの割り込みを許可
	out	02h,AL

	popf
	push	[BP+port]
	call	CALLMODEL PTR SIO_ENABLE
	pop	BP
	ret	8
endfunc			; }

func SIO_END		; sio_end() {
	push	BP
	mov	BP,SP
	port = (RETSIZE+1)*2

	cmp	byte ptr sio_cmdbackup,0
	jne	short END_GO
	pop	BP
	ret	2
END_GO:
	push	[BP+port]
	call	CALLMODEL PTR SIO_DISABLE
	; 各制御線を OFF
	xor	AX,AX
	push	AX
	mov	AX,SIO_CMD_ER or SIO_CMD_RS or SIO_CMD_BREAK
	push	AX
	call	SIO_BIT_OFF
	jmp	short LEAVE_GO
endfunc			; }

func SIO_LEAVE		; sio_leave() {
	push	BP
	mov	BP,SP
	port = (RETSIZE+1)*2

	cmp	byte ptr sio_cmdbackup,0
	je	short LEAVE_IGNORE
	push	[BP+port]
	call	CALLMODEL PTR SIO_DISABLE
LEAVE_GO:
	pushf
	CLI
	in	AL,02h
	or	AL,00010000b		; 8251Aの割り込みをマスク
	out	02h,AL
	popf

	; 割り込みベクタの復元
	mov	AX,SIO_INTVECT
	push	AX
	push	word ptr old_intvect+2
	push	word ptr old_intvect
	call	DOS_SETVECT
	mov	sio_cmdbackup,0

	; 割り込みマスクの復元
	pushf
	CLI

	in	AL,SYSTEM_PORTC
	jmp	short $+2
	or	AL,old_intmask
	out	SYSTEM_PORTC,AL

	in	AL,02h
	and	AL,old_intmask		; 8251Aの割り込みマスクを復元
	out	02h,AL

	popf

LEAVE_IGNORE:
	pop	BP
	ret	2
endfunc			; }

func SIO_SETSPEED	; sio_setspeed() {
	mov	BX,SP
	; 引数
	port	= (RETSIZE+1)*2
	speed	= RETSIZE*2
	mov	AX,SS:[BX+speed]
	test	AX,AX
	jz	short SPEED_IGNORE

	mov	CX,AX

	xor	BX,BX
	mov	ES,BX
	test	byte ptr ES:[0501H],80h
	jz	short SPEED_5MHz	; 5MHz系? 8MHz系?
	; 8MHz系(タイマは2MHz)
	mov	AX,8		; 8(20800bps)を上限にする
	mov	BX,1664
	mov	DX,4		; RS-MIDI 31250bps近似値用
	jmp	short SPEED_1
SPEED_5MHz:
	; 5MHz系(タイマは2.5MHz)
	mov	AX,9		; 9(38400bps)を上限にする
	mov	BX,2048
	mov	DX,5		; RS-MIDI 31250bps近似値用
SPEED_1:
	cmp	CX,SIO_MIDISPEED
	je	short MIDI
	sub	CX,AX		; CX = min(CX,AX)
	sbb	DX,DX
	and	CX,DX
	add	CX,AX
	shr	BX,CL		; BX = タイマ#2の分周値
	mov	DX,BX
MIDI:

	pushf
	CLI
	mov	AL,0b6h		; カウンタ#2をモード３に
	out	77h,AL
	jmp	$+2
	jmp	$+2
	mov	AL,DL
	out	75h,AL		; 下位
	jmp	$+2
	jmp	$+2
	mov	AL,DH
	out	75h,AL		; 上位
	popf
SPEED_IGNORE:
	ret 4
endfunc			; }

func SIO_ENABLE		; sio_enable() {
	pushf
	CLI
	in	AL,SYSTEM_PORTC
	jmp	$+2
	and	AL,not SIO_INTMASKS
	or	AL,SIO_TXRE_BIT or SIO_RXRE_BIT
	out	SYSTEM_PORTC,AL
	popf
	ret	2
endfunc			; }

func SIO_DISABLE	; sio_disable() {
	pushf
	CLI
	in	al,SYSTEM_PORTC		; システムポートを読む
	jmp	$+2
	and	al,not SIO_INTMASKS
	out	SYSTEM_PORTC,al		; RS-232Cの全割り込みを禁止
	popf
	ret	2
endfunc			; }

END
