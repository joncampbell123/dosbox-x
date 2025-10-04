; master library - MS-DOS
;
; Description:
;	�����������擾�œ����������̊J��
;
; Functions:
;	void smem_release( unsigned memseg ) ;
;
; Parameters:
;	memseg	�J������[smem_wget,smem_lget�̂ǂ��炩�œ���]�Z�O�����g�A�h���X
;
; Returns:
;	none
;
; Notes:
;	���W�X�^�͑S�ĕۑ����܂��B
;
; Revision History:
;	93/ 3/20 Initial
;

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN mem_EndMark:WORD

	.CODE

	; void smem_release( unsigned memseg ) ;
func SMEM_RELEASE
	push	BX
	mov	BX,SP
	; ����
	memseg	= (RETSIZE+1)*2
	mov	BX,SS:[BX+memseg]
	mov	mem_EndMark,BX
	pop	BX
	ret	2
endfunc

END
