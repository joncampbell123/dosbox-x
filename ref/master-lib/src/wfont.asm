; master library - wfont
;
; Description:
;	���k�t�H���g�֘A�f�[�^
;
; Variables:
;	wfont_Plane1	�������C���`��p GRCG���[�h���W�X�^�ݒ�l
;	wfont_Plane2	����C���`��p GRCG���[�h���W�X�^�ݒ�l
;	wfont_AnkSeg	���k�t�H���g�̊i�[�Z�O�����g
;
; Binding Target:
;	Microsoft-C / Turbo-C
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
;	Kazumi(���c  �m)
;
; Revision History:
;	93/ 7/ 2 Initial:wfont.asm/master.lib 0.20
;	95/ 1/31 [M0.23] wfont_Reg�ϐ��ǉ�


	.MODEL SMALL
	.DATA
	public _wfont_Plane1,wfont_Plane1
wfont_Plane1 label byte
_wfont_Plane1	db 11001110b
	public _wfont_Plane2,wfont_Plane2
wfont_Plane2 label byte
_wfont_Plane2	db 11001101b
	public _wfont_Reg,wfont_Reg
wfont_Reg label byte
_wfont_Reg	db 0ffh
	public _wfont_AnkSeg,wfont_AnkSeg
wfont_AnkSeg label word
_wfont_AnkSeg	dw 0

END
