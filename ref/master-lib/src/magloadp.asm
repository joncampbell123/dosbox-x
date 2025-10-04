; master library - graphics - load - mag
;
; Description:
;	16�Fmag�t�H�[�}�b�g�摜�t�@�C���̃p�b�N�`���ǂݍ���
;
; Function/Procedures:
;	int mag_load_pack( const char * filename, MagHeader * head, void far * *image ) ;
;
; Parameters:
;	filename	MAG�t�H�[�}�b�g�摜�̃t�@�C����
;	head		�ǂݍ��݌��ʂ�MAG�f�[�^�Ɋւ�����̊i�[��
;	image		�ǂݍ��݌��ʂ̉摜�f�[�^�̐擪�A�h���X�̊i�[��
;
; Returns:
;	NoError			�ǂݍ��񂾁B
;	FileNotFound		�t�@�C�����݂���Ȃ�
;	InvalidData		�f�[�^��MAG�łȂ����A��ɗ]��(^^;
;	InsufficientMemory	�W�J����ɂ̓����������Ƒ���Ȃ�
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	V30/186
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	256�F�f�[�^�� 1byte=1pixel�œW�J�����͂�(^^;
;	��3850�h�b�g�ȏ�Ȃ� InvalidData
;	x1>x2 �܂��� y1>y2 �Ȃ� InvalidData
;	flag A��64K�ȏ�Ȃ� InvalidData
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
;	93/12/15 Initial: magloadp.asm/master.lib 0.22
;	94/ 1/ 9 [M0.22] �R�����g���ǂݍ��ނ悤�ɂ���
;	94/ 2/18 [M0.22a] MagHeader�� xsize, ysize�����o�ǉ�
;	94/ 3/16 [M0.23] �ŏ��̃o�b�t�@�m�ێ��s����ƃt�@�C������Ȃ�����
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 4/ 1 [M0.22k] mem_AllocID�ł�ID��pi�ɂȂ��Ă��̂�mag�ɒ���


	.186
	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_LGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL
	EXTRN	HMEM_LALLOCATE:CALLMODEL
	EXTRN	DOS_ROPEN:CALLMODEL

IFDEF ??version		; tasm check
	JUMPS
	WARN
ENDIF

	.DATA

	EXTRN mem_AllocID:WORD		; mem.asm

MAG_ID	DB	'MAKI02  '

	;  y,x*2
point	DB 0,0, 0,2, 0,4, 0,8
	DB 1,0, 1,2, 2,0, 2,2
	DB 2,4, 4,0, 4,2, 4,4
	DB 8,0, 8,2, 8,4, 16,0

	.CODE

	; buffer�̃Z�O�����g���f�[�^���
;BUFFERSIZE equ 4096
BUFFERSIZE equ 8192
bufptr_ds equ	WORD PTR DS:[BUFFERSIZE+0]
fd_ds	equ	WORD PTR DS:[BUFFERSIZE+2]

POINT_OFF = BUFFERSIZE+16	; tempseg����̑��΂ł̒l�B
FLAG_OFF = POINT_OFF+32		; flag_seg�́Atempseg�̒���ł���B

BUFFERBLOCK_SIZE equ (BUFFERSIZE+4)

	; �����̏����r�l
MAX_WIDTH equ 3850

	; Mag�t�@�C���̃w�b�_�������̂܂�
MagHeader STRUC
_head	   db ?		; 0
_machine   db ?		; 1
_exflag	   db ?		; 2
_scrnmode  db ?		; 3
_x1	   dw ?		; 4
_y1	   dw ?		; 6
_x2	   dw ?		; 8
_y2	   dw ?		; 10
_flagAofs  dd ?		; 12
_flagBofs  dd ?		; 16
_flagBsize dd ?		; 20
_pixelofs  dd ?		; 24
_pixelsize dd ?		; 28
_xsize	   dw ?		; 32
_ysize	   dw ?		; 34
_palette   db 48 dup (?) ; 
MagHeader ENDS


	; in: DS = tempseg
	; breaks: BX=0
readbuf	PROC NEAR
	push	AX
	push	CX
	push	DX
	mov	BX,fd_ds
	xor	DX,DX
	mov	CX,BUFFERSIZE
	mov	AH,3fh		; read
	int	21h
	pop	DX
	pop	CX
	pop	AX
	xor	BX,BX
	ret
	EVEN
readbuf	ENDP

	; in: DS = tempseg
	; out: AX = read char
	; breaks: AX,BX
readword PROC NEAR
	mov	BX,bufptr_ds
	cmp	BX,BUFFERSIZE-1
	jb	short READW_W
	je	short READW_B
	call	readbuf
READW_W:
	mov	AX,[BX]
	add	BX,2
	mov	bufptr_ds,BX
	ret
	EVEN
	; out: AL = read char
	; breaks: AL,BX
readbyte:			;��������
	mov	BX,bufptr_ds
	cmp	BX,BUFFERSIZE
	jb	short READB1
READW_B:
	mov	AL,[BX]
	call	readbuf
READB1:	mov	AH,[BX]
	inc	BX
	mov	bufptr_ds,BX
	ret
	EVEN
readword ENDP

IF 1			; 1�ɂ���ƃ}�N���W�J
_readword MACRO
	local READW_W,READW_NB,READW_E
	mov	BX,bufptr_ds
	cmp	BX,BUFFERSIZE-1
	jb	short READW_W
	jne	short READW_NB
	mov	AL,[BX]
	call	readbuf
	mov	AH,[BX]
	inc	BX
	jmp	short READW_E
	even
READW_NB:
	call	readbuf
READW_W:
	mov	AX,[BX]
	add	BX,2
READW_E:
	mov	bufptr_ds,BX
ENDM
ELSE
_readword MACRO
	call	readword
ENDM
ENDIF


	; read_block( unsigned tempseg, unsigned read_seg, long len) ;
	; in: DS=tempseg, DXAX = len, ES = readto
	; out:
	; breaks:
read_block PROC NEAR
	ASSUME DS:NOTHING
	push	SI
	push	DI
	xor	DI,DI			; read to
	mov	CX,BUFFERSIZE
	mov	SI,bufptr_ds
	sub	CX,SI			; �܂��o�b�t�@�̒�������o��
	jz	short RB_2		; �o�b�t�@����Ȃ��΂�
	test	DX,DX
	jnz	short RB_1
	cmp	CX,AX
	jb	short RB_1
	mov	CX,AX			; CX = min(CX, DXAX)
RB_1:
	sub	AX,CX
	sbb	DX,0

	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb
	mov	bufptr_ds,SI

					; ���ɒ��ڃt�@�C�����[�h�Ōp��
RB_2:
	mov	BX,AX
	or	BX,DX
	jz	short RB_END		; �v�����������Ă����Ȃ�I����
	mov	BX,fd_ds
	push	DS
	push	ES
	pop	DS		; DS:DI = readto

RB_LOOP:
	push	AX
	push	DX

	mov	CX,8000h
	test	DX,DX
	jnz	short RB_LOOP_1
	cmp	CX,AX
	jb	short RB_LOOP_1
	mov	CX,AX
RB_LOOP_1:
	mov	DX,DI
	mov	AH,3fh		; read
	int	21h

	mov	AX,DS
	add	AX,0800h
	mov	DS,AX

	pop	DX
	pop	AX

	sub	AX,CX
	sbb	DX,0
	jnz	short RB_LOOP
	test	AX,AX
	jnz	short RB_LOOP

	pop	DS

RB_END:	
	pop	DI
	pop	SI
	ret
	EVEN
read_block	ENDP


MRETURN macro
	pop	DI
	pop	SI
	leave	
	ret	(DATASIZE*3)*2
	EVEN
endm

func MAG_LOAD_PACK	; mag_load_pack() {
	enter	34,0
	push	SI
	push	DI

	filename   = (RETSIZE+1+DATASIZE*2)*2
	head	   = (RETSIZE+1+DATASIZE)*2
	image	   = (RETSIZE+1)*2

	flagAsize  = -4
	flagBsize  = -8
	image_seg  = -10
	flagAseg   = -12
	flagAptr   = -14
	flagBseg   = -16
	flagBptr   = -18
	tempseg	   = -20
	line_bytes = -22
	x_8dots	   = -24
	min_offset = -26
	lines	   = -28
	fd	   = -30
	sub_offset = -32
	max_offset = -34

	CLD
	ASSUME DS:DGROUP

	_push	[BP+filename+2]
	push	[BP+filename]
	_call	DOS_ROPEN
	jc	short FASTRETURN

	mov	[BP+fd],AX
	push	BUFFERBLOCK_SIZE
	_call	SMEM_WGET
	jc	short CLOSERETURN2
	push	DS
	push	DS
	pop	ES		; ES = DGROUP
	mov	[BP+tempseg],AX

	ASSUME DS:NOTHING

	mov	DS,AX		; DS = tempseg

	mov	AX,[BP+fd]
	mov	fd_ds,AX
	call	readbuf		; �܂��o�b�t�@�ɓǂݍ���

	mov	DI,offset MAG_ID
	xor	SI,SI
	mov	CX,8 / 2
	repe	cmpsw
	je	short SEARCH_EOFCHAR_START
	pop	DS
	ASSUME DS:DGROUP
	jmp	INVALID_RETURN
FASTRETURN:
	jmp	RETURN		; ********* �����B���p(^^;
CLOSERETURN2:
	push	AX
	jmp	CLOSERETURN	; ���p
	EVEN

	ASSUME DS:NOTHING

SEARCH_EOFCHAR_START:
	mov	bufptr_ds,8	; ID������̒���
	mov	CX,-1
SEARCH_EOFCHAR:
	call	readbyte
	inc	CX
	cmp	AH,1ah
	jne	short SEARCH_EOFCHAR

	; �R�����g��ǂ�
	; �R�����g��8KB�����ł��邱��
	mov	BX,DS
	pop	DS

	ASSUME DS:DGROUP

	mov	mem_AllocID,MEMID_mag
	push	CX
	_call	HMEM_ALLOCBYTE
	push	DS

	mov	DS,BX
	ASSUME DS:NOTHING

	jc	short COMMENT_READ_END
	mov	ES,AX
	xor	DI,DI
	mov	SI,8
	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb
	mov	CX,DI
COMMENT_READ_END:
	; �w�b�_��ǂ�
	_les	DI,[BP+head]
     s_ <push	SS>		; small model�̂Ƃ���, SS=DGROUP�����肵���Ⴄ
     s_ <pop	ES>
	stosw			; �R�����g�̃Z�O�����g
	mov	AX,CX		; �R�����g��
	stosw
	mov	CX,32/2		; mag�w�b�_�̑傫��(�p���b�g�̑O)
READHEAD:
	call	readword
	stosw
	loop	short READHEAD

	stosw				; _xsize�̂Ԃ�i�߂�(��ŏ�������)
	stosw				; _ysize�̂Ԃ�i�߂�(�����)

	; �p���b�g���ǂ񂶂Ⴄ
	mov	CX,16
READPALETTE:
	call	readword
	xchg	AL,AH
	stosw
	call	readbyte
	mov	AL,AH
	stosb
	loop	short READPALETTE
	sub	DI,type MagHeader	; �w�b�_�̃T�C�Y�Ԃ�߂�
	pop	DS

	ASSUME DS:DGROUP

	mov	AX,ES:[DI]._x2
	or	AX,7
	mov	SI,ES:[DI]._x1
	and	SI,not 7
	sub	AX,SI
	jc	short INVALID_RETURN	; ���̒������}�C�i�X���Ⴀ�˂�
	inc	AX
	mov	ES:[DI]._xsize,AX	; ���̒����i�[

IF 0	; 256�F�͂܂��˂��c
	mov	CL,2
	test	ES:[DI]._scrnmode,80h
	jnz	short COLOR256
	inc	CX			; �ł��p���b�g���c
COLOR256:
	...
ELSE
	test	ES:[DI]._scrnmode,80h
	jnz	short INVALID_RETURN
	add	AX,7
	shr	AX,3
ENDIF
	mov	[BP+x_8dots],AX

	mov	CX,ES:[DI]._y2
	sub	CX,ES:[DI]._y1
	jc	short INVALID_RETURN	; �c�̒������}�C�i�X���Ⴀ�˂�
	inc	CX
	mov	ES:[DI]._ysize,CX	; �c�̒����i�[
	mov	[BP+lines],CX

	shl	AX,2
	mov	CX,AX		; CX(line_bytes) = x_8dots * 4 ;

	mov	SI,AX		; SI(min_offset) = line_bytes * 16 ;
	shl	SI,4

	test	AH,0f0h
	jnz	short INVALID_RETURN	; 1segment��16line����Ȃ��Ɩ���
	add	AX,4		; 4�͗]�T(4byte���ɏ������ނ��߂ɗV�т�����)
	neg	AX
	mov	[BP+max_offset],AX	; AX = max_offset

	mov	DX,CX
	add	DX,0fh
	and	DL,0f0h
	mov	[BP+sub_offset],DX	; sub_offset = (line_bytes+15)&0xfff0;

	cmp	CX,MAX_WIDTH		; �����̏������
	jae	short INVALID_RETURN
	cmp	AX,SI			; max_offset < min_offset�Ȃ�s�\
	jnb	short DATA_OK
INVALID_RETURN:				; �f�[�^���Ή��ł��Ȃ��Ƃ�
	mov	AX,InvalidData
	jmp	short FAILURE
NO_MEMORY:				; �������s����
	mov	AX,InsufficientMemory
FAILURE:
	push	AX
	push	[BP+tempseg]
	_call	SMEM_RELEASE
CLOSERETURN:					; �����ɂ���ɂ͗vpush ax
	mov	AH,3eh			; close
	mov	BX,[BP+fd]
	int	21h
	pop	AX
	stc
	jmp	RETURN			; **************
DATA_OK:

	mov	[BP+line_bytes],CX
	mov	[BP+min_offset],SI

	mov	AX,CX			; line_bytes+32�o�C�g����
	add	AX,32			; tempseg�u���b�N���g�傷��
	push	AX			;                  ~~~~~~~~
	_call	SMEM_WGET
	jc	short NO_MEMORY
	add	CX,offset FLAG_OFF+15
	and	CL,0f0h
	mov	[BP+flagAptr],CX	; flagA��tempseg�ɒu���̂�offset���c

	mov	AX,word ptr ES:[DI]._flagBofs
	mov	DX,word ptr ES:[DI+2]._flagBofs
	sub	AX,word ptr ES:[DI]._flagAofs
	sbb	DX,word ptr ES:[DI+2]._flagAofs
	jnz	short INVALID_RETURN	; flagA�� 64K�ȓ��ł��邱��
	mov	[BP+flagAsize],AX
	push	AX
	_call	SMEM_WGET		; �������g��ړI
	jc	short NO_MEMORY
	mov	[BP+flagAseg],AX	; �ł��ǂݍ��݂̂��߂�segment�͕ۑ�

	mov	AX,WORD PTR ES:[DI]._pixelofs
	mov	DX,WORD PTR ES:[DI+2]._pixelofs
	sub	AX,WORD PTR ES:[DI]._flagBofs
	sbb	DX,WORD PTR ES:[DI+2]._flagBofs
	mov	[BP+flagBsize],AX
	mov	[BP+flagBsize+2],DX
	push	DX
	push	AX
	_call	SMEM_LGET
	jc	short NO_MEMORY
	mov	[BP+flagBseg],AX

	mov	AX,[BP+line_bytes]
	mul	WORD PTR [BP+lines]
	push	DX
	push	AX
	mov	mem_AllocID,MEMID_mag
	_call	HMEM_LALLOCATE
	jc	NO_MEMORY		;***********
	mov	[BP+image_seg],AX

	_push	DS
	_lds	BX,[BP+image]		; *image �ւ̏�������
	mov	WORD PTR [BX],0
	mov	[BX+2],AX
	_pop	DS

	push	DS
	mov	DS,[BP+tempseg]
	ASSUME DS:NOTHING
	mov	AX,[BP+flagAsize]
	xor	DX,DX
	mov	ES,[BP+flagAseg]
	call	read_block

	mov	AX,[BP+flagBsize]
	mov	DX,[BP+flagBsize+2]
	mov	ES,[BP+flagBseg]
	call	read_block
	pop	DS
	ASSUME DS:DGROUP

	; ���Έʒu�\���쐬
	mov	ES,[BP+tempseg]
	mov	CX,16
	mov	DI,POINT_OFF
MAKE_POINT:
	mov	AL,point[DI-POINT_OFF]	; .y
	mov	AH,0
	imul	word ptr [BP+line_bytes]
	add	AL,point[DI+1-POINT_OFF] ; .x
	adc	AH,0
	neg	AX
	stosw
	loop	short MAKE_POINT
	; �������� flag���[���N���A
	xor	AX,AX
	mov	CX,[BP+line_bytes]
	rep	stosb

	mov	[BP+flagBptr],AX	; 0

	mov	DI,AX			; ES:DI = output address
	cmp	[BP+min_offset],DI
	je	short LOOPSTART
	mov	AX,[BP+min_offset]	; DI �� min_offset�ȏ�ɂȂ�悤��
	add	AX,0fh			; image_seg �����炵�ďC��
	and	AL,0f0h
	mov	DI,AX
	shr	AX,4
	sub	[BP+image_seg],AX
LOOPSTART:
	mov	ES,[BP+image_seg]
	mov	DH,80h			; DH = flag_bit

	; y�̃��[�v����
	push	DS
	mov	DS,[BP+tempseg]
	ASSUME DS:NOTHING

	; y�̃��[�v
YLOOP:
	; DI�̃A�h���X�𒲐�
	cmp	[BP+max_offset],DI
	jae	short V_ADJUST_SKIP
	mov	AX,[BP+sub_offset]
	sub	DI,AX
	shr	AX,4
	add	[BP+image_seg],AX		; ES��i�߂�
	mov	ES,[BP+image_seg]
V_ADJUST_SKIP:
	; flag B pointer ���K��
	mov	AX,[BP+flagBptr]
	mov	CX,AX
	shr	AX,4
	add	[BP+flagBseg],AX
	and	CX,0fh
	mov	[BP+flagBptr],CX

	; x�̃��[�v����
	mov	SI,FLAG_OFF	; flagptr�͑��Έʒu�\�̌��ɂ���

	; (DS: | flag_seg)SI = flag pointer
	; ES:DI = image pointer

	mov	CX,[BP+x_8dots]

	; x�̃��[�v
XLOOP:
	mov	BX,[BP+flagAptr]
	test	[BX],DH
	jz	short UETO_ONAJI
	lds	BX,[BP+flagBptr]
	inc	WORD PTR [BP+flagBptr]
	mov	AL,[BX]
	mov	DS,[BP+tempseg]
	xor	[SI],AL
UETO_ONAJI:
	ror	DH,1
	adc	word ptr [BP+flagAptr],0

	mov	BL,[SI]
	inc	SI
	mov	DL,BL		; DL = *flagptr
	shr	BX,4
	and	BX,0fh
	jnz	short REFERENCE_1

	_readword
	jmp	short RIGHT2DOT
	EVEN
REFERENCE_1:
	add	BX,BX
	mov	BX,POINT_OFF[BX]
	mov	AX,ES:[BX+DI]

RIGHT2DOT:
	stosw

	mov	BX,DX
	and	BX,0fh
	jnz	short REFERENCE_2

	_readword
	stosw
	loop	XLOOP	; X�̃��[�v�I���(1)
	dec	WORD PTR [BP+lines]
	je	SHORT SUCCESS
	jmp	YLOOP
	EVEN
REFERENCE_2:
	add	BX,BX
	mov	BX,POINT_OFF[BX]
	mov	AX,ES:[BX+DI]
	stosw
	loop	XLOOP	; X�̃��[�v�I���(2)
	dec	WORD PTR [BP+lines]
	jne	YLOOP
SUCCESS:
	pop	DS
	ASSUME DS:DGROUP

	push	[BP+tempseg]
	_call	SMEM_RELEASE
	mov	AH,3eh			; close
	mov	BX,[BP+fd]
	int	21h
	xor	AX,AX			; NoError
RETURN:
	MRETURN
endfunc			; }

END
