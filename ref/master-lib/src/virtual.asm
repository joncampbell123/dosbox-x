; superimpose & master library module
; Description:
;	���zVRAM�̃Z�O�����g�A�h���X��ێ�����ϐ�
;
; Variables:
;	virtual_seg
;
; Notes:
;	vircopy.asm �� virtual_copy(),virtual_vram_copy(), 
;	vg4vcopy.asm �� vga4_virtual_copy(),vga4_virtual_vram_copy()
;	�Ȃǂ���g�p����܂��B
;
; Revision History:
;	94/10/21 �����l���s�肾�����̂�0�ɒ���

	.MODEL SMALL
	.DATA
	PUBLIC	virtual_seg,_virtual_seg
_virtual_seg label word
virtual_seg	DW	0

	PUBLIC	virtual_vramwords,_virtual_vramwords
_virtual_vramwords label word
virtual_vramwords	DW	0

END
