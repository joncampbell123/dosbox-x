; Subject:
;	Master.lib Version String Definition
;
; Author:
;	óˆíÀè∫ïF
; 
; Revision History:
;	92/12/22 Initial( version 0.10 )
;	93/ 1/27 version 0.11
;	93/ 2/12 medium,compact
;	93/ 2/14 version 0.12
;	93/ 3/11 version 0.13
;	93/ 3/23 version 0.14
;	93/ 4/12 version 0.15
;	93/ 5/16 version 0.16
;	93/ 5/17 version 0.17
;	93/ 6/ 1 version 0.18
;	93/ 6/27 version 0.19
;	93/ 8/10 version 0.20
;	93/11/24 version 0.21
;	94/ 2/ 7 version 0.22
;	94/ 2/21 version 0.22a
;	95/  /   version 0.23

	.MODEL SMALL
	include func.inc

	.DATA

VERSIONSTR	equ '0.23'

VERS	macro lab,str
	public lab
lab	db 'MASTER', str,'.LIB Version '
	endm

IFDEF NEARMODEL
	VERS _Master_Version_NEAR,'S'
ENDIF
IFDEF FARMODEL
	VERS _Master_Version_FAR,'L'
ENDIF
IFDEF MEDIUMMODEL
	VERS _Master_Version_MEDIUM,'M'
ENDIF
IFDEF COMPACTMODEL
	VERS _Master_Version_COMPACT,'C'
ENDIF
	public _Master_Version
	public _Master_Copyright
_Master_Version db VERSIONSTR, ' '
_Master_Copyright label byte
	db 'Copyright (c)1995 '
	db 'A.Koizuka,'
	db 'Kazumi,'
	db 'steelman,'
	db 'iR,'
;	db ','		; â¸ë¢ÇµÇΩèÍçáÇÕÇ±Ç±Ç…ñºëOÇí«â¡Ç∑ÇÈÇ±Ç∆
	db 'All rights reserved.',0

END
