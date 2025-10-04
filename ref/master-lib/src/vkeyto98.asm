; master library - key - PC/AT
;
; Description:
;	key code 変換 ( PC/AT→PC-98 )
;
; Procedures/Functions:
;	unsigned vkey_to_98( unsigned long pcat_key ) ;
;
; Returns:
;	PC-98 key code
;	・98なら下位16bitをそのまま返します。
;	・AT互換機なら、引数が0なら-1を、そうでなければ98キーに変換して
;	　返します。
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT , PC-98
;
; Author:
;	のろ/V
;
; Revision History:
;	93/ 9/ 4 Initial
;	93/ 9/ 9 [shift]+[↑][↓][←][→]
;	93/12/13 CTRL+カーソルキーなどに対応 (by 恋塚)
;	93/12/13 未定義コード 0xffff を 0x0000 に.
;	94/ 4/10 Initial: vkeyto98.asm/master.lib 0.23
;	94/ 4/10 AT互換機の場合、引数が 0 ならば -1 を返すようにした
;	94/11/23 拡張コード 00/E0 の場合、00/00 としていた(バグ)が
;		 漢字コードとして 00/E0 を返すようにした。
;		 上記以外の ??/E0 は、00/00 を返す。
;	95/ 1/27 [M0.23] HOMEキーのテーブルがコメントなっていたため、それ以後
;			 ぜんぶずれていた。0.22i時点では正しかった。いつのまに?
;	95/ 3/15 [M0.22k] shift+Tabが変換されてなかったので処理追加
;	95/ 3/17 [M0.22k] シフト類にかかわらず BackSpaceをK_BSに変換する。
;			  同じくTabをK_TABに変換する。

	.MODEL SMALL
	include func.inc

	.DATA

