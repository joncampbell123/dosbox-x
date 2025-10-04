; master library - PC-9801
;
; Description:
;	GRPHìØéûì¸óÕÇÃâ¬î\Ç»ÉLÅ[ì¸óÕ
;
; Functions:
;	int gkey_getkey( void ) ;
;
; Author:
;	óˆíÀè∫ïF
;
; Revision History:
;	93/ 4/30 Initial: gkey.asm/master.lib 0.16
;

	.MODEL SMALL
	include func.inc

	.DATA

_ESC	= 27 ; [ESC]
BSP	= 'H' - '@' ; [BS]
TAB	= 'I' - '@' ; [TAB]
_RET	= 'M' - '@' ; [RETURN]

XFR	= 0		; [XFER]		?
RUP	= 'R' - '@'	; [ROLL UP]		?
RDN	= 'C' - '@'	; [ROLL DOWN]
_INS	= 'V' - '@'	; [INS] ctrl+V
DEL	= 'G' - '@'	; [DEL] ctrl+G
UPA	= 'E' - '@'	; [Å™]
DNA	= 'X' - '@'	; [Å´]
LFA	= 'S' - '@'	; [Å©]
RIA	= 'D' - '@'	; [Å®]
HMC	= 'Z' - '@'	; [HOME/CLR]		?
HLP	= 'P' - '@'	; [HELP]		?
NFR	= 0		; [NFER]

	;   00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f
keytable db _ESC,'1','2','3','4','5','6','7','8','9','0','-','^','\',BSP,TAB
	db  'Q','W','E','R','T','Y','U','I','O','P','@','[',_RET,'A','S','D'
	db  'F','G','H','J','K','L',';',':',']','Z','X','C','V','B','N','M'
	db  ',','.','/','_',' ',XFR,RUP,RDN,_INS,DEL,UPA,LFA,RIA,DNA,HMC,HLP
	db  '-','/','7','8','9','*','4','5','6','+','1','2','3','=','0',','
	db    0,NFR


	.CODE

func GKEY_GETKEY
	mov	AH,1
	int	18h	; sense key buffer
	test	BH,BH
	jnz	short BIOSTEST
GRAPHTEST:
	mov	AH,2	; sense shift key
	int	18h
	test	AL,08h	; GRPH
	jz	short DOSGET	; GRPHÇ™âüÇ≥ÇÍÇƒÇ¢ÇΩÇÁDOSÇí Ç≥Ç»Ç¢
NASHI:
	mov	AX,-1
	ret

DOSGET:
	mov     AH,6
	mov     DL,0FFh
	int     21h
	jz      short NASHI
	mov	AH,0
	ret

BIOSTEST:
	test	AL,0e0h
	jns	short DOSGET	; lower 7bit
	jpe	short DOSGET	; kana

	; 80h..9fh, e0h..ffh ( GRPH-key! )
	mov	AL,AH
	mov	BX,offset keytable
	xlatb
	test	AL,AL
	je	short	GRAPHTEST

	mov	AH,0
	push	AX
	int	18h	; read key buffer
	pop	AX
	ret
endfunc

END
