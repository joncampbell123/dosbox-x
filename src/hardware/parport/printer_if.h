
#ifndef PRINTER_IF
#define PRINTER_IF
Bitu PRINTER_readdata(Bitu port,Bitu iolen);
void PRINTER_writedata(Bitu port,Bitu val,Bitu iolen);
Bitu PRINTER_readstatus(Bitu port,Bitu iolen);
void PRINTER_writecontrol(Bitu port,Bitu val, Bitu iolen);
Bitu PRINTER_readcontrol(Bitu port,Bitu iolen);

bool PRINTER_isInited();

// STROBE-pulse one byte into the printer engine via the DATA/CONTROL latches
// - STROBE off, byte latched, STROBE pulsed, STROBE off, status read to clear
// ACK. What every LPTn:/PRN-redirection Putchar() does to hand a byte to the
// virtual printer; shared so backends that also drive a real device (e.g.
// extlpt) don't each reimplement the same five register writes.
inline void PRINTER_StrobeByte(Bitu val) {
	PRINTER_writecontrol(0, 0xD4, 1); // strobe off
	PRINTER_writedata(0, val, 1);
	PRINTER_writecontrol(0, 0xD5, 1); // strobe pulse
	PRINTER_writecontrol(0, 0xD4, 1); // strobe off
	PRINTER_readstatus(0, 1);         // clear ack
}
#endif
