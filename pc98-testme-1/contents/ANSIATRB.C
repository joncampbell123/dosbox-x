#define HEADER_LINE "Testing text attribute changes via escape sequences..."

#define pc98_text_cursor_x() *(char far*)(0x071C)
#define pc98_text_cursor_y() *(char far*)(0x0710)

unsigned char far* pc98_tram_at(unsigned char x, unsigned char y)
{
	return (unsigned char far *)((int __seg *)(0xA000) + ((y * 80) + x));
}

unsigned char far* pc98_aram_at(unsigned char x, unsigned char y)
{
	return (unsigned char far *)((int __seg *)(0xA200) + ((y * 80) + x));
}

void putc(const char c)
{
	__asm {
		mov dl, c
		mov ah, 02h
		int 21h
	}
}

void put_3_digit_number(unsigned int n)
{
	unsigned int units = n % 10;
	unsigned int tens = (n / 10) % 10;
	unsigned int hundreds = n / 100;
	if(hundreds >= 1) {
		putc('0' + hundreds);
		putc('0' + tens);
	} else if(tens >= 1) {
		putc('0' + tens);
	}
	putc('0' + units);
}

void puts(const char *str)
{
	while(*str) {
		putc(*str);
		str++;
	}
}

void puts_escapeseq(const char *seq)
{
	puts("\x1B[");
	puts(seq);
	putc('m');
}

#define NUM_COLORS 8
#define elementsof(x) (sizeof(x) / sizeof(x[0]))

typedef struct {
	unsigned int taken;
	unsigned int passed;
} test_state_t;

typedef struct {
	const char name[17];
	const char seqs[NUM_COLORS][5];
	const unsigned char attrib_expected[NUM_COLORS];
} color_test_t;

void attrib_test(
	test_state_t *state,
	const char *seq,
	unsigned char dispchar,
	unsigned char exp
)
{
	int passed;
	unsigned char cx = pc98_text_cursor_x();
	unsigned char cy = pc98_text_cursor_y();
	unsigned char far *aram = pc98_aram_at(cx, cy);

	state->taken++;

	puts("\x1B[");
	puts(seq);
	putc('m');
	putc(' ');
	putc(dispchar);
	putc(' ');

	passed = (aram[0] == exp) && (aram[2] == exp) && (aram[4] == exp);
	state->passed += passed;
	if(!passed) {
		*(pc98_tram_at(cx + 1, cy)) = 'F';
	}
}

void color_test(
	test_state_t *state,
	const color_test_t *test,
	char prefix_attrib,
	unsigned char prefix_attrib_flag
)
{
	static const char testchars[] = {
		'_', 'R', 'G', 'Y', 'B', 'P', 'C', 'W'
	};
	int i;

	if(test->name[0]) {
		puts("\r\n* ");
		puts(test->name);
		putc(' ');
	}

	for(i = 0; i < elementsof(testchars); i++) {
		char final_seq[7] = { '\0' };
		char *p = final_seq;
		const char *q = test->seqs[i];
		if(prefix_attrib) {
			*p++ = prefix_attrib;
			*p++ = ';';
		}
		while(*q) {
			*p++ = *q++;
		}
		attrib_test(
			state,
			final_seq,
			testchars[i],
			test->attrib_expected[i] | prefix_attrib_flag
		);
		puts_escapeseq("0");
	}
}

void hr(int len)
{
	int i;
	for(i = 0; i < len; i++) {
		puts("\x86\x44");
	}
	puts("\r\n");
}

int main(void)
{
	static const color_test_t tests[] = {
		{
			"Foreground set 1",
			{ "16", "17", "20", "21", "18", "19", "22", "23" },
			{ 0xE0, 0x41, 0x81, 0xC1, 0x21, 0x61, 0xA1, 0xE1 }
		}, {
			"",
			{ "30", "31", "32", "33", "34", "35", "36", "37" },
			{ 0x01, 0x41, 0x81, 0xC1, 0x21, 0x61, 0xA1, 0xE1 }
		}, {
			"Reversed via FG ",
			{ "8;7", "17;7", "20;7", "21;7", "18;7", "19;7", "22;7", "23;7" },
			{ 0xE4, 0x45, 0x85, 0xC5, 0x25, 0x65, 0xA5, 0xE5 }
		}, {
			"",
			{ "30;7", "31;7", "32;7", "33;7", "34;7", "35;7", "36;7", "37;7" },
			{ 0x05, 0x45, 0x85, 0xC5, 0x25, 0x65, 0xA5, 0xE5 }
		}, {
			"Reversed, direct",
			{ "40", "41", "42", "43", "44", "45", "46", "47" },
			{ 0x05, 0x45, 0x85, 0xC5, 0x25, 0x65, 0xA5, 0xE5 }
		}
	};

	int i;
	test_state_t state = {0};

	int HEADER_LINE_LEN = sizeof(HEADER_LINE) - 1;
	puts(HEADER_LINE);
	puts("\r\n");
	hr(HEADER_LINE_LEN);
	puts("Regular:");
	for(i = 0; i < elementsof(tests); i++) {
		color_test(&state, &tests[i], 0, 0);
	}
	puts("\r\nBlinking:");
	for(i = 0; i < elementsof(tests); i++) {
		color_test(&state, &tests[i], '5', 0x02);
	}
	puts("\r\nUnderline:");
	for(i = 0; i < elementsof(tests); i++) {
		color_test(&state, &tests[i], '4', 0x08);
	}
	puts("\r\nBit 4:");
	for(i = 0; i < elementsof(tests); i++) {
		color_test(&state, &tests[i], '2', 0x10);
	}

	puts("\r\n");
	hr(HEADER_LINE_LEN);

	// Secret attribute
	attrib_test(&state, "8", '.', 0xE0);
	// Reset for invalid codes
	attrib_test(&state, "99", '.', 0xE1);

	if(state.passed == state.taken) {
		puts_escapeseq("42");
	} else if(state.passed < state.taken) {
		puts_escapeseq("41");
	}
	put_3_digit_number(state.passed);
	puts(" of ");
	put_3_digit_number(state.taken);
	puts(" tests passed.\r\n");
	puts_escapeseq("0");
	return state.passed != state.taken;
}
