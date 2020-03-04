include super.inc
BGM_STAT_PLAY	equ	1	;リターンコード
BGM_STAT_MUTE	equ	0
BGM_STAT_REPT	equ	1
BGM_STAT_1TIM	equ	0
BGM_STAT_ON	equ	1
BGM_STAT_OFF	equ	0
BGM_MES_ON	equ	1	;パラメータ
BGM_MES_OFF	equ	0
BGM_MUSIC	equ	1
BGM_SOUND_	equ	2
; ↑'BGM_SOUND'より変更
BGM_COMPLETE	equ	NoError ; エラーナンバー
BGM_FILE_ERR	equ	FileNotFound
BGM_FORMAT_ERR	equ	InvalidFormat
BGM_OVERFLOW	equ	InsufficientMemory
BGM_TOOMANY	equ	InvalidData
BGM_NO_MUSIC	equ	InvalidData
BGM_NOT_PLAY	equ	GeneralFailure
BGM_NOT_STOP	equ	GeneralFailure
BGM_EXTENT_ERR	equ	InvalidData

ON		equ	1
OFF		equ	0
TRUE		equ	1
FALSE		equ	0
FINISH		equ	1
NOTFIN		equ	0
SEND		equ	0
VEND		equ	0
BEEP_SW		equ	37h	;ポートアドレス・設定値
BEEP_ON		equ	06h
BEEP_OFF	equ	07h
BEEP_CNT	equ	3fdbh
BEEP_MODE	equ	3fdfh
BEEP_CNT_AT	equ	42h
BEEP_MODE_AT	equ	43h
INTCTRL		equ	00h
EOI		equ	20h
IMR		equ	02h
TIMERMASK	equ	01h
TIMER_CNT	equ	71h
TIMER_SET	equ	77h
CLOCK_CHK	equ	42h
PMAX		equ	3	;パート数
ALL_PART	equ	7	;演奏終了チェック用
BUFMAX		equ	4096	;１パートのバッファ長
SBUFMAX		equ	256	;効果音１音のバッファ長
MINNOTE		equ	32	;最小音符
MINLCNT		equ	1	;最小カウントデータ
DEFTEMPO	equ	120	;デフォルトテンポ
TEMPOMAX	equ	240	;最大テンポ
TEMPOMIN	equ	30	;最小テンポ
DEFOCT		equ	4	;デフォルトオクターブ
DEFLEN		equ	4	;デフォルト音長
DEFLCNT		equ	8	;デフォルト音長カウント値
DIVNUM		equ	8	;1オクターブの音数
REST		equ	'H'	;休符
UNDF		equ	100	;未定義音符
MMAX		equ	16	;最大曲数
SMAX		equ	16	;最大効果音数
TIMER		equ	1	;タイマ	=	1ms
TVAL8ORG	equ	1997	;タイマ設定値	8MHz(1msec)
TVAL10ORG	equ	2458	;タイマ設定値	10MHz(1msec)
TVALATORG_RTC	equ	16	;タイマ設定値   8192Hz*2(AT互換機) (1msec)
TVALATORG	equ	1193	;タイマ設定値  1193.18K(AT互換機) (1msec)
TVAL8ORG2	equ	19968	;タイマ設定値	8MHz(10msec)
TVAL10ORG2	equ	24576	;タイマ設定値	10MHz(10msec)
;TVALATORG2	equ	41	;タイマ設定値   4096Hz(AT互換機) (10msec)
;TVALATORG2	equ	11931	;タイマ設定値  1193.18K(AT互換機) (10msec)
TCNTMAX		equ	20	;割り込み何回に	1回処理するか

;
;ステータス構造体
;
SBSTAT		struc
bmusic		dw	?	;演奏処理 ON/OFF
bsound		dw	?	;効果音処理 ON/OFF
bplay		dw	?	;演奏中か否か
beffect		dw	?	;効果音出力中か否か
brepeat		dw	?	;リピート演奏か否か
bmnum		dw	?	;登録曲数
brnum		dw	?	;セレクト中曲番号
btempo		dw	?	;現在のテンポ
bsnum		dw	?	;登録効果音数
bfnum		dw	?	;セレクト中効果音番号
SBSTAT		ends

;
;グローバルデータ構造体
;
SGLB		struc
simr		dw	?	;interrupt mask register
; ↑'imr'より変更
tval		dw	?	;タイマ設定値
tp		dw	?	;テンポ
rflg		dw	?	;BGM ON/OFF
pnum		dw	?	;パート数
pcnt		dw	?	;パートカウンタ
fin		dw	?	;パート終了フラグ
repsw		dw	?	;リピート ON/OFF
; ↑'rep'より変更
tcnt		dw	?	;処理カウンタ(4回に 1回処理)
bufsiz		dw	?	;楽譜バッファのサイズ
buflast		dw	?	;楽譜バッファのラスト
mnum		dw	?	;登録曲数
mcnt		dw	?	;セレクト中曲番号
track		dw	MMAX dup(?)	;曲のトラックナンバー
pmask		dw	?	;パートマスク情報
; ↑'mask'より変更
mtp		dw	MMAX dup(?)	;曲ごとのテンポ情報
effect		dw	?	;効果音 ON/OFF
snum		dw	?	;登録効果音数
scnt		dw	?	;セレクト中効果音番号
music		dw	?	;演奏処理 ON/OFF
sound		dw	?	;効果音処理 ON/OFF
init		dw	?	;イニシャライズ実行フラグ
clockbase	dd	?	;tempo120時のタイマカウント
SGLB		ends

;
;パート構造体
;
SPART struc
pptr		dd	?	;楽譜バッファポインタ
; ↑'ptr'より変更
mbuf		dd	?	;楽譜バッファ
note		dw	?	;現在の音符
oct		dw	?	;現在のオクターブ
len		dw	?	;現在の音長
dflen		dw	?	;デフォルトの音長
lcnt		dw	?	;音長カウンタ
msk		dw	?	;パートマスク ON/OFF
; ↑'mask'より変更
tnt		dw	?	;テヌート ON/OFF
SPART ends

;
;効果音構造体
;
SESOUND struc
sptr		dd	?	;効果音データポインタ
sbuf		dd	?	;効果音データバッファ
SESOUND ends
