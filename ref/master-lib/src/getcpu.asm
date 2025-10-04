; master library - CPU
;
; Description:
;	CPUÇÃîªíË
;
; Functions/Procedures:
;	unsigned get_cpu(void) ;
;
; Parameters:
;	none
;
; Returns:
;	bit 0..7	0=8086
;			1=80186/V30/V50
;			2=80286
;			3=80386
;			4=i486/Cx486SLC/DLC/DRx2
;			5=Pentium
;			6... ?
;
;	bit 8:		0=Real Mode
;			1=V86 Mode
;	bit 15-14:	00=Intel
;			10=NEC
;			01=Cyrix
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	
;
; Requiring Resources:
;	CPU: 8086, 80186, V30, V50, 80286, 80386, 80486, Cx486, Pentium
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
;	óˆíÀè∫ïF
;
; Revision History:
;	94/ 5/27 Initial: getcpu.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.CODE
V86BIT   equ 0100h
NECBIT   equ 8000h
CYRIXBIT equ 4000h

.xlist
OPR32	macro	; operand size prefix
	db 66h
endm
_pushfd	macro
	OPR32
	pushf
	endm
_popfd	macro
	OPR32
	popf
	endm

_push32	macro r
	OPR32
	push r
	endm
_pop32	macro r
	OPR32
	pop r
	endm

_cpuid	macro
	db	0fh, 0a2h
	endm

.list

func GET_CPU	; get_cpu() {
	mov	BX,0		; BXÇ…åãâ Çäiî[Ç∑ÇÈ
	; Ç®Ç®Ç¥Ç¡ÇœÇ»îªíË( (8086,80186,V), (286), (386,486)ÇÃ3ï™äÑ )
	pushf
	pushf
	pop	AX	; AX=flag
	or	AX,4000h
	push	AX
	popf		; flag|=4000h
	pushf
	pop	AX	; AX=flag
	popf
	test	AH,40h
	jz	short IS286
	test	AH,80h
	jz	short IS386486
IS8086V30:
	mov	AL,1
	mov	CL,20h
	shr	AL,CL
	jnz	short IS186		; intel186
	; 8086, V30/50?
	mov	AX,100h
	DB 0d5h,0	; AAD 0
	jz	short OVER		; 8086
	or	BX,NECBIT		; V30/V50
IS186:	mov	BL,1	; i80186
	jmp	short OVER
IS286:	mov	BL,2	; 286
OVER:	mov	AX,BX
	ret
	EVEN

IS386486:
	mov	BL,3	; 386 or 486...

	.286P
	smsw	ax
	test	AL,1
	jz	short IS386REAL
	or	BX,V86BIT		; V86mode
IS386REAL:
	xor	DX,DX
	pushf
	mov	AX,-1
	mov	CX,4
	div	CX
	pushf
	pop	CX
	pop	AX
	xor	AX,CX
	and	AX,08d5h	; CF PF AF ZF SF OF
	jnz	short ISI386
	or	BX,CYRIXBIT

	; i386/i486/Pentium...?
ISI386:	
	mov	CX,SP
	and	SP,0fffch
	_pushfd
	cli
	_pushfd
	_pop32	AX
    OPR32
	or	AX,0
	dw	24h		; or eax,00240000h
	_push32 AX
	_popfd
	_pushfd
	pop	AX		; bit15-0 (drop)
	pop	AX		; bit31-16
	_popfd
	mov	SP,CX
	test	AX,4		; test eax,00040000h
	jz	short OVER	; 80386

    	mov	BL,4		; 486
	test	AX,20h		; test eax,00200000h
	jz	short OVER	; 80486

     	; Pentium or later....
     OPR32
	mov	AX,1
	dw	0		; mov eax,1
	push	BX		; guard BX
	_cpuid
	pop	BX
	mov	BL,AH
	and	BL,0fh		; family...
	jmp	short OVER
endfunc		; }

END