;			PC-98	  PC/AT  KEY
key_code_98	dw	0101h	; 3B00	[F1]
		dw	0102h	; 3C00	[F2]
		dw	0103h	; 3D00	[F3]
		dw	0104h	; 3E00	[F4]
		dw	0105h	; 3F00	[F5]
		dw	0106h	; 4000	[F6]
		dw	0107h	; 4100	[F7]
		dw	0108h	; 4200	[F8]
		dw	0109h	; 4300	[F9]
		dw	010ah	; 4400	[F10]
		dw	0000h	; 45	?
		dw	0000h	; 46	?
		dw	0019h	; 47E0	[HOME]	, \[HOME]
		dw	0005h	; 48E0	[↑]	, 0012h	; 48E0	\[↑]
		dw	0012h	; 49E0	[PUP]	, 0000h; 49E0	\[PUP]
		dw	002dh	; 4A	[-] , @[-]
		dw	0013h	; 4BE0	[←]	, 0001h	; 4BE0	\[←]
		dw	0000h	; 4C	?
		dw	0004h	; 4DE0	[→]	, 0006h	; 4DE0	\[→]
		dw	002bh	; 4E	[+] , @[+]
		dw	0100h	; 4FE0	[END] , \[END]
		dw	0018h	; 50E0	[↓]	, 0003h	; 50E0	\[↓]
		dw	0003h	; 51E0	[PDN] ,	\[PDN]
		dw	0016h	; 52E0	[INS] ,	\[INS]
		dw	0007h	; 53E0	[DEL] , \[DEL]
		dw	010bh	; 5400	\[F1]
		dw	010ch	; 5500	\[F2]
		dw	010dh	; 5600	\[F3]
		dw	010eh	; 5700	\[F4]
		dw	010fh	; 5800	\[F5]
		dw	0110h	; 5900	\[F6]
		dw	0111h	; 5A00	\[F7]
		dw	0112h	; 5B00	\[F8]
		dw	0113h	; 5C00	\[F9]
		dw	0114h	; 5D00	\[F10]
		dw	012ah	; 5E00	^[F1]
		dw	012bh	; 5F00	^[F2]
		dw	012ch	; 6000	^[F3]
		dw	012dh	; 6100	^[F4]
		dw	012eh	; 6200	^[F5]
		dw	012fh	; 6300	^[F6]
		dw	0130h	; 6400	^[F7]
		dw	0131h	; 6500	^[F8]
		dw	0132h	; 6600	^[F9]
		dw	0133h	; 6700	^[F10
		dw	0000h	; 6800	@[F1]
		dw	0000h	; 6900	@[F2]
		dw	0000h	; 6A00	@[F3]
		dw	0000h	; 6B00	@[F4]
		dw	0000h	; 6C00	@[F5]
		dw	0000h	; 6D00	@[F6]
		dw	0000h	; 6E00	@[F7]
		dw	0000h	; 6F00	@[F8]
		dw	0000h	; 7000	@[F9]
		dw	0000h	; 7100	@[F10
		dw	0000h	; 7200	^[PRINTSCREEN]
		dw	0213h	; 73E0	^[←]
		dw	0204h	; 74E0	^[→]
		dw	0000h	; 75E0	^[END]
		dw	0203h	; 76E0	^[PDN]
		dw	0000h	; 77E0	^[HOME]
		dw	0000h	; 78	@[1]
		dw	0000h	; 79	@[2]
		dw	0000h	; 7A	@[3]
		dw	0000h	; 7B	@[4]
		dw	0000h	; 7C	@[5]
		dw	0000h	; 7D	@[6]
		dw	0000h	; 7E	@[7]
		dw	0000h	; 7F	@[8]
		dw	0000h	; 80	@[9]
		dw	0000h	; 81	@[0]
		dw	0000h	; 82	@[-]
		dw	0000h	; 83	@[^]
		dw	0212h	; 84E0	^[PUP]
		dw	0120h	; 8500	[F11]
		dw	0121h	; 8600	[F12]
		dw	0125h	; 8700	\[F11]
		dw	0126h	; 8800	\[F12]
		dw	0134h	; 8900	^[F11]
		dw	0135h	; 8A00	^[F12]
		dw	0000h	; 8B00	@[F11]
		dw	0000h	; 8C00	@[F12]
		dw	0205h	; 8DE0	^[↑]
		dw	0000h	; 8E	^[-]
		dw	0000h	; 8F	?
		dw	0000h	; 90	^[+]
		dw	0218h	; 91E0	^[↓]
		dw	0216h	; 92E0	^[INS]
		dw	0207h	; 93E0	^[DEL]
		dw	0009h	; 94	^[TAB]
		dw	0000h	; 95
		dw	0000h	; 96	^[*]
		dw	0000h	; 9700	@[HOME]
		dw	0305h	; 9800	@[↑]
		dw	0312fh	; 9900	@[PUP]
		dw	0000h	; 9A
		dw	0313h	; 9B00	@[←]
		dw	0000h	; 9C
		dw	0304h	; 9D00	@[→]
		dw	0000h	; 9E
		dw	0000h	; 9F00	@[END]
		dw	0318h	; A000	@[↓]
		dw	0303h	; A100	@[PDN]
		dw	0316h	; A200	@[INS]
		dw	0307h	; A300	@[DEL]
		dw	0000h	; A4
		dw	0009h	; A5	@[TAB]
		dw	0000h	; A6
		dw	0000h	; A7
		dw	0000h	; A8
		dw	0000h	; A9
		dw	0000h	; AA
		dw	0000h	; AB
		dw	0000h	; AC
		dw	0000h	; AD
		dw	0000h	; AE
		dw	0000h	; AF
		dw	0000h	; B0
		dw	0000h	; B1
		dw	0000h	; B2
		dw	0000h	; B3
		dw	0000h	; B4
		dw	0000h	; B5
		dw	0000h	; B6

	EXTRN Machine_State : WORD

	.CODE

func VKEY_TO_98	; vkey_to_98() {
	CLI
	add	SP,RETSIZE*2
	pop	AX	; key(Low)
	pop	CX	; key(High)
	sub	SP,(RETSIZE+2)*2
	STI

	test	Machine_State,0010h	; PC/AT だったら
	jz	short KEY_CHANGE_EXIT

	cmp	AH,0eh
	je	short BackSpace
	cmp	AH,0fh
	je	short Tab

	or	AL,AL		; cmp al,00
	je	short ZERO_TESTER

	cmp	AX,000e0h
	je	short KEY_CHANGE_EXIT
	cmp	AL,0e0h
	je	short GO_CHANGE_KEY
	mov	AH,0
	jmp	short KEY_CHANGE_EXIT

ZERO_TESTER:
	mov	DX,AX
	or	DX,CX
	jnz	short GO_CHANGE_KEY
	mov	AX,-1
	ret	4

GO_CHANGE_KEY:
	test	CL,03
	jz	short NO_SHIFT_KEY
	cmp	AH,48h		; 0012h \[↑]
	je	short SHIFT_UP
	cmp	AH,4Bh		; 0001h	\[←]
	je	short SHIFT_LEFT
	cmp	AH,4Dh		; 0006h \[→]
	je	short SHIFT_RIGHT
	cmp	AH,50h		; 0003h	\[↓]
	jne	short NO_SHIFT_KEY
SHIFT_DOWN:
	mov	AX,0003h
	jmp	short KEY_CHANGE_EXIT
	even
SHIFT_UP:
	mov	AX,0012h
	jmp	short KEY_CHANGE_EXIT
	even
SHIFT_LEFT:
	mov	AX,0001h
	jmp	short KEY_CHANGE_EXIT
	even
SHIFT_RIGHT:
	mov	AX,0006h
	jmp	short KEY_CHANGE_EXIT
	even
Tab:
	mov	AX,0009h
	jmp	short KEY_CHANGE_EXIT
	even
BackSpace:
	mov	AX,0008h
	jmp	short KEY_CHANGE_EXIT
	even
NO_SHIFT_KEY:
	mov	BL,AH
	sub	BL,03bh
	mov	BH,0
	add	BX,BX
	mov	AX,key_code_98[BX]
KEY_CHANGE_EXIT:
	ret	4
endfunc		; }

END
