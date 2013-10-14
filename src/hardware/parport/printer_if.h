
#ifndef PRINTER_IF
#define PRINTER_IF
Bitu PRINTER_readdata(Bitu port,Bitu iolen);
void PRINTER_writedata(Bitu port,Bitu val,Bitu iolen);
Bitu PRINTER_readstatus(Bitu port,Bitu iolen);
void PRINTER_writecontrol(Bitu port,Bitu val, Bitu iolen);
Bitu PRINTER_readcontrol(Bitu port,Bitu iolen);

bool PRINTER_isInited();
#endif
