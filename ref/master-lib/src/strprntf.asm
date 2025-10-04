page 60,120
; master library
;
; Description:
;	�t�H�[�}�b�g�w�肵�ĕ�������쐬����
;
; Function/Procedures:
;	void str_printf( char * buf, const char * format, ... ) ;
;
; Parameters:
;	char * buf	�������ݐ�
;	char * format	����������
;	...		����
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	none
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�b����stdio�� sprintf�Ɏ��Ă��܂����A�������Ⴂ�܂��B
;	����:
;		%[0][��][l]u	�E�l�ߕ����Ȃ�10�i��
;		%[0][��][l]x	�E�l��16�i��
;		%[0][��][l]b	�E�l��2�i��
;		%[��]s		���l�ߕ�����
;		%c		����
;		%%		%���g
;
;		[0]�́A�w�肷��Ɨ]�����������[���Ŗ��߂܂��B
;		[��]�͏ȗ������ 0 �ɂȂ�܂��B
;		[l]�́Ashort�ł͂Ȃ�long�����ɂȂ�܂��B
;
;		x,X�́A�ǂ�����啶���Ƃ��ē��삵�܂��B
;		x,s,u,c�ȊO�̕����͑S��u�����ɂȂ�܂��B
;		�������A�����R�[�h��2fh�ȉ��̋L���͂��̕������g��\���܂��B
;
;		[��]�̈Ӗ�:
;			%u,x,b	�Œ�̌���(�t�B�[���h��)
;			%s	�ŏ�����
;			%c	����
;			%%	����
;
;	FAR�ł́A%s�̃|�C���^��far pointer,
;	NEAR�ł�near pointer�݂̂ɂȂ�܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/21 Initial
;	92/12/10 Large�Ή�, '0'pad�Ή�
;	92/12/15 %X�Ή�(near 256bytes)
;	92/12/16 %x��%X�̓���,%c,%%�Ή�(near 242bytes, far 252bytes)
;	92/12/17 %b�̒ǉ�(near 250bytes, far 260bytes)
;	93/ 6/ 9 [M0.19] format �� str �� seg ���قȂ�Ƃ��� bug fix(+2bytes)

	.MODEL SMALL
	include func.inc

	.CODE

; DX:AX = DX:AX / CL
; CH    = DX:AX % CL
; 92/9/30
; 92/12/10
; 92/12/16 ����ɏk��(22bytes)
	public ASM_LBDIV
ASM_LBDIV proc near
LLONG:
	push	BX
	xor	BX,BX
	mov	CH,BL	; 0
	test	DX,DX		; �����̂Q�s������4bytes���邪
	je	short LWORD	; ������͔��������c
	xchg	BX,AX	; BX <- AX ( �폜���̉��� )
	xchg	AX,DX	; AX <- DX ( �폜���̏�� )
			; DX = 0
	div	CX
	xchg	AX,BX	; BX = ���̏��16bit
LWORD:
	div	CX
	mov	CH,DL	; CH = �]��
	mov	DX,BX
	pop	BX
	ret
	EVEN
ASM_LBDIV endp


func _str_printf
	push	BP
	mov	BP,SP
	_push	DS

	; ����
	buf	= (RETSIZE+1)*2
	format	= (RETSIZE+1+DATASIZE)*2
	param	= (RETSIZE+1+DATASIZE+DATASIZE)*2

	push	SI
	push	DI
 s_	<push	DS>
 s_	<pop	ES>

	mov	AL,0
	_les	SI,[BP+format]	; ES:SI = DI = format
	mov	DI,SI
	mov	CX,-1
	repne	scasb		; �I�[��T��
	not	CX		; CX = �I�[�������܂񂾒���
	_lds	DI,[BP+buf]	; DS:DI = buf
	_push	DS		; push <segment buf>
	_push	ES
	_pop	DS		; DS = ES = seg format

	jmp	short SloopFirst

Schar:	; �����Ă�Ȃ��B�������B
	mov	AL,[BP+param]
	inc	BP
	inc	BP
Snama:
	stosb

Sloop:
	_push	ES		; push <segment buf>
	_push	DS
	_pop	ES
SloopFirst:
	mov	DX,DI
	mov	DI,SI		; ES:DI = format
	mov	AL,'%'
	repne	scasb		; while ( AL != *DI++ ) {}
	lahf
	dec	DI
	sub	DI,SI
	sahf
	xchg	AX,DI		; AX = %�̑O�܂ł�len
	mov	DI,DX
	_pop	ES		; pop <segment buf> to ES

	xchg	AX,CX		; %�̑O�܂ł̕������]��
	rep	movsb
	xchg	AX,CX		; CX ���A, AX = 0
	jnz	short Sowari	; %���Ȃ�������I����
	inc	SI		; %���΂�

	; ES:DI = buf
	; DS:SI = format
	mov	BX,2000h ; BH = ' ', BL = 0

	; �����ǂݎ��`
	lodsb
	dec	CX
	jz	short Sowari
	cmp	AL,'0'
	jne	short S2E
	mov	BH,AL		; ������ 0�Ŏn�܂�Ƃ�: BH = '0'
	jmp	short S2N

	; ����ȂƂ���Ɂc
	; �����ɗ���Ƃ��� AL = 0 ����
Sowari:			; �I���
	; mov	AL,0
	stosb
	pop	DI
	pop	SI
	_pop	DS
	pop	BP
	ret

	; �����ǂݎ��̑���
S2:	and	AL,0fh	; �ȗ�
	aad		; AX = AH*10+AL
	mov	AH,AL
S2N:	lodsb
	dec	CX
	jz	short Sowari
	cmp	AL,'0'
S2E:	jb	short Snama	; �L���͐��ŏ�������(%%�ɑΉ����邽��)
	cmp	AL,'9'
	jbe	short S2

	cmp	AL,'s'
	je	short Sstr
	cmp	AL,'c'
	je	short Schar

	cmp	AL,'l'	; %<��>l<����> = long
	jne	short S4
	inc	BX	; long: BL=1  int:BL=0
	lodsb
	dec	CX
	jz	short Sowari
S4:
	xchg	BL,AH	; BL = ����, AH = long(1) or short(0)

	; %[0]<wid>[l]<flag>

	push	CX
	; �����i�[

	mov	CL,2
	cmp	AL,'b'		; �Q�i��
	je	short S4d

	mov	CL,10
	and	AL,0dfh	; �啶����
	cmp	AL,'X'
	jne	short S4d
	mov	CL,16
S4d:
				; DI = buf  BH=pad BL = ��  AH=long or short
	mov	CH,AH		; CH=long(1) or short(0)
	cwd			; DX = 0(AH��0��1������)
	mov	AX,[BP+param]
	inc	BP
	inc	BP
	test	CH,CH
	jz	short S4a
	mov	DX,[BP+param]		; long
	inc	BP
	inc	BP
S4a:
	push	BX		; padchar + ���� BX ����ۑ�
	xor	BH,BH
	dec	BX		; 0���Ȃ�Ȃɂ�����
	jl	short S4q

	; ���������������ł䂭�`
		; DXAX = input value   CH = unuse CL = radix
S4l:		; DS:SI = format   ES:DI = buf   BX = ��
	call	ASM_LBDIV
	cmp	CH,10
	jl	short S4dec
	add	CH,7
S4dec:
	add	CH,'0'
 l_	<mov	ES:[DI+BX],CH>
 s_	<mov	[DI+BX],CH>
	dec	BX
	js	short S4q
	test	AX,AX
	jnz	short S4l
	cmp	AX,DX
	jne	short S4l

	; �]�������� padchar�Ŗ��߂Ă䂭�`
	pop	AX		; padchar + ���� AX�ɕ��A
S4pad:
 l_	<mov	ES:[DI+BX],AH>	; pad char
 s_	<mov	[DI+BX],AH>
	dec	BX
	jns	short S4pad
	push	AX
S4q:
	pop	BX
	pop	CX		; CX = format len   SI = format
	xor	BH,BH
	add	DI,BX		; DI = buf
	jmp	near ptr Sloop

Sstr:	; ������(%s) ���l�߂���
				; DI = buf  BH = pad AH = ��  AH=long or short
	push	CX
	push	SI
	mov	CH,0
	mov	CL,AH		; CX=��

	_push	DS
	_lds	SI,[BP+param]
	s_inc	BP
	s_inc	BP
	_add	BP,4
	jmp	short SStrs
Sstrloop:
	dec	CX
	stosb			; �����蔲���B���ꂶ�Ⴀ�������x����Ȃ�
SStrs:
	lodsb			; �ł��T�C�Y���������̂ł����̂�
	test	AL,AL
	jne	short Sstrloop
Sstrfill:
	test	CX,CX
	jle	short Sstrend
	mov	AL,' '	; space
	rep	stosb
Sstrend:
	_pop	DS
	pop	SI
	pop	CX
	jmp	near ptr Sloop
endfunc

END
