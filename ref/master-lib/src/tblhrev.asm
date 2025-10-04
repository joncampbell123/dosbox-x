; 8bit水平逆転データテーブル
;
; (Public Domain扱い)
;
; Revision History:
;	93/2/3	Initial
;	93/ 5/29 [M0.18] .CONST->.DATA
;

.MODEL SMALL
.DATA

	public _table_hreverse
	public table_hreverse
_table_hreverse label byte
table_hreverse label byte
	db 000h,080h,040h,0c0h, 020h,0a0h,060h,0e0h
	db 010h,090h,050h,0d0h, 030h,0b0h,070h,0f0h
	db 008h,088h,048h,0c8h, 028h,0a8h,068h,0e8h
	db 018h,098h,058h,0d8h, 038h,0b8h,078h,0f8h
	db 004h,084h,044h,0c4h, 024h,0a4h,064h,0e4h
	db 014h,094h,054h,0d4h, 034h,0b4h,074h,0f4h
	db 00ch,08ch,04ch,0cch, 02ch,0ach,06ch,0ech
	db 01ch,09ch,05ch,0dch, 03ch,0bch,07ch,0fch

	db 002h,082h,042h,0c2h, 022h,0a2h,062h,0e2h
	db 012h,092h,052h,0d2h, 032h,0b2h,072h,0f2h
	db 00ah,08ah,04ah,0cah, 02ah,0aah,06ah,0eah
	db 01ah,09ah,05ah,0dah, 03ah,0bah,07ah,0fah
	db 006h,086h,046h,0c6h, 026h,0a6h,066h,0e6h
	db 016h,096h,056h,0d6h, 036h,0b6h,076h,0f6h
	db 00eh,08eh,04eh,0ceh, 02eh,0aeh,06eh,0eeh
	db 01eh,09eh,05eh,0deh, 03eh,0beh,07eh,0feh

	db 001h,081h,041h,0c1h, 021h,0a1h,061h,0e1h
	db 011h,091h,051h,0d1h, 031h,0b1h,071h,0f1h
	db 009h,089h,049h,0c9h, 029h,0a9h,069h,0e9h
	db 019h,099h,059h,0d9h, 039h,0b9h,079h,0f9h
	db 005h,085h,045h,0c5h, 025h,0a5h,065h,0e5h
	db 015h,095h,055h,0d5h, 035h,0b5h,075h,0f5h
	db 00dh,08dh,04dh,0cdh, 02dh,0adh,06dh,0edh
	db 01dh,09dh,05dh,0ddh, 03dh,0bdh,07dh,0fdh

	db 003h,083h,043h,0c3h, 023h,0a3h,063h,0e3h
	db 013h,093h,053h,0d3h, 033h,0b3h,073h,0f3h
	db 00bh,08bh,04bh,0cbh, 02bh,0abh,06bh,0ebh
	db 01bh,09bh,05bh,0dbh, 03bh,0bbh,07bh,0fbh
	db 007h,087h,047h,0c7h, 027h,0a7h,067h,0e7h
	db 017h,097h,057h,0d7h, 037h,0b7h,077h,0f7h
	db 00fh,08fh,04fh,0cfh, 02fh,0afh,06fh,0efh
	db 01fh,09fh,05fh,0dfh, 03fh,0bfh,07fh,0ffh

END
