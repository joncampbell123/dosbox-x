; master library
;
; Description:
;	�t�@�C���A�N�Z�X�֘A�̃O���[�o���ϐ���`
;
; Global Variables:
;	file_Buffer		��ƃo�b�t�@�̃A�h���X
;	file_BufferSize		��ƃo�b�t�@�̑傫��(�o�C�g�P��)
;
;	file_BufferPos		��ƃo�b�t�@�擪�̃t�@�C�����ʒu(0=�擪)
;	file_BufPtr		��ƃo�b�t�@�̃J�����g�|�C���^(0=Bufptr)
;	file_InReadBuf		��ƃo�b�t�@�ɓǂݍ��܂ꂽ�o�C�g��
;
;	file_Eof		EOF�ɒB�����t���O
;	file_ErrorStat		�������݃G���[�t���O
;
;	file_Handle		�t�@�C���n���h��( -1=closed )
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/17 Initial
;	92/11/29 file_BufferPos�ǉ�
;	93/ 2/ 4 file_Pointer...

	.MODEL SMALL

	.DATA?

	public _file_Pointer,file_Pointer
_file_Pointer label dword
file_Pointer	dd	?

	public _file_Buffer,file_Buffer
_file_Buffer label dword
file_Buffer	dd	?

	public _file_BufferPos,file_BufferPos
_file_BufferPos label dword
file_BufferPos dd	?

	public _file_BufPtr,file_BufPtr
_file_BufPtr label word
file_BufPtr	dw	?

	public _file_InReadBuf,file_InReadBuf
_file_InReadBuf label word
file_InReadBuf	dw	?

	public _file_Eof,file_Eof
_file_Eof label word
file_Eof	dw	?

	public _file_ErrorStat,file_ErrorStat
_file_ErrorStat label word
file_ErrorStat	dw	?

	.DATA

	public _file_BufferSize,file_BufferSize
_file_BufferSize label word
file_BufferSize	dw	0

	public _file_Handle,file_Handle
_file_Handle label word
file_Handle	dw	-1

END
