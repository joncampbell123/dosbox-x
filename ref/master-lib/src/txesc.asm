; master library - PC-9801
;
; Description:
;	
;
; Function/Procedures:
;	int text_height( void ) ;
;	int text_systemline_shown( void ) ;
;	void text_25line(void) ;
;	void text_20line(void) ;
;	void text_cursor_show(void) ;
;	void text_cursor_hide(void) ;
;	void text_systemline_hide(void) ;
;	void text_systemline_show(void) ;
;
; Parameters:
;	none
;
; Returns:
;	
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
;	���ˏ��F
;
; Revision History:
;	92/11/19 Initial
;	93/ 2/ 4 on/off -> show/hide�ɖ��O�ύX

	.MODEL SMALL
	include func.inc

	.CODE

; �e�L�X�g��ʂ̍s���𓾂�
; ���t�@���N�V�����L�[�\��������Ă���ƂP�������Ȃ��l�ɂȂ�B
func TEXT_HEIGHT
	xor	AX,AX
	mov	ES,AX
	mov	AL,ES:[0712h]
	inc	AX
	ret
	EVEN
endfunc

; �t�@���N�V�����L�[�\����Ԃ𓾂�
func TEXT_SYSTEMLINE_SHOWN
	xor	AX,AX
	mov	ES,AX
	mov	AL,ES:[0711h]
	ret
	EVEN
endfunc

func TEXT_25LINE
	mov	DX,'3l'
	jmp	short putcmd
	EVEN
endfunc

func TEXT_20LINE
	mov	DX,'3h'
	jmp	short putcmd
	EVEN
endfunc

func TEXT_CURSOR_HIDE
	mov	DX,'5h'
	jmp	short putcmd
	EVEN
endfunc

func TEXT_CURSOR_SHOW
	mov	DX,'5l'
	jmp	short putcmd
	EVEN
endfunc

func TEXT_SYSTEMLINE_HIDE
	mov	DX,'1h'
	jmp	short putcmd
	EVEN
endfunc

func TEXT_SYSTEMLINE_SHOW
	mov	DX,'1l'
	jmp	short putcmd
	EVEN
endfunc

; In: DH = first byte, DL = second byte
putcmd	proc CALLMODEL
	mov	AL,27	; ESC
	int	29h
	mov	AL,'['	; '['
	int	29h
	mov	AL,'>'	; '>'
	int	29h
	mov	AL,DH
	int	29h
	mov	AL,DL
	int	29h
	ret
putcmd	endp

END
