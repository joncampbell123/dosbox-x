/* ReC98 Research
 * --------------
 * Testing JIS trail word rendering in the PC-98 text RAM
 */

#pragma option -O -Z -G -k- -d -1-

#include <dos.h>

typedef unsigned char uint4_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

static const unsigned int RES_X = 640;
static const unsigned int RES_Y = 400;

uint16_t kuten_to_tram(uint8_t ku, uint8_t ten) {
	return ((ku - 0x20) | (ten << 8));
}

struct gaiji_t {
	uint16_t space_for_int18_1A;
	uint16_t dots[16];
};

const gaiji_t GAIJI[] = {
	{ 0, 0, 0, 0, 0x8001,0x8003,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0, 0, 0 },
	{ 0, 0, 0, 0, 0xC003,0x6006,0x6006,0x6006,0x6000,0xC000,0x8001,0x0003,0x0006,0xE007,0, 0, 0 },
	{ 0, 0, 0, 0, 0xC003,0x6006,0x6006,0x6000,0xC001,0xC001,0x6000,0x6006,0x6006,0xC003,0, 0, 0 },
	{ 0, 0, 0, 0, 0xC000,0xC001,0xC001,0xC002,0xC002,0xC006,0xC006,0xE007,0xC000,0xC000,0, 0, 0 },
	{ 0, 0, 0, 0, 0xC00F,0x000C,0x000C,0x000C,0x800F,0xC000,0xC000,0xC00C,0xC00C,0x8007,0, 0, 0 },
	{ 0, 0, 0, 0, 0xC003,0x6006,0x6006,0x0006,0xC007,0x6006,0x6006,0x6006,0x6006,0xC003,0, 0, 0 },
	{ 0, 0, 0, 0, 0xE007,0x6000,0x6000,0x6000,0xC000,0xC000,0x8001,0x8001,0x8001,0x8001,0, 0, 0 },
	{ 0, 0, 0, 0, 0xC003,0x6006,0x6006,0x6006,0xC003,0x6006,0x6006,0x6006,0x6006,0xC003,0, 0, 0 },
	{ 0, 0, 0, 0, 0xC003,0x6006,0x6006,0x6006,0x6006,0xE003,0x6000,0x6006,0x6006,0xC003,0, 0, 0 },
	{ 0, 0, 0, 0, 0xF018,0x9839,0x9819,0xB819,0xF819,0xD819,0x9819,0x9819,0x9819,0xF018,0, 0, 0 },
	{ 0, 0, 0, 0, 0x6018,0xE038,0x6018,0x6018,0x6018,0x6018,0x6018,0x6018,0x6018,0x6018,0, 0, 0 },
	{ 0, 0, 0, 0, 0xF018,0x9839,0x9819,0x9819,0x1818,0x3018,0x6018,0xC018,0x8019,0xF819,0, 0, 0 },
	{ 0, 0, 0, 0, 0xF018,0x9839,0x9819,0x1818,0x7018,0x7018,0x1818,0x9819,0x9819,0xF018,0, 0, 0 },
	{ 0, 0, 0, 0, 0x3018,0x7038,0x7018,0xB018,0xB018,0xB019,0xB019,0xF819,0x3018,0x3018,0, 0, 0 },
	{ 0, 0, 0, 0, 0xF819,0x8039,0x8019,0x8019,0xF019,0x1818,0x1818,0x9819,0x9819,0xF018,0, 0, 0 },
	{ 0, 0, 0, 0, 0xF018,0x9839,0x9819,0x8019,0xF019,0x9819,0x9819,0x9819,0x9819,0xF018,0, 0, 0 },
	{ 0, 0, 0x80FF,0xC0FF,0xE0C0,0x61C0,0x61C0,0x63C0,0xE3C0,0xC3FF,0x87FF,0x07C0,0x07C0,0x0FC0,0x0EC0,0x0EC0,0 },
	{ 0, 0, 0x6060,0x60F0,0x60F0,0x60F8,0x6098,0x609C,0x609C,0x600C,0x600E,0x600E,0x60FE,0x30FF,0x3F07,0x1F07,0 },
	{ 0, 0, 0x7F18,0xFF18,0xC119,0x8019,0x8019,0xC019,0xFF18,0x7F18,0x0118,0x0018,0x8019,0xC131,0xFFF0,0x7FE0,0 },
	{ 0, 0, 0xFE0F,0xFE8F,0x00CC,0x00CC,0x000C,0x000C,0xFE0F,0xFE8F,0x00CC,0x00CC,0x00CC,0x00CC,0xFE8F,0xFE0F,0 },
	{ 0, 0x801F,0xC03F,0xE070,0x70E0,0x31C0,0x03C0,0xF3C3,0xF0C3,0x31C0,0x33C0,0x73E0,0xE370,0xC13F,0x801F,0, 0 },
};

