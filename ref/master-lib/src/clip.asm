; graphics - clipping
;
; DESCRIPTION:
;	�`��N���b�v�g�̐ݒ�ϐ��i�����l���j
;
; GLOBAL VARIABLES:
;	int ClipXL, ClipXW, ClipXR	���[x, �g�̉���(xr-xl), �E�[x
;	int ClipYT, ClipYH, ClipYB	��[y, �g�̍���(yb-yt), ���[y
;	unsigned ClipYB_adr		���[�̍��[��VRAM�I�t�Z�b�g
;	unsigned ClipYT_seg		��[��VRAM�Z�O�����g(�v���[��)
;
; RUNNING TARGET:
;	NEC PC-9801 NORMAL MODE
;
; REFER:
;	  grc_setclip()
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; AUTHOR:
;	���ˏ��F
;
; HISTORY:
;	92/5/16 Initial/gc_poly.lib
;	92/6/28 copyright�ǉ�
;	92/6/30 version 0.10
;	92/7/16 version 0.11
;	92/7/17 version 0.12
;	92/7/27 version 0.13
;	92/7/29 version 0.14
;	92/10/5 version 0.15
;	version 0.16
;	93/ 3/27 master.lib�ɍ���
;	93/ 6/12 [M0.19] �o�[�W���������񋭐������N

	.MODEL SMALL

	; �o�[�W����������������N����
	EXTRN _Master_Version:BYTE

	.DATA

	public ClipXL, ClipXW, ClipXR, ClipYT, ClipYH, ClipYB
	public ClipYT_seg,ClipYB_adr
	public _ClipXL, _ClipXW, _ClipXR, _ClipYT, _ClipYH, _ClipYB
	public _ClipYT_seg,_ClipYB_adr
_ClipXL label word
ClipXL	dw 0	; ���[(0)
_ClipXW label word
ClipXW dw 639	; ��(639)
_ClipXR label word
ClipXR dw 639	; �E�[
_ClipYT label word
ClipYT dw 0	; �Ă��؂�(0)
_ClipYH label word
ClipYH	dw 399	; ����(399)
_ClipYB label word
ClipYB dw 399	; ���[(399)
_ClipYT_seg label word
ClipYT_seg dw 0a800h	; ��[��VRAM�Z�O�����g(�v���[����)
_ClipYB_adr label word
ClipYB_adr dw 399*80	; ���[��VRAM�I�t�Z�b�g


	;��`�f�[�^
.DATA?
	public trapez_a,trapez_b
	public _trapez_a,_trapez_b
_trapez_a label word
trapez_a dw 4 dup(?)
_trapez_b label word
trapez_b dw 4 dup(?)

END
