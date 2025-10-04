volatile unsigned char far* pc98_dos_charmode(void)
{
	return (unsigned char far *)((char __seg *)(0x0060) + 0x8A);
}

void putc(const char c)
{
	__asm {
		mov dl, c
		mov ah, 0x02
		int 0x21
	}
}

void puts(const char *str)
{
	while(*str)  {
		putc(*str);
		str++;
	}
}

int main(void)
{
	static const char *CHARMODE_NAME[2] = {
		"\x1B[17mgraph mode\x1B[0m", "\x1B[20mkanji mode\x1B[0m"
	};
	static const char *OTHER_ESC[2] = {"\x1B)0", "\x1B)3"};

	int i = 0;
	unsigned char charmode = *pc98_dos_charmode();

	puts("Switched from ");
	puts(CHARMODE_NAME[charmode]);

	puts(OTHER_ESC[charmode]);

	puts(" to ");
	puts(CHARMODE_NAME[*pc98_dos_charmode()]);
	puts(".\r\n");

	puts("0x80 - 0xA0: ");
	for(i = 0x80; i <= 0xA0; i++) {
		putc(i);
	}
	puts("\r\n0xE0 - 0xFF: ");
	for(i = 0xE0; i <= 0xFF; i++) {
		putc(i);
	}

	puts("\r\nRe-run to switch back.\r\n");
	return 0;
}
