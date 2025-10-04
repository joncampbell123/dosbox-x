PAGE 98,120
; graphics - tile
;
; DESCRIPTION:
;	�^�C���p�^�[���̐ݒ�
;
; FUNCTION:
;	void far _pascal grc_settile( char * tile, int tilelen ) ;
;	void far _pascal grc_settile_solid( int color ) ;

; PARAMETERS:
;	char * tile	
;	int len		
;
;	int color	
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C
;
; RUNNING TARGET:
;	NEC PC-9801 Normal mode
;
; REQUIRING RESOURCES:
;	CPU: V30
 ;
; COMPILER/ASSEMBLER:
;	OPTASM 1.6
;
; NOTES:
;	�@grcg_settile()�Őݒ肷��̂́A�u�^�C���p�^�[�����́v�ł�
;	�Ȃ��A�u�^�C���p�^�[���̊i�[���ꂽ�A�h���X�v�ł��邱�Ƃɒ���
;	���Ă��������B�Ƃ������Ƃ́Agrcg_settile()�Ăяo����A�ݒ肵��
;	�A�h���X����̃^�C���f�[�^��ύX����ƁA���̌�̕`��p�^�[��
;	���ύX����邱�ƂɂȂ�܂��B�܂��A�^�C���p�^�[�����i�[���Ă���
;	���������J������Ƃ��́A���grcg_settile_solid()�Ȃǂ��g����
;	�^�C���p�^�[���̃A�h���X��ύX���Ă��������B
;
; AUTHOR:
;	���ˏ��F
;
; �֘A�֐�:
;	
;
; HISTORY:
;	92/6/13	Initial

	.186
	.MODEL SMALL
	.DATA?

_TileLen	dw	?	; �^�C���̃��C����
_TileStart	dw	?	; �^�C���̐擪�A�h���Xoffset
_TileSeg	dw	?	; �^�C���̐擪�A�h���Xsegment
_TileLast	dw	?	; �^�C���̍Ō�̃��C���̐擪offset

	EXTRN _TileSolid:WORD

	.CODE
	include func.inc

; int far pascal grc_settile( char * tile, int tilelen ) ;
func GRC_SETTILE
	enter 0,0
	mov	BX,[BP+(RETSIZE+2)*2]	; tile
	mov	CX,[BP+(RETSIZE+1)*2]	; tilelin
	xor	AX,AX
	or	CX,CX
	jle	short RETURN	; ���C�������[���ȉ����Ƒʖ�

	mov	_TileStart,BX
	mov	_TileSeg,DS	; ���[�W�f�[�^���f�����Ή�
	mov	_TileLen,CX
	dec	CX
	shl	CX,2
	add	BX,CX
	mov	_TileLast,BX
	mov	AX,1
RETURN:
	leave
	ret 4+(DATASIZE+1)*2
endfunc


; void far pascal grc_settile_solid( int color ) {
func GRC_SETTILE_SOLID
	enter 0,0
	mov	BX,[BP+(RETSIZE+1)*2]	; color

	and	BX,0Fh
	shl	BX,2
	add	BX,offset _TileSolid
	mov	_TileStart,BX
	mov	_TileLast,BX
	mov	_TileSeg,seg _TileSolid
	mov	_TileLen,1
	leave
	ret 2
endfunc
END
