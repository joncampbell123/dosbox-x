; master library - DOS - CREATE
;
; Description:
;	DOSのファイル作成
;
; Function/Procedures:
;	int dos_create( const char * filename, int attribute ) ;
;	function dos_create( filename:String; attribute:Integer ) : Integer;
;
; Parameters:
;	filename	作成するパス名
;	attribute	ファイル属性
;
; Returns:
;	PathNotFound	 (cy=1)	失敗,パスが無効
;	TooManyOpenFiles (cy=1)	失敗,オープンファイルが多すぎる
;	AccessDenied 	 (cy=1)	失敗,属性が無効
;	else        	 (cy=0)	成功,ファイルハンドル
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS 2.1 or later
;
; Requiring Resources:
;	CPU: 8086
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
;	93/ 8/ 3 Initial: doscreat.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE
	EXTRN DOS_AXDX : CALLMODEL

func DOS_CREATE	; dos_create() {
	mov	BX,SP
	; 引数
	filename  = (RETSIZE+1)*2
	attribute = (RETSIZE+0)*2
	mov	AX,3c00h
	push	AX
	_push	SS:[BX+filename+2]
	push	SS:[BX+filename]
	mov	CX,SS:[BX+attribute]
	_call	DOS_AXDX
	ret	(DATASIZE+1)*2
endfunc		; }

END
