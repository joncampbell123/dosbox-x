#include <stdio.h>
#include <conio.h>

typedef unsigned char uchar;

void pascal near __scroll(uchar __dir, uchar __x1, uchar __y1, uchar __x2, uchar __y2, uchar __lines);
unsigned int _NEC_WHEREXY(void);

typedef struct
{
	uchar windowx1;
	uchar windowy1;
	uchar windowx2;
	uchar windowy2;
	uchar attribute;
	uchar normattr;
	uchar currmode;
	uchar screenheight;
	uchar screenwidth;
	uchar graphicsmode;
	uchar columns;
	union {
			char far * p;
			struct { unsigned off,seg; } u;
	} displayptr;
	uchar unknown_0;
	char far * unknown_1;
	unsigned int unknown_2;
	unsigned int unknown_3;
} VIDEOREC;

extern VIDEOREC _video;

void print_wherexy(void)
{
	register int xy = _NEC_WHEREXY();
	printf("Where: %u, %u\n", (xy & 0xFF00) << 8, (xy & 0xFF));
}

#define V_SET_MODE              0x00
#define V_SET_CURSOR_POS        0x02
#define V_GET_CURSOR_POS        0x03
#define V_SCROLL_UP             0x06
#define V_SCROLL_DOWN           0x07
#define V_RD_CHAR_ATTR          0x08
#define V_WR_CHAR_ATTR          0x09
#define V_WR_CHAR               0x0a
#define V_WR_TTY                0x0e
#define V_GET_MODE              0x0f

void main (void)
{
	// print_wherexy();
	textcolor(GREEN);
	cputs("Green on black. ");
	cputs("Call #2.\r\n");

	/*printf(
		"Video structure:\n"
		"* windowx1: %u\n"
		"* windowy1: %u\n"
		"* windowx2: %u\n"
		"* windowy2: %u\n"
		"* attribute: %u\n"
		"* normattr: %u\n"
		"* currmode: %u\n"
		"* screenheight: %u\n"
		"* screenwidth: %u\n"
		"* graphicsmode: %u\n"
		"* columns: %u\n"
		"* displayptr: %Fp\n"
		"* unknown_0: %u\n"
		"* unknown_1: %Fp\n",
		_video.windowx1,
		_video.windowy1,
		_video.windowx2,
		_video.windowy2,
		_video.attribute,
		_video.normattr,
		_video.currmode,
		_video.screenheight,
		_video.screenwidth,
		_video.graphicsmode,
		_video.columns,
		_video.displayptr.p,
		_video.unknown_0,
		_video.unknown_1
	);*/

	// print_wherexy();
	textattr(GREEN | REVERSE);
	cputs("Black on green.\r\n");
	// __scroll(V_SCROLL_UP, 0, 0, 80, 23, 1);
}
