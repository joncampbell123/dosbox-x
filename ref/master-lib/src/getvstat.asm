; hiper library
;
; Description:
;	PC/AT の video mode と付随情報を得る
;
; Procedures/Functions:
;	void backup_video_state( *vmode ) ;
;
; Parameters:
;	IBM_VMODE_T	*vmode		情報格納用引数
;
;	typedef struct ibm_vmode_t VIDEO_STATE ;
;	struct VIDEO_STATE {
;		unsigned mode;		//現在のビデオ・モード
;		unsigned rows;		//現在の１画面の行数
;		unsigned cols;		//現在の１行あたりの桁数
;		unsigned total_rows;	//現在の画面全体の行数
;	};
;
;
; Returns:
;	ビデオモード (AT互換機でなければ 0 が返る)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	のろ/V
;	SIVA / DSA
;
; Revision History:
;	93/07/21 Initial
;	93/08/10 関数名から _ を取る
;	93/12/27 VESA対応? by 恋塚
;	93/12/27 Initial: setvideo.asm/master.lib 0.22
;	94/03/23 DSPX L 対応(SIVA/DSA)
;	94/ 3/23 Initial: getvstat.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc
	include vgc.inc
	extrn	FEP_EXIST:CALLMODEL

	.DATA
	EXTRN Machine_State : WORD

	.CODE
FEPTYPE_UNKNOWN equ 0
FEPTYPE_IAS	equ 1
FEPTYPE_MSKANJI	equ 2

func BACKUP_VIDEO_STATE	; backup_video_state() {
	push	BP
	mov	BP,SP
	push	DI
	vmode = (RETSIZE+1) * 2

	s_mov	AX,DS
	s_mov	ES,AX
	_les	DI,[BP+vmode]

	xor	AX,AX
	test	Machine_State,0010h	; PC/AT だったら
	jz	short FAILURE

IF 0	; 94/6/22 comment out
	test	Machine_State,1	; 英語モード?
	jz	short OLDTYPE	; 日本語なら無条件に普通のBIOSで取る
	mov	AX,4f03h	; VESA get video mode
	int	10h
	cmp	AX,4fh
	jne	short OLDTYPE
	mov	AX,BX
	cmp	AX,100h		; VESAで100h以上なら確定, 未満なら取り直し
	jnc	short SUCCESS
OLDTYPE:
ENDIF
	mov	AH,0fh
	int	10h

	and	AL,7Fh		;ビデオ・モード記憶
	mov	(VIDEO_STATE ptr ES:[DI]).mode, AL
	mov	AL,AH		;桁数記憶
	mov	(VIDEO_STATE ptr ES:[DI]).cols, AL

	push	ES
	mov	AX,0040h
	mov	ES,AX
	mov	AL,ES:[0084h]
	pop	ES
	inc	AL		;行数記憶
	mov	(VIDEO_STATE ptr ES:[DI]).rows, AL
	mov	(VIDEO_STATE ptr ES:[DI]).total_rows, AL
	call	FEP_EXIST
	cmp	AX,FEPTYPE_IAS
	jne	short SKIP_SHIFTLINE
	mov	AX,1D02h
	xor	BX,BX
	int	10h
	add	(VIDEO_STATE ptr ES:[DI]).total_rows,BL ;画面全体の行数記憶
SKIP_SHIFTLINE:

SUCCESS:
FAILURE:
	pop	DI
	pop	BP
	ret	(DATASIZE)*2
endfunc			; }

END