void __fastcall ansi_puts(const char *str)
{
	while(*str) {
		_AL = *(str++);
		geninterrupt(0x29);
	}
}

class JISCursor {
	uint16_t far *offset;
	uint8_t attribute_cycle;

public:
	JISCursor() :
		offset(reinterpret_cast<uint16_t __seg *>(0xA000)),
		attribute_cycle(0) {
		ansi_puts("\x1B*");
	}

	void __fastcall put(uint16_t word);

	void putjis_same(const uint8_t& ku, const uint8_t& ten) {
		uint16_t word = kuten_to_tram(ku, ten);
		put(word);
		put(word);
	}

	void putjis_adjacent(const uint8_t& ku, const uint8_t& ten) {
		put(kuten_to_tram(ku, ten));
		put(kuten_to_tram((ku - 1), (ten + 1)));
	}

	void putjis_inverted(const uint8_t& ku, const uint8_t& ten) {
		uint16_t word = kuten_to_tram(ku, ten);
		put((word + 0x80));
		put(word);
	}

	void putjis_proper(const uint8_t& ku, const uint8_t& ten) {
		uint16_t word = kuten_to_tram(ku, ten);
		put(word);
		put((word + 0x80));
	}

	void putjis_empty(const uint8_t& ku, const uint8_t& ten) {
		put(kuten_to_tram(ku, ten));
		put(0x00);
	}

	void put_ascii(const char *str);

	void testcase_single(
		const char* ku_name,
		const uint8_t* test_name,
		const uint8_t ku,
		void (JISCursor::*putfunc)(const uint8_t& ku, const uint8_t& ten)
	);
	void __fastcall testcase(const char* name, const uint8_t ku);
};

void __fastcall JISCursor::put(uint16_t word)
{
	offset[0x0000 / sizeof(uint16_t)] = word;
	offset[0x2000 / sizeof(uint16_t)] = (0x41 + (attribute_cycle * 0x20));
	attribute_cycle++;
	if(attribute_cycle >= 6) {
		attribute_cycle = 0;
	};
	offset++;
}

void JISCursor::put_ascii(const char *str)
{
	while(*str) {
		char c = *(str++);
		switch(c) {
		case '\n':
			offset += ((160 - (reinterpret_cast<uint16_t>(offset) % 160)) / 2);
			break;
		default:
			put(c);
		}
	}
}

void JISCursor::testcase_single(
	const char* ku_name,
	const uint8_t* test_name,
	const uint8_t ku,
	void (JISCursor::*putfunc)(const uint8_t& ku, const uint8_t& ten)
)
{
	enum {
		START = 0x21,
		END = 0x35,
	};
	ansi_puts("\n");
	put_ascii(ku_name);
	put_ascii(test_name);
	put(kuten_to_tram(0x76, 0x2B));
	for(unsigned i = START; i < END; i++) {
		(this->*putfunc)(ku, i);
	}
	put_ascii("\n");
}

