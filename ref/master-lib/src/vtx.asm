; master library - PC/AT - graph - text
;
; Description:
;	�E�O���t�B�b�N���[�h�ɂ�DOS/V emulation text�̋@�\��t������

;	vtext_colortable: DOS/V emulation text �̃G�~�����[�V�����p�F�ϊ��z��
;	vtextx_Seg: vtextx�Ɋm�ۂ��ꂽ���zVRAM�̃Z�O�����g
;	vtextx_Size: vtextx�Ɋm�ۂ��ꂽ���zVRAM�̑傫��(TextVramSize�Ɠ����P��)
;
	.MODEL SMALL
	.DATA
	public vtext_colortable, _vtext_colortable
_vtext_colortable label byte
vtext_colortable db 0,9,0ch,0dh,0ah,0bh,0eh,0fh
		 db 8,1,4,5,2,3,6,7

	public vtextx_Seg, _vtextx_Seg
_vtextx_Seg label word
vtextx_Seg	dw	0

	public vtextx_Size, _vtextx_Size
_vtextx_Size label word
vtextx_Size	dw	0
END
