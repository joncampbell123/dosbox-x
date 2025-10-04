PAGE 98,120
;tab 8
; master library - Pi Format Graphic File Load
;
; Description:
;	16�F��Pi Format Graphic File �� pack�`���Ń������ɓǂݍ��݂܂��B
;
; Function/Procedures:
;	int graph_pi_load_pack(const char *filename,PiHeader *header, void far * * bufptr );
;
; Parameters:
;	char *filename	   �t�@�C����
;	PiHeader *header   ���i�[�p����
;	void far ** bufptr �摜�f�[�^�̐擪�A�h���X�i�[��
;
;	typedef struct PiHeader PiHeader ;
;	struct PiHeader {
;	unsigned char	far *comment; // ���̊֐��ł�NULL��set����邾��
;	unsigned int	commentlen;
;	unsigned char	mode;
;	unsigned char	n; // aspect
;	unsigned char	m; // aspect
;	unsigned char	plane; // �ʏ�� 4
;	unsigned char	machine[4];
;	unsigned int	maexlen; // machine extend data length
;	unsigned char	far *maex; // machine extend data
;	unsigned int	xsize;
;	unsigned int	ysize;
;	unsigned char	palette[48];
;	} ;
;
; Returns:
;	NoError			����
;	FileNotFound		�t�@�C�����Ȃ�
;	InvalidData		Pi�摜�t�@�C������Ȃ�,
;				ASPECT RATIO��0����Ȃ�, 16�F����Ȃ�
;	InsufficientMemory	�������s��
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;
; Compiler/Assembler:
;	TASM 2.51
;
; Note:
;	�R�����g�́A�{�֐������s���������ł͓ǂݍ��܂�܂���B
;	�Ԃ�l�́A�摜�̎n�܂���w���Ă��܂��B�������u���b�N�̎n�܂��
;	�������-(xsize*2)�ړ������ʒu�ɂ���܂��B
;
;	�� �t�@�C�����r���Ő؂�Ă���ꍇ�A�W�J���ʂ͕s���ɂȂ�܂��B
;	�� �G���[�̂Ƃ��ɂ� bufptr �̍����|�C���^�̒l�͕s��ł��B
;
; Author:
;	SuCa
;
; Rivision History:
;	93/10/13 initial
;	93/11/05 �����ƁA�w�b�_�[�ǂݍ��݂̒ǉ�
;	93/11/21 Initial: grppilod.asm/master.lib 0.21
;	93/11/24 �W�J��f�[�^��1byte 2pixel�����֕ύX (������Ɖ���)
;	93/12/ 7 [M0.22] palette���ύX, �g���̈悪����Ƃ��̏C��
;	93/12/10 [M0.22] �����Ɩ߂�l�ύX
;	95/ 2/ 6 [M0.22k] ��������̂Ƃ��ɕ���Ă����̂ŉ��Ή�
;	95/ 3/ 3 [M0.22k] ��������̂Ƃ��̃������m�ۏC��
;	95/ 4/ 1 [M0.22k] mem_AllocID�Ή�

	.186

	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN	DOS_ROPEN:CALLMODEL		; fontopen.asm
	EXTRN	HMEM_LALLOCATE:CALLMODEL	; memheap.asm
	EXTRN	SMEM_WGET:CALLMODEL		; smemwget.asm
	EXTRN	SMEM_RELEASE:CALLMODEL		; smemrel.asm

	BUFF_LEN EQU 1024*16
	EOF EQU 26

;	debug = 1

IFDEF ??version		; tasm check
	JUMPS
	WARN
ENDIF

.DATA
	EXTRN mem_AllocID:WORD			; mem.asm



; for pi decode
	xwidth		EQU word ptr DS:[BUFF_LEN]
	ywidth		EQU word ptr DS:[BUFF_LEN+2]
	gbuffer		EQU dword ptr DS:[BUFF_LEN+4]
	gbuffer_off	EQU word ptr DS:[BUFF_LEN+4]
	gbuffer_seg	EQU word ptr DS:[BUFF_LEN+6]
	gblast		EQU dword ptr DS:[BUFF_LEN+8]
	gblast_off	EQU word ptr DS:[BUFF_LEN+8]
	gblast_seg	EQU word ptr DS:[BUFF_LEN+10]
	ctable		EQU		 BUFF_LEN+12
	mode		EQU byte ptr DS:[BUFF_LEN+256+12]

; for bit_load
	handle		EQU word ptr DS:[BUFF_LEN+256+14]
	buff_p		EQU word ptr DS:[BUFF_LEN+256+16]
	bit_buf		EQU byte ptr DS:[BUFF_LEN+256+18]
	bit_len		EQU byte ptr DS:[BUFF_LEN+256+19]
BUFFERBLOCK_SIZE		=	(BUFF_LEN+256+20)

IFDEF debug
	public _dss,_ess,_dii,_sii

	_dss	DW 0
	_ess	DW 0
	_dii	DW 0
	_sii	DW 0

	_where	DW -1
ENDIF

.CODE

_too	macro	regoff,regwrk,regseg
	local	_toolabel
	or	regoff,regoff
	jnz	short _toolabel
	mov	regwrk,regseg
	add	regwrk,1000h
	mov	regseg,regwrk
	even
	_toolabel:
endm

_tor	macro	regdst,regwrk,regseg,regoff,value
	local	_torlabel
	mov	regdst,regseg:[regoff-value]
	cmp	regoff,value
	jnc	short _torlabel
	push	es
	mov	regwrk,regseg
	sub	regwrk,1000h
	mov	regseg,regwrk
	mov	regdst,regseg:[regoff-value]
	pop	es
_torlabel:
endm

_bitl	macro
	local	_nbbl_bil,_not_load_bilA

	push	CX
	mov	DH,0
	mov	DL,bit_buf
	mov	CL,bit_len
;if (bl < sz)
	cmp	CL,CH
	jnc	short _nbbl_bil
;shl bl : byteload : sz -= bl : bl = 8
	shl	DX,CL
	mov	SI,buff_p
	cmp	SI,BUFF_LEN
	jne	short _not_load_bilA
	call	_buffer_load
even
_not_load_bilA:
	mov	DL,[SI]
	inc	SI
	sub	CH,CL
	mov	CL,8
	mov	buff_p,SI
even
_nbbl_bil:
;shl sz : bl -= sz
	xchg	CL,CH
	shl	DX,CL
	sub	CH,CL
	mov	bit_buf,DL
	mov	bit_len,CH
	mov	DL,DH
	mov	DH,0
	pop	CX
endm

; --------- 8bit load ---------
_bitl8	macro
	local	_not_load_bilA
	push	CX
	mov	SI,buff_p
	cmp	SI,BUFF_LEN
	jne	short _not_load_bilA
	call	_buffer_load
even
_not_load_bilA:
	mov	DH,0
	mov	DL,[SI]
	inc	SI
	mov	buff_p,SI
	mov	CL,bit_len
	sub	CL,8	; CL = 8 - bitlen
	neg	CL
	shl	DX,CL
	xchg	DL,bit_buf
	or	DL,DH
	mov	DH,0
	pop	CX
endm

; IN
;	DH = 0
;	DL = bit_buf
;	CL = bit_len
; OUT
;	cy = LOAD VALUE
;	DL = bit_buf
; BREAK
;	DX SI
;
;	1bit load (�A���g�p�p)
_bitl1	macro
	local	_nbbl_bil,_not_load_bilA

;if (bl < sz)
	dec	CL
	jns	short _nbbl_bil
;shl bl : byteload : sz -= bl : bl = 8
	mov	SI,buff_p
	cmp	SI,BUFF_LEN
	jne	short _not_load_bilA
	call	_buffer_load
even
_not_load_bilA:
	mov	DL,[SI]
	inc	SI
	mov	CL,7
	mov	buff_p,SI
even
_nbbl_bil:
;shl sz : bl -= sz
	shl	DL,1
endm

EVEN
retfunc _errorA	; in: AX=error code
	pop	DI
	pop	SI
	pop	BP
	push	seg DGROUP
	pop	DS
	stc
	ret	(DATASIZE+DATASIZE+DATASIZE)*2
endfunc

func GRAPH_PI_LOAD_PACK	; graph_pi_load_pack() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	CLD

	; ����
	filename = (RETSIZE+1+DATASIZE+DATASIZE)*2
	header	 = (RETSIZE+1+DATASIZE)*2
	bufptr	 = (RETSIZE+1)*2

; file open
	_push	[bp+filename+2]
	push	[bp+filename]
	call	DOS_ROPEN
	jc	_errorA
	mov	BX,AX		; handle

; load buffer allocate
	push	offset BUFFERBLOCK_SIZE
	call	SMEM_WGET
	jc	_errorA

	mov	DS,AX		; data segment

	mov	handle,BX
	mov	bit_buf,0
	mov	bit_len,0

	mov	CX,BUFF_LEN
	xor	DX,DX
	mov	buff_p,DX
	mov	AH,3fh
	int	21h	; buffer load

; color table initialize
	mov	AX,DS
	mov	ES,AX
	mov	AX,1
	mov	CX,16
	mov	DI,OFFSET ctable
even
_ctinit_loop:
	and	AL,15
	stosb
	inc	AL
	inc	AH
	test	AH,00001111b
	jnz	short _ctinit_loop
	inc	AL
	loop	short _ctinit_loop

; header read
; Pi CHECK
	mov	AX,InvalidData
	call	byte_load
	cmp	DL,'P'
	jnz	_errorA
	call	byte_load
	cmp	DL,'i'
	jnz	_errorA

	_les	DI,[BP+header]
	s_mov	AX,<seg DGROUP>
	s_mov	ES,AX
	xor	AX,AX
	stosw
	stosw
	dec	AX
_comment_skip:
	inc	AX
	call	byte_load
	cmp	DL,EOF
	jnz	_comment_skip
	stosw
_dummy_skip:
	call	byte_load
	or	DL,DL
	jnz	_dummy_skip

; MODE
	call	byte_load
	mov	AL,DL
	stosb
	mov	mode,AL
; ASPECT RATIO
	call	byte_load
	mov	AH,DL
	call	byte_load
	mov	AL,DL
	stosw
	test	AX,AX
	mov	AX,InvalidData
	jnz	_errorA
; COLOR
	call	byte_load
	cmp	DL,4		; �F��=16�F?
	jnz	_errorA
	mov	AL,DL
	stosb
; MACHINE INFORMATION
	call	byte_load
	mov	AL,DL
	call	byte_load
	mov	AH,DL
	stosw
	call	byte_load
	mov	AL,DL
	call	byte_load
	mov	AH,DL
	stosw

	call	byte_load
	mov	AH,DL
	call	byte_load
	mov	AL,DL
	stosw
	mov	BX,AX	; BX = minfo length
	xor	AX,AX
	stosw		; �g���f�[�^ offset=0

	push	DS
	push	AX
	push	BX
	push	seg DGROUP
	pop	DS
	mov     mem_AllocID,MEMID_pi
	call	HMEM_LALLOCATE	; minfo buffer
	pop	DS
	stosw		; �g���f�[�^ segment
	jc	short _minfo_skipB
	push	ES
	push	DI
	mov	ES,AX
	xor	DI,DI
even
_minfo_skipA:
	call	byte_load
	mov	AL,DL
	stosb
	dec	BX
	jnz	short _minfo_skipA
	pop	DI
	pop	ES
even
_minfo_skipB:
; PICTURE WIDTH
	call	byte_load
	mov	AH,DL
	call	byte_load
	mov	AL,DL
	stosw
	mov	xwidth,AX
	mov	BX,AX
	call	byte_load
	mov	AH,DL
	call	byte_load
	mov	AL,DL
	stosw
	mov	ywidth,AX

;GBUFFER ALLOCATE
	;inc	BX	; (xwidth����̏ꍇ�ւ̍l��)
	;shr	BX,1	; half size (1byte = 2pixel)
	add	AX,2
	mul	BX	; BX = xwidth, DXAX = xwidth * (ywidth+2)
	shr	DX,1
	rcr	AX,1	; 2�Ŋ���
	push	DS
	push	DX	; ��hmem_lallocate()�̈���
	push	AX
	shl	DX,12	; long to segment (gblast�p)
	mov	gblast_off,AX	; �I���A�h���X
	mov	gblast_seg,DX
	push	seg DGROUP
	pop	DS
	mov     mem_AllocID,MEMID_pi
	call	HMEM_LALLOCATE		; �摜�̈�̊m��
	pop	DS
	mov	BX,AX
	mov	AX,InsufficientMemory
	jc	_errorA
	mov	AX,BX
	mov	gbuffer_off,0
	mov	gbuffer_seg,AX
	add	gblast_seg,AX	; add top address (segment)
	mov	CX,xwidth
	;add	CX,gbuffer_off
	_push	ES
	_les	BX,[BP+bufptr]
	mov	ES:[BX],CX
	mov	ES:[BX+2],AX	; gbuffer_seg
	_pop	ES

;PALETTE
	shl	mode,1
	jc	short _palette_skip
	mov	BL,48
even
_palette_load:
	call	byte_load
	mov	AL,DL
	stosb
	dec	BL
	jnz	short _palette_load
even
_palette_skip:
;header read end


;decode pi data
	xor	BL,BL
	call	_read_color
	mov	BL,DL
	mov	AL,DL
	shl	AL,4
	call	_read_color
	or	AL,DL
	les	DI,gbuffer
	mov	CX,xwidth
	rep	stosb		; fill 2 line

	mov	CL,-1
;�僋�[�v	CL == �O��ʒu�f�[�^ ES:DI == gbuffer
even
_main_decode_loopA:
;						position decode
	mov	CH,2
	_bitl
	mov	BL,DL
	cmp	DL,3
	jnz	short _position_decode_skip
	push	CX
	mov	DL,bit_buf
	mov	CL,bit_len
	mov	DH,0
	_bitl1
	mov	bit_buf,DL
	mov	bit_len,CL
	adc	BL,0
	pop	CX
even
_position_decode_skip:
	cmp	BL,CL
	jnz	short _decode_forkA

;�����A
	_tor	BL,AX,ES,DI,1
	and	BL,0fh
even
_decode_loopA:
	call	_read_color
	mov	BL,DL
	mov	AL,DL
	shl	AL,4
	call	_read_color
	mov	BL,DL
	or	AL,DL
	stosb
	_too	DI,AX,ES
;IFDEF debug
;	mov	[_where],0
;	call	_debug
;ENDIF
	mov	DL,bit_buf
	mov	CL,bit_len
	mov	DH,0
	_bitl1
	mov	bit_buf,DL
	mov	bit_len,CL
	jc	short _decode_loopA
	mov	CL,-1
	jmp	_end_checkB

; �����B	BP:DX = length
even
_decode_forkA:
; BL = position
	mov	BH,0
	xor	BP,BP
	mov	AX,1		; length
	mov	DL,bit_buf
	mov	CL,bit_len
even
loop_rlA:
	inc	BH		; count number of bit length
	_bitl1
	jc	short loop_rlA
	mov	bit_buf,DL
	mov	bit_len,CL
	mov	DH,0
	dec	BH
	jz	end_rlA
IF 1
	cmp	BH,8		; read length context
	jl	short skip_rlB
	mov	CX,BP
even
rlA1:
	_bitl8
	mov	CH,CL
	mov	CL,AH
	mov	AH,AL
	mov	AL,DL
	sub	BH,8
	cmp	BH,8
	jg	short rlA1
	mov	BP,CX
even
skip_rlB:
	mov	CH,BH
	_bitl
	mov	CL,CH
	mov	DH,AH
	shl	AX,CL
	or	AL,DL
	mov	DL,0
	rol	DX,CL
	mov	DH,0
	shl	BP,CL
	or	BP,DX
ELSE
even
loop_rlB:
	mov	CH,BH
	sub	BH,8
	jc	short skip_rlB
	mov	CH,8
even
skip_rlB:
	_bitl
	mov	CL,CH
	mov	CH,0
even
loop_rlC:
	shl	AX,1
	rcl	BP,1
	loop	short loop_rlC
	or	AL,DL
	cmp	BH,0
	jg	short loop_rlB
ENDIF
even
end_rlA:
	mov	CX,AX

	test	BL,BL
	jz	_position_zero	; position: 0      �O��2dot�������F�Ȃ�2dot��
				;                  �Ⴄ�F�Ȃ�4dot��
	mov	BH,0		; BH = pixel position
	mov	AX,xwidth
	cmp	BL,1
	je	short _ssC	; position: 1       1line���2dot
	cmp	BL,2
	jne	short _ssB
	shl	AX,1		; position: 2       2line���2dot
	adc	BH,BH
	jmp	short _ssC
even
_ssB:				; position: 3 or 4  1line���1dot{�E,��}��2dot
	dec	AX
	cmp	BL,3
	je	short _ssC
	add	AX,2		; position: 4       1line���1dot����2dot
even
_ssC:				; in: AX=address offset, BH=pixel offset(0or1)
	push	DS
	shr	BH,1
	rcr	AX,1
	sbb	BH,BH
	mov	SI,DI
	sbb	SI,AX		; ds:si = reference address
	mov	DX,ES
	jnb	short _nbs0
	sub	DX,1000h
even
_nbs0:
	mov	DS,DX
	or	BH,BH
	jnz	short _slAB
even
_slAA:			; pixel position equal byte arign
IF 1
	mov	AX,CX	; AX = original length
_slAA0:
	mov	CX,SI
	cmp	SI,DI
	ja	short _slAA1
	mov	CX,DI
_slAA1:
	neg	CX
	test	AX,AX
	jz	short _slAA2
	sub	CX,AX
	sbb	DX,DX
	and	CX,DX
	add	CX,AX	; CX = min(SI,DI,CX)
_slAA2:
	sub	AX,CX	; AX = (last CX) - new CX
	rep	movsb

	_too	DI,CX,ES
	_too	SI,CX,DS

	test	AX,AX
	jnz	short _slAA0
ELSE
	movsb
	_too	DI,AX,ES
	_too	SI,AX,DS
	loop	short _slAA
	sub	BP,1
	jnc	short _slAA
ENDIF
	pop	DS
	jmp	_end_check
even
_slAB:			; pixel position not equal byte align
;IFDEF debug
;	mov	[SS:_where],4
;	call	_debug
;ENDIF
	lodsb
	mov	AH,AL
	_too	SI,BP,DS
	mov	AL,[SI]
	shr	AX,4
	stosb
	_too	DI,AX,ES
	loop	short _slAB
	pop	DS
	jmp	short _end_check
even
_position_zero:			; position type zero( left 2 or 4 )
	_tor	DL,AX,ES,DI,1	; read left 2pixel -> DL
	mov	AL,DL
	ror	DL,4
	cmp	AL,DL
	jne	short _length_four
even
_slB:
;IFDEF debug
;	mov	[_where],2
;	mov	dl,al
;	call	_debug
;ENDIF
	stosb
	_too	DI,DX,ES
	loop	short _slB
	sub	BP,1
	jnc	short _slB
	jmp	short _end_check
even
_length_four:
	mov	BH,AL
	_tor	DL,AX,ES,DI,2
even
_slC:
;IFDEF debug
;	mov	[_where],3
;	call	_debug
;ENDIF
	mov	AL,DL
	stosb
	_too	DI,AX,ES
	loop	short _slD
	sub	BP,1
	jc	short _end_check
even
_slD:
	mov	AL,BH
	stosb
	_too	DI,AX,ES
	loop	short _slC
	sub	BP,1
	jnc	short _slC
even
_end_check:
	mov	CL,BL
even
_end_checkB:
	cmp	gblast_off,DI
	jnbe	_main_decode_loopA
	mov	AX,ES
	cmp	gblast_seg,AX
	jnbe	_main_decode_loopA

;file close
	mov	AH,3eh
	mov	BX,handle
	int	21h

	push	DS
	mov	BX,seg DGROUP
	mov	DS,BX
	call	SMEM_RELEASE		; ���S���W�X�^�ۑ�������

	clc
	mov	AX,NoError
	pop	DI
	pop	SI
	pop	BP
	ret	(DATASIZE+DATASIZE+DATASIZE)*2
endfunc			; }

;	IN
;	BL = �����ǂ��ƍ��̐F
; OUT
;	DL = �F
; BREAK
;	DL
even
_read_color proc near
	push	AX
	push	BX
	push	CX
; �F�����̉��(�O��)
	xor	AX,AX
	mov	DL,bit_buf
	mov	CL,bit_len
	_bitl1
	jc	short _skip_rc
	add	AL,2
	_bitl1
	jnc	short _skip_rc
	add	AL,2
	inc	AH
	_bitl1
	jnc	short _skip_rc
	add	AL,4
	inc	AH
even
_skip_rc:
	mov	bit_buf,DL
	mov	bit_len,CL
; �F�����̉��(�㔼)
	mov	CH,AH
	inc	CH
	_bitl
	add	AL,DL
	xor	AL,15
; �F�\�̃A�h���X�Z��
	mov	AH,0
	mov	BH,0
	shl	BX,4
	add	BX,OFFSET ctable
	add	BX,AX
; �F�\�̍X�V�ƐF�̓ǂ݂���
	mov	CX,15
	mov	DH,0
	mov	DL,[BX]
	sub	CX,AX
	jne	short _loop_rcA

	pop	CX
	pop	BX
	pop	AX
	ret
even
_loop_rcA:			; �F���Q�Ƃ����̂ŁA���̃f�[�^��擪�ɕ���
	mov	AL,[BX+1]
	mov	[BX],AL
	inc	BX
	loop	short _loop_rcA

	mov	[BX],DL

	pop	CX
	pop	BX
	pop	AX
	ret
_read_color endp




;	IN
; OUT
;	DH = 0
;	DL = LOAD VALUE(Signed char)
; BREAK
;	DX SI
even
byte_load proc near
	_bitl8
	ret
byte_load endp
even
_buffer_load proc near
	pusha	; (DX CX BX AX)
	mov	AH,3fh
	mov	BX,handle
	mov	CX,BUFF_LEN
	xor	DX,DX
	int	21h
	popa
	xor	SI,SI
	ret
_buffer_load endp

IFDEF debug
	EXTRN	C printf:CALLMODEL

_debug proc near
	pusha
	push	ds
	push	es

	push	AX
	mov	AX,@data
	mov	DS,AX
	pop	AX
;    s_ <call	printf c,offset dbg,WORD PTR [gblast],SI,AX,BX,CX,DX,WORD PTR [_where]>
;    l_ <call	printf c,offset dbg,seg dbg,WORD PTR [gblast],SI,AX,BX,CX,DX,WORD PTR [_where]>
     s_ <call	printf c,offset dbg,ES,DI,AX,SI,CX,BP,BX,DX,WORD PTR [_where]>
     l_ <call	printf c,offset dbg,seg dbg,ES,DI,AX,SI,CX,BP,BX,DS,WORD PTR [_where]>

	pop	es
	pop	ds
	popa
	ret
_debug endp

.data
;	dbg	db "ese %04X SI %04X AX %04X BX %04X CX %04X DX %04X wh %d",10,0
	dbg	db "ES %04X DI %04X DS %04X SI %04X CX %04X BP %04X BX %04X DX %04X wh %d",10,0

ENDIF

END
