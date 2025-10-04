; master library - FEP
;
; Description:
;	���{��FEP�̐���
;
; Function/Procedures:
;	int fep_exist(void) ;
;	int fep_shown(void) ;
;	void fep_show(void) ;
;	void fep_hide(void) ;
;
; Parameters:
;	none
;
; Returns:
;	fep_exist:
;		FEPTYPE_UNKNOWN	���݂��Ȃ�
;		FEPTYPE_IAS	$IAS
;		FEPTYPE_MSKANJI	MS-KANJI API
;	fep_shown:
;		0 �����Ă���
;		1 ���Ă�
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	���{��MS-DOS, DR-DOS/V, AX-DOS, MS-DOS/V, IBM DOS/V, IBM ���{��DOS
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	���O�ɕK�� get_machine�����s���Ȃ��� $IAS�͌������܂���
;	���̌�Afep_exist�����s���Ȃ��Ƒ���fep_*�͓��삵�܂���
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	94/ 1/12 Initial: fepexist.asm/master.lib 0.22
;	94/ 3/ 3 [M0.23] BUGFIX fep_exist �߂�l��fep_type�̒l��Ԃ��Ȃ�����

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	Machine_State:WORD
PCAT	EQU	00010000b

fep_type	dw 0		; FEP�̎��

IasDevName	db '$IBMAIAS',0	; $IAS.SYS�̃f�o�C�X��
MsKanjiDevName	db 'MS$KANJI',0	; MS-KANJI�̃f�o�C�X��
MsKanjiAPIEntry	dd 0		; MS-KANJI�̃G���g���A�h���X

FEPTYPE_UNKNOWN equ 0
FEPTYPE_IAS	equ 1
FEPTYPE_MSKANJI	equ 2

; MS Kanji API function code (wFunc)
KKAsk	equ 1	; �J�i�����ϊ��V�X�e�����𒲂ׂ�
KKOpen	equ 2	; �J�i�����ϊ��V�X�e���̎g�p���J�n����
KKClose	equ 3	; �J�i�����ϊ��V�X�e���̎g�p���I������
KKInOut	equ 4	; �J�i�����ϊ��V�X�e���Ƃ̃f�[�^���o�͂��s��
KKMode	equ 5	; �J�i�������̓��[�h�� ON/OFF ���Q�Ɓ^�ݒ肷��

Funcparm	STRUC
wFunc		dw	?	; �t�@���N�V�����ԍ�
wMode		dw	?	; �������̓��[�h�̎Q�Ɓ^�ݒ�̃t���O
lpKkname	dd	0	; �J�i�����ϊ��V�X�e�����p�\���̂ւ̃|�C���^
lpDataparm	dd	0	; �f�[�^��p�����[�^�p�\���̂ւ̃|�C���^
Reserved	dd	0	; �g���p (�S�v�f 0 �Ƃ��Ă���)
Funcparm	ENDS

Dataparm	STRUC
wType		dw	?	; �\���؊��X�C�b�`
wScan		dw	?	; �L�[��X�L������R�[�h
wAscii		dw	?	; �����R�[�h
wShift		dw	?	; �V�t�g��L�[��X�e�[�^�X
wExShift	dw	?	; �g���V�t�g��L�[��X�e�[�^�X

cchResult	dw	?	; �m�蕶����̃o�C�g��
lpchResult	dd	?	; �m�蕶����̊i�[�|�C���^

cchMode		dw	?	; ���[�h�\��������̃o�C�g��
lpchMode	dd	?	; ���[�h�\��������̊i�[�|�C���^
lpattrMode	dd	?	; ���[�h�\��������̑����̊i�[�|�C���^

cchSystem	dw	?	; �V�X�e��������̃o�C�g��
lpchSystem	dd	?	; �V�X�e��������̊i�[�|�C���^
lpattrSystem	dd	?	; �V�X�e��������̑����̊i�[�|�C���^

cchBuf		dw	?	; ���m�蕶����̃o�C�g��
lpchBuf		dd	?	; ���m�蕶����̊i�[�|�C���^
lpattrBuf	dd	?	; ���m�蕶����̑����̊i�[�|�C���^
cchBufCursor	dw	?	; ���m�蕶���񒆂̃J�[�\���ʒu

DReserved	db 34 dup (?)	; �g���p (�S�v�f 0 �Ƃ��Ă���)
Dataparm	ENDS


; DBCS keyboard�̏󋵃��[�h(IAS)
IAS_KANJI    equ 10000000b
IAS_ROMA     equ 01000000b
IAS_HIRAGANA equ 00000100b
IAS_KATAKANA equ 00000010b
IAS_EISUU    equ 00000000b
IAS_ZENKAKU  equ 00000001b

	.CODE

func FEP_EXIST	; fep_exist() {
	mov	fep_type,FEPTYPE_UNKNOWN
	test	Machine_State,PCAT
	jz	short TEST_MSKANJI

	; $IAS�����݂��邩��?
	mov	DX,offset IasDevName
	mov	AX,3d00h		; open
	int	21h
	jc	short TEST_MSKANJI
	mov	BX,AX
	mov	AH,3eh			; close
	int	21h
	mov	fep_type,FEPTYPE_IAS
	jmp	short EXIST_DONE
	EVEN

	; MS KANJI API�����݂��邩��?
TEST_MSKANJI:
	mov	DX,offset MsKanjiDevName
	mov	AX,3d00h
	int	21h
	jc	short EXIST_DONE
	mov	BX,AX

	mov	DX,offset MsKanjiAPIEntry
	mov	CX,4
	mov	AX,4402h		; IOCTL read
	int	21h
	push	AX
	mov	AH,3eh			; close
	int	21h
	pop	AX
	cmp	AX,4
	jne	short EXIST_DONE
	mov	fep_type,FEPTYPE_MSKANJI
EXIST_DONE:
	mov	AX,fep_type
	ret
endfunc		; }


; In:
;	CX = wFunc
;	AX = wMode
; Out:
;	AX = wMode
KKfunc	proc	near
	push	BP
	mov	BP,SP
	sub	SP,type Funcparm
	Funcbuf EQU [BP-(type Funcparm)]

	cmp	word ptr MsKanjiApiEntry+2,0
	je	short KKfunc_done

	mov	Funcbuf.wFunc,CX
	mov	Funcbuf.wMode,AX
	xor	BX,BX
	mov	word ptr Funcbuf.lpKkname,BX
	mov	word ptr Funcbuf.lpKkname+2,BX
	mov	word ptr Funcbuf.lpDataparm,BX
	mov	word ptr Funcbuf.lpDataparm+2,BX
	mov	word ptr Funcbuf.Reserved,BX
	mov	word ptr Funcbuf.Reserved+2,BX
	push	SS
	lea	AX,Funcbuf
	push	AX
	call	MsKanjiAPIEntry		; MS-KANJI�̌Ăяo��
	mov	AX,Funcbuf.wMode
KKfunc_done:
	mov	SP,BP
	pop	BP
	ret
KKfunc	endp

func FEP_SHOWN	; fep_shown() {
	cmp	fep_type,FEPTYPE_IAS	; 1
	je	short SHOWN_IAS
	jc	short HIDDEN		; 0
SHOWN_MSKANJI:
	mov	CX,KKMode
	mov	AX,0			; �Q��
	call	KKfunc
	test	AX,2			; ON bit
	jnz	short SHOWN
HIDDEN:
	xor	AX,AX
	ret

SHOWN_IAS:
	mov	AX,1402h	; �V�t�g�󋵂̎擾
	int	16h
	cmp	AL,0		; �V�t�g�󋵂͕\��?
	jne	short HIDDEN
	mov	AX,1301h	; DBCS keyboard mode�̎擾
	int	16h
	test	DL,IAS_KANJI
	jz	short HIDDEN
SHOWN:
	mov	AX,1
	ret
endfunc		; }

func FEP_SHOW	; fep_show() {
	cmp	fep_type,FEPTYPE_IAS	; 1
	je	short ON_IAS
	jc	short ON_DONE		; 0
ON_MSKANJI:
	mov	CX,KKMode
	mov	AX,0			; �Q��
	call	KKfunc
	test	AX,2			; ON bit
	jnz	short ONMS_DONE
	mov	CX,KKMode
	xor	AX,8003h		; ON/OFF����, �擾/�ݒ����
	call	KKfunc
ONMS_DONE:
	ret
	EVEN

ON_IAS:
	mov	AX,1402h	; �V�t�g�󋵂̎擾
	int	16h
	cmp	AL,1		; �V�t�g�󋵂͏���?
	jne	short ONIAS_1
	mov	AX,1400h	; �����Ă���Ȃ�V�t�g�\��
	int	16h
ONIAS_1:
	cmp	AL,0		; �V�t�g�󋵂͕\��?
	jne	short ON_DONE
	mov	AX,1301h	; DBCS keyboard mode�̎擾
	int	16h
	test	DL,IAS_KANJI
	jnz	short ON_DONE
	or	DL,IAS_KANJI
	test	DL,IAS_ROMA
	jnz	short ONIAS_2
	or	DL,IAS_ZENKAKU+IAS_HIRAGANA
ONIAS_2:
	mov	AX,1300h	; DBCS keyboard mode�̐ݒ�
	int	16h
ON_DONE:
	ret
endfunc		; }

func FEP_HIDE	; fep_hide() {
	cmp	fep_type,FEPTYPE_IAS	; 1
	je	short OFF_IAS
	jc	short OFF_DONE		; 0
OFF_MSKANJI:
	mov	CX,KKMode
	mov	AX,0			; �Q��
	call	KKfunc
	test	AX,1			; OFF bit
	jnz	short OFFMS_DONE
	mov	CX,KKMode
	xor	AX,8003h		; ON/OFF����, �擾/�ݒ����
	call	KKfunc
OFFMS_DONE:
	ret

OFF_IAS:
	mov	AX,1402h	; �V�t�g�󋵂̎擾
	int	16h
	cmp	AL,0
	jne	short OFF_DONE
	mov	AX,1301h	; DBCS keyboard mode�̎擾
	int	16h
	cmp	DL,IAS_KANJI
	jz	short OFF_DONE
	xor	DL,IAS_KANJI
	mov	AX,1300h	; DBCS keyboard mode�̐ݒ�
	int	16h
OFF_DONE:
	ret
endfunc		; }

END
