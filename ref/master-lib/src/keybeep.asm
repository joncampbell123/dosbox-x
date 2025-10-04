	.MODEL SMALL
	include func.inc
	.CODE

;	void key_beep_off(void) ;
;	void key_beep_on(void) ;
;	92/11/24

func KEY_BEEP_OFF
	xor	AX,AX
	mov	ES,AX
	or	byte ptr ES:[500H],20h
	ret
	EVEN
endfunc

func KEY_BEEP_ON
	xor	AX,AX
	mov	ES,AX
	and	byte ptr ES:[500H],NOT 20h
	ret
	EVEN
endfunc

END
