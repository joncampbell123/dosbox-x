; master library - PC-9801 - vsync - enter
;
; Description:
;	VSYNC���荞�݁E�`�F�C����
;
; Function/Procedures:
;	void vsync_enter(void) ;
;
; Parameters:
;	none
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
;	vsync_enter()�����s������A�K���v���O�����I���O��vsync_leave()��
;	���s���Ă��������B
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
;	93/ 8/25 Initial: vsyncent.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc
	EXTRN DOS_SETVECT:CALLMODEL

	.DATA
	EXTRN vsync_Count1 : WORD	; ������������J�E���^1
	EXTRN vsync_Count2 : WORD	; ������������J�E���^2
	EXTRN vsync_Proc : DWORD

	EXTRN vsync_OldVect : DWORD	; int 0ah(vsync)
	EXTRN vsync_OldMask : BYTE

	.CODE
	EXTRN VSYNC_START:CALLMODEL

VSYNC_VECT	EQU 0ah
IMR		EQU 2	; ���荞�݃}�X�N���W�X�^
VSYNC_DISABLE	EQU 4

chain_int	proc far
	push	AX
	push	DS
	mov	AX,seg DGROUP
	mov	DS,AX
	inc	vsync_Count1
	inc	vsync_Count2

	cmp	WORD PTR vsync_Proc+2,0
	je	short VSYNC_COUNT_END
	push	BX
	push	CX
	push	DX
	push	SI	; for pascal
	push	DI	; for pascal
	push	ES
	CLD
	call	DWORD PTR vsync_Proc
	pop	ES
	pop	DI	; for pascal
	pop	SI	; for pascal
	pop	DX
	pop	CX
	pop	BX
	CLI

VSYNC_COUNT_END:
	test	vsync_OldMask,VSYNC_DISABLE
	jnz	short SKIP_OLD

	; �ۑ����荞�݃x�N�^�̃A�h���X���Ăяo��
	pushf
	call	vsync_OldVect

	; �ۑ����荞�݃}�X�N�̍X�V
	in	AL,IMR
	mov	AH,AL
	or	AL,not VSYNC_DISABLE
	mov	vsync_OldMask,AL

	; �܂����荞�݋���
	mov	AL,not VSYNC_DISABLE
	and	AL,AH
	out	IMR,AL

	out	64h,AL		; �O�̂���
	pop	DS
	pop	AX
	iret

SKIP_OLD:
	mov	AL,20h		; EOI
	out	0,AL		; send EOI to master PIC
	out	64h,AL
	pop	DS
	pop	AX
	iret
chain_int	endp

func VSYNC_ENTER	; vsync_enter() {
	pushf
	CLI
	call	VSYNC_START
	mov	AX,VSYNC_VECT
	push	AX
	push	CS
	mov	AX,offset chain_int
	push	AX
	call	DOS_SETVECT
	popf
	ret
endfunc			; }

END
