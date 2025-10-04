; master library - PC98 - MSDOS
;
; Description:
;	CRTC�ɂ��e�L�X�g��ʃX���[�X�X�N���[������
;
; Function/Procedures:
;	void text_smooth_start( unsigned y1, unsigned y2 ) ;
;	void text_smooth_end(void) ;
;
; Parameters:
;	unsigned y1	�X�N���[���J�n�s(0�`)
;	unsigned y2	�X�N���[���I���s(0�`)
;
; Returns:
;	none
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
;	y1 <= y2�łȂ���΂Ȃ�Ȃ��B�����łȂ���Γ���͕s��ł���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 2/11 Initial
;

	.MODEL SMALL
	include func.inc
	.CODE

func TEXT_SMOOTH_START
	mov	BX,SP
	; ����
	y1	= (RETSIZE+1)*2
	y2	= (RETSIZE+0)*2
	mov	AL,SS:[BX+y1]
	not	AX
	out	78h,AL		; SUR
	stc
	adc	AL,SS:[BX+y2]
	out	7Ah,AL		; SDR
	ret	4
endfunc

func TEXT_SMOOTH_END
	xor	AX,AX
	out	76h,AL		; SSR
	push	AX
	mov	ES,AX
	mov	AL,ES:[0712h]
	push	AX
	call	TEXT_SMOOTH_START
	ret
endfunc

END
