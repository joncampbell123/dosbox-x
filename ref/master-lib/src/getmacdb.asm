; master library - 
;
; Description:
;	DOSBOX内ならばMachine_StateにDOSBOXビットを立てる
;
; Functions/Procedures:
;	unsigned get_machine_dosbox(void);
;
; Parameters:
;	none
;
; Returns:
;	Machine_Stateの値がそのまま返る
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	DOS, OS/2, Windows, Windows NT
;
; Requiring Resources:
;	CPU: 8086?
;
; Notes:
;	
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	95/ 3/ 8 Initial: getmacdb.asm/master.lib 0.22k
;	95/ 3/17 [M0.22k] OS/2 1.X, 3.Xに対応

DOSBOX	EQU	8000h

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	Machine_State:WORD

	.CODE

func GET_MACHINE_DOSBOX	; get_machine_dosbox() {
	; DOS互換BOX内かどうかの判定
	mov	AX,3306h
	int	21h
	cmp	AL,0ffh
	je	short DOS_OR_WINDOWS
	cmp	BX,3205h	;  5.50: WindowsNT DOS BOX
	je	short IN_DOSBOX
	cmp	BL,10		; 10.xx: OS/2 1.x
	je	short IN_DOSBOX
	cmp	BL,20		; 20.xx: OS/2 2.x
	je	short IN_DOSBOX
	cmp	BL,30		; 30.xx: OS/2 3.x
	je	short IN_DOSBOX
DOS_OR_WINDOWS:
	mov	AX,1600h
	int	2fh
	and	AL,7fh		; nonzero: Win3.x 386Enhanced (or Win386)
	jz	short RAW_DOS
IN_DOSBOX:
	or	Machine_State,DOSBOX
RAW_DOS:
	mov	AX,Machine_State
	ret
endfunc		; }

END
