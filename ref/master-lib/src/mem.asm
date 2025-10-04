; master library
;
; Description:
;	�������}�l�[�W���̍�ƕϐ�
;
; Variables:
;	mem_TopSeg	�������}�l�[�W�����Ǘ�����擪�̃Z�O�����g
;	mem_OutSeg	�������}�l�[�W�����Ǘ����閖���̎��̃Z�O�����g
;	mem_EndMark	�X�^�b�N�^�������}�l�[�W���̎��Ɋm�ۂ���ʒu
;	mem_TopHeap	�q�[�v�^�������}�l�[�W���̊m�ۂ����擪
;	mem_FirstHole	�q�[�v�^�������}�l�[�W���̊J�����ŏ��̌�
;	mem_AllocID	�������m�ێ��Ƀu���b�N�Ǘ����������܂��p�rID
;       mem_Reserve     mem_assign_all�Ŋm�ۂ���Ƃ��Ɏc���傫��
;
; Revision History:
;	93/ 6/12 [M0.19] �o�[�W���������񋭐������N
;	93/ 9/15 [M0.21] mem_TopSeg �������[��������
;	95/ 2/14 [M0.22k] mem_AllocID�ǉ�
;       95/ 3/ 3 [M0.22k] mem_Reserve�ǉ�

	.MODEL SMALL

	; �o�[�W����������������N����
	EXTRN _Master_Version:BYTE

	.DATA
	public mem_TopSeg,  mem_OutSeg
	public _mem_TopSeg, _mem_OutSeg
	public mem_TopHeap,  mem_FirstHole,  mem_EndMark
	public _mem_TopHeap, _mem_FirstHole, _mem_EndMark
	public mem_MyOwn, _mem_MyOwn
	public mem_AllocID, _mem_AllocID
	public mem_Reserve, _mem_Reserve

_mem_TopSeg label word
mem_TopSeg	dw	0
	public mem_MyOwn
_mem_MyOwn label word
mem_MyOwn	dw	0

	public mem_AllocID
_mem_AllocID label word
mem_AllocID	dw	0

	public mem_Reserve
_mem_Reserve label word
mem_Reserve	dw	256		; 4096 bytes�󂯂�

	.DATA?
_mem_OutSeg label word
mem_OutSeg	dw	?
_mem_TopHeap label word
mem_TopHeap	dw	?
_mem_FirstHole label word
mem_FirstHole	dw	?
_mem_EndMark label word
mem_EndMark	dw	?

END
