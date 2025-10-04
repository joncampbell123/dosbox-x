; master library module
;
; Variables:
;	TextVramAdr:
;	        PC-9801 series: TextVramSeg:0
;	TextVramSeg:
;		PC-9801 Normal Mode : 0xA000
;		PC-9801 HiReso Mode : 0xE000
;
;	TextVramWidth:
;		�e�L�X�g��ʂ̉�������
;
; Revision History:
;	92/11/15 Initial
;	92/11/22 for Turbo Pascal
;	93/ 6/12 [M0.19] �o�[�W���������񋭐������N
;	93/ 9/14 [M0.21] TextVramAdr�ǉ�
;	94/ 1/ 8 [M0.22] TextShown�ǉ�
;	94/ 4/ 9 [M0.23] TextVramWidth, TextVramSize�ǉ�

	.MODEL SMALL

	; �o�[�W����������������N����
	EXTRN _Master_Version:BYTE

	.DATA

	public _TextVramAdr,TextVramAdr
	public _TextVramSeg,TextVramSeg
	public _TextVramWidth,TextVramWidth
	public _TextShown,TextShown
TextVramAdr label dword
_TextVramAdr label dword
	    dw 0
TextVramSeg label word
_TextVramSeg dw 0a000h

TextVramWidth label word
_TextVramWidth dw 80

TextShown label word
_TextShown dw 1

END
