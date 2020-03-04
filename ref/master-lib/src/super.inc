;
;$Id: super.inc 0.03 93/01/15 11:43:23 Kazumi Rel $
; master.lib..
;
MEMID_Unknown	EQU	0
MEMID_font	EQU	1
MEMID_gaiji	EQU	2
MEMID_wfont	EQU	3
MEMID_super	EQU	4
MEMID_vvram	EQU	5
MEMID_bfile	EQU	6
MEMID_pfile	EQU	7
MEMID_bgm	EQU	8
MEMID_efs	EQU	9
MEMID_pi	EQU	10
MEMID_mag	EQU	11
MEMID_textback	EQU	12
MEMID_vtx	EQU	13

NoError		EQU	0
FileNotFound	EQU	-2
InsufficientMemory	EQU	-8
InvalidData	EQU	-13
GeneralFailure	EQU	-31
InvalidFormat	EQU	-11
PATTERN_ERASE	EQU	0
PATTERN_BLUE	EQU	1
PATTERN_RED	EQU	2
PATTERN_GREEN	EQU	3
PATTERN_INTEN	EQU	4
PLANE_ERASE	EQU	000c0h
PLANE_BLUE	EQU	0ffceh
PLANE_RED	EQU	0ffcdh
PLANE_GREEN	EQU	0ffcbh
PLANE_INTEN	EQU	0ffc7h

bfnt_header	STRUC
id		DB	5 dup (?)
col		DB	?
ver		DB	?
x00		DB	?
Xdots		DW	?
Ydots		DW	?
START		DW	?
END_		DW	?
font_name	DB	8 dup (?)
time		DD	?
extSize		DW	?
hdrSize		DW	?
bfnt_header	ENDS
BFNT_HEADER_SIZE EQU 32

MAXCHAR		EQU	512
MAXPAT		EQU	512