void __fastcall JISCursor::testcase(const char* name, const uint8_t ku)
{
	testcase_single(name, " +     same: ", ku, &JISCursor::putjis_same);
	testcase_single(name, " +    empty: ", ku, &JISCursor::putjis_empty);
	testcase_single(name, " +   proper: ", ku, &JISCursor::putjis_proper);
	testcase_single(name, " + adjacent: ", ku, &JISCursor::putjis_adjacent);
	testcase_single(name, " + inverted: ", ku, &JISCursor::putjis_inverted);
}

void __fastcall setcolor(uint4_t col, uint4_t r, uint4_t g, uint4_t b)
{
	outportb(0xA8, col);
	outportb(0xAA, g);
	outportb(0xAC, r);
	outportb(0xAE, b);
}

int main(void)
{
	// Enter 400-line mode
	_AH = 0x42;
	_CH = 0xC0;
	geninterrupt(0x18);

	// Enter 16-color mode
	outportb(0x6A, 1);

	// Show VRAM
	_AH = 0x40;
	geninterrupt(0x18);

	outportb(0xA6, 0);	// Access page 0
	outportb(0xA4, 0);	// Show page 0

	setcolor(0x0, 0x0, 0x0, 0x0);	// Color 0 (0000b): Black
	setcolor(0x1, 0x8, 0x0, 0x0);	// Color 1 (0001b): Bright red
	setcolor(0x2, 0x4, 0x0, 0x0);	// Color 2 (0010b): Dark red

	_DI = 0x0;
	for(unsigned int y = 0; y < RES_Y; y++) {
		for(unsigned int x = 0; x < ((RES_X / 8) / sizeof(uint16_t)); x++) {
			if((y & 0xF) == 0) {
				*reinterpret_cast<uint16_t far *>(MK_FP(0xA800, _DI)) = 0xFFFF;
				*reinterpret_cast<uint16_t far *>(MK_FP(0xB000, _DI)) = 0x0000;
			} else {
				*reinterpret_cast<uint16_t far *>(MK_FP(0xA800, _DI)) = 0x0080;
				*reinterpret_cast<uint16_t far *>(MK_FP(0xB000, _DI)) = 0x8000;
			}
			*reinterpret_cast<uint16_t far *>(MK_FP(0xB800, _DI)) = 0x0000;
			*reinterpret_cast<uint16_t far *>(MK_FP(0xE000, _DI)) = 0x0000;
			_DI += sizeof(uint16_t);
		}
	}

	const gaiji_t* gaiji = GAIJI;
	for(unsigned int i = 0; i < (sizeof(GAIJI) / sizeof(GAIJI[0])); i++) {
		_BX = FP_SEG(gaiji);
		_CX = FP_OFF(gaiji);
		_DX = (0x7621 + i);
		_AH = 0x1A;
		geninterrupt(0x18);
		gaiji++;
	}

	JISCursor cursor;
	cursor.testcase(" Circled digits", 0x2D);
	cursor.testcase("First kanji row", 0x30);
	cursor.testcase("  Fullwidth box", 0x2C);
	cursor.testcase("          Gaiji", 0x76);

	ansi_puts("\n");
	cursor.put_ascii("Gaiji interrupted by kanji: ");
	cursor.put(kuten_to_tram(0x76, 0x2B));
	cursor.putjis_proper(0x4D, 0x6B);
	cursor.putjis_proper(0x3D, 0x43);
	cursor.putjis_proper(0x21, 0x21);
	cursor.putjis_proper(0x21, 0x41);
	cursor.put_ascii("Midnight Rainstorm\n");

	ansi_puts("\n");
	cursor.put_ascii("   Same for circled digits: ");
	cursor.put(kuten_to_tram(0x2D, 0x2B));
	cursor.putjis_proper(0x4D, 0x6B);
	cursor.putjis_proper(0x3D, 0x43);
	cursor.putjis_proper(0x21, 0x21);
	cursor.putjis_proper(0x21, 0x41);
	cursor.put_ascii("Midnight Rainstorm\n");

	return 0;
}
