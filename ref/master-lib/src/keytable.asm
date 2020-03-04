; master library
; Descriptin:
;	制御キーの初期定義情報
;
; Revision History:
;	93/ 3/16 Initial
;	93/12/ 9 [M0.22] assignment変更

	.MODEL SMALL
	.DATA

	PUBLIC key_table_normal,_key_table_normal
	PUBLIC key_table_shift,_key_table_shift
	PUBLIC key_table_ctrl,_key_table_ctrl
	PUBLIC key_table_alt,_key_table_alt

	HELPKEY equ 100h

_key_table_normal label word
key_table_normal dw	'C'-'@'		; ROLLUP
		dw	'R'-'@'		; ROLLDOWN
		dw	'V'-'@'		; INS
		dw	'G'-'@'		; DEL
		dw	'E'-'@'		; UP
		dw	'S'-'@'		; LEFT
		dw	'D'-'@'		; RIGHT
		dw	'X'-'@'		; DOWN
		dw	'Y'-'@'		; HOME/CLR
		dw	HELPKEY		; HELP
		dw	0		; ----- no means ----

_key_table_shift label word
key_table_shift	dw	'Z'-'@'		; ROLLUP
		dw	'W'-'@'		; ROLLDOWN
		dw	'@'-'@'		; INS
		dw	'T'-'@'		; DEL
		dw	'R'-'@'		; UP
		dw	'A'-'@'		; LEFT
		dw	'F'-'@'		; RIGHT
		dw	'C'-'@'		; DOWN
		dw	0		; ----- no means ----
		dw	HELPKEY		; HELP
		dw	'@'-'@'		; SHIFT HOME/CLR

_key_table_ctrl label word
key_table_ctrl	dw	'C'-'@'+200h	; ROLLUP
		dw	'R'-'@'+200h	; ROLLDOWN
		dw	'V'-'@'+200h	; INS
		dw	'G'-'@'+200h	; DEL
		dw	'E'-'@'+200h	; UP
		dw	'S'-'@'+200h	; LEFT
		dw	'D'-'@'+200h	; RIGHT
		dw	'X'-'@'+200h	; DOWN
		dw	HELPKEY		; HOME/CLR
		dw	HELPKEY		; HELP
		dw	0		; ----- no means ----

_key_table_alt label word
key_table_alt	dw	'C'-'@'+300h	; ROLLUP
		dw	'R'-'@'+300h	; ROLLDOWN
		dw	'V'-'@'+300h	; INS
		dw	'G'-'@'+300h	; DEL
		dw	'E'-'@'+300h	; UP
		dw	'S'-'@'+300h	; LEFT
		dw	'D'-'@'+300h	; RIGHT
		dw	'X'-'@'+300h	; DOWN
		dw	HELPKEY		; HOME/CLR
		dw	HELPKEY		; HELP
		dw	0		; ----- no means ----

END
