; master library - MS-DOS - 30BIOS
;
; Description:
;	30�sBIOS����
;	GDC�N���b�N�̎擾�C�ύX
;
; Procedures/Functions:
;	bios30_getclock( void ) ;
;	void bios30_setclock( int clock ) ;
;
; Parameters:
;	clock	0(BIOS30_CLOCK2) = 2.5MHz
;		1(BIOS30_CLOCK5) = 5.0MHz
;
; Returns:
;	bios30_getclock:
;		0(BIOS30_CLOCK2) = 2.5MHz / ���T�|�[�g
;		1(BIOS30_CLOCK5) = 5.0MHz
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios or TT)
;
; Requiring Resources:
;	CPU: 80186(V30)
;
; Notes:
;	30�sBIOS 1.20�ȍ~�ŗL��
;	TT.com 1.50�ȍ~�ŗL��
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����`��
;	����������
;
; Revision History:
;	94/ 8/ 6 �����o�[�W�����B
;

	.MODEL SMALL
	.186
	INCLUDE FUNC.INC
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETCLOCK	; bios30_getclock( void ) {
	_call	BIOS30_TT_EXIST		; 30�sBIOS 1.20�ȍ~, TT 1.50�ȍ~
	test	al,8ch
	mov	al,0
	jz	short GETCLOCK_FAILURE
	jns	short TT_GETCLOCK	; jnp�́ATT 1.50�ȍ~��30�sBIOS API��
	jnp	short GETCLOCK_FAILURE	; �����Ȃ����Ƃ��O��

	mov	bl,0ffh			; 30�sBIOS
	mov	ax,0ff07h
	int	18h
	xor	ah,ah
GETCLOCK_FAILURE:
	ret

TT_GETCLOCK:
	mov	ax,1802h		; TT
	mov	bx,'TT'
	int	18h
	shr	ax,2
	and	ax,1
	ret
endfunc			; }


func BIOS30_SETCLOCK	; void bios30_setclock( int clock ) {
	_call	BIOS30_TT_EXIST		; 30�sBIOS 1.20�ȍ~, TT 1.50�ȍ~
	mov	bx,sp			; bx clock
	clock = (RETSIZE+0)*2
	mov	bx,ss:[bx+clock]
	and	bx,1
	test	al,8ch
	jz	short SETCLOCK_FAILURE
	jns	short TT_SETCLOCK	; jnp�́ATT 1.50�ȍ~��30�sBIOS API��
	jnp	short SETCLOCK_FAILURE	; �����Ȃ����Ƃ��O��

	mov	ax,0ff07h		; 30�sBIOS
	int	18h			; GDC�N���b�N�ݒ�

	xor	bl,bl			; ���[�h���Đݒ肵���肳����
	mov	ax,0ff09h		; dl �g���ݒ� by KEI SAKAKI.
	int	18h
	and	al,0c0h
	mov	dl,al

	mov	ah,0bh			; �ݒ肷��l�̍쐬 by KEI SAKAKI.
	mov	bx,'30'+'�s'
	int	18h
	and	al,NOT 0c0h
	or	al,dl
	dec	ah			; mov ah,0ah
TT_RETURN:
	int	18h
SETCLOCK_FAILURE:
	ret	2

TT_SETCLOCK:
	mov	dx,bx			; TT
	mov	ax,1802h
	mov	bx,'TT'
	int	18h
	and	al,NOT 04h
	shl	dl,2
	or	al,dl
	jmp	short TT_RETURN
endfunc			; }

	END
