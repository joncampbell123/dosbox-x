; master library
;
; Description:
;	�O���t�B�b�N��ʊ֘A�̃O���[�o���ϐ���`
;
; Global Variables:
;	graph_VramSeg	�O���t�B�b�N��ʂ̐擪�Z�O�����g�A�h���X
;	graph_VramWords	�O���t�B�b�N��ʂ�1�v���[��������̃��[�h��
;	graph_VramLines	�O���t�B�b�N��ʂ̏c�h�b�g��
;	graph_VramWidth	�O���t�B�b�N��ʂ�1���C��������̃o�C�g��
;	graph_VramZoom	����8bit: �O���t�B�b�N��ʂ̏c�{�V�t�g��
;			���8bit: GDC5MHz�Ȃ�40h, 2.5MHz�Ȃ�0(graph_[24]00line)
;			(��L��98�̏ꍇ�̂�)
;	graph_MeshByte	{grcg,vgc}_bytemesh_x�̃��b�V���p�^�[��(����line)
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/16 Initial
;	92/11/22 VSYNC�J�E���^�ǉ�
;	93/ 2/10 vsync_Proc�ǉ�
;	93/ 6/12 [M0.19] �o�[�W���������񋭐������N
;	93/ 8/ 8 vsync�֘A��vs.asm�Ɉړ�
;	93/12/ 4 [M0.22] graph_VramWidth��ǉ�
;	94/ 1/22 [M0.22] graph_VramZoom��ǉ�
;	95/ 1/20 [M0.23] graph_MeshByte��ǉ�
;
	.MODEL SMALL

	; �o�[�W����������������N����
	EXTRN _Master_Version:BYTE

	.DATA

	public _graph_VramSeg,graph_VramSeg
graph_VramSeg label word
_graph_VramSeg	dw 0a800h
	public _graph_VramWords,graph_VramWords
graph_VramWords label word
_graph_VramWords	dw 16000
	public _graph_VramLines,graph_VramLines
graph_VramLines label word
_graph_VramLines	dw 400
	public _graph_VramWidth,graph_VramWidth
graph_VramWidth label word
_graph_VramWidth	dw 80
	public _graph_VramZoom,graph_VramZoom
graph_VramZoom label word
_graph_VramZoom	dw 0

	public _graph_MeshByte,graph_MeshByte
graph_MeshByte label word
_graph_MeshByte	db 55h

END
