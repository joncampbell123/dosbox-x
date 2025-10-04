; hiper library
;
; Description:
;	PC/AT �� video mode �𓾂�
;
; Procedures/Functions:
;	unsigned get_video_mode( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	�r�f�I���[�h (AT�݊��@�łȂ���� 0 ���Ԃ�)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	�̂�/V
;
; Revision History:
;	93/07/21 Initial
;	93/08/10 �֐������� _ �����
;	93/12/27 VESA�Ή�? by ����
;	93/12/27 Initial: setvideo.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN Machine_State : WORD

	.CODE

func GET_VIDEO_MODE	; get_video_mode() {
	xor	AX,AX
	test	Machine_State,0010h	; PC/AT ��������
	jz	short FAILURE

	test	Machine_State,1	; �p�ꃂ�[�h?
	jz	short OLDTYPE	; ���{��Ȃ疳�����ɕ��ʂ�BIOS�Ŏ��
	mov	AX,4f03h	; VESA get video mode
	int	10h
	cmp	AX,4fh
	jne	short OLDTYPE
	mov	AX,BX
	cmp	AX,100h		; VESA��100h�ȏ�Ȃ�m��, �����Ȃ��蒼��
	jnc	short SUCCESS
OLDTYPE:
	mov	AH,0fh
	int	10h
	mov	AH,0
SUCCESS:
FAILURE:
	ret
endfunc			; }

END
