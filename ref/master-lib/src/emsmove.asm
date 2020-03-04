; master library - PC98 - MSDOS - EMS
;
; Description:
;	EMSメモリと主メモリなどの間でデータを転送する
;
; Function/Procedures:
;	int ems_movememoryregion( struct const EMS_move_source_dest * block ) ;
;
; Parameters:
;	struct .. * block パラメータブロック
;
; Returns:
;	0 ........... success
;	80h～ ....... failure(EMS エラーコード)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;	EMS: LIM EMS 4.0
;
; Notes:
;	・NEC の EMSドライバは、このファンクションによって segment B000hの
;	　メモリを VRAMでなく設定してしまいます。
;	　実行後、ems_enablepageframe()によって VRAMに戻して下さい。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/16 Initial


	.MODEL SMALL
	include func.inc
	.CODE

func	EMS_MOVEMEMORYREGION
	push	SI
	mov	SI,SP

	_push	DS
	_lds	SI,SS:[SI+(RETSIZE+1)*2]
	mov	AX,5700h
	int	67h
	mov	AL,AH
	xor	AH,AH

	_pop	DS
	pop	SI
	ret	DATASIZE*2
endfunc

END
