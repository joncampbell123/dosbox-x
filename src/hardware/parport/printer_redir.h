
// include guard
#ifndef DOSBOX_PRREDIR_H
#define DOSBOX_PRREDIR_H

#include "dosbox.h"
#include "parport.h"
#include "printer_if.h"

class CPrinterRedir : public CParallel {
public:
	CPrinterRedir(Bitu nr, uint8_t initIrq, CommandLine* cmd);
	

	~CPrinterRedir();
	
	bool InstallationSuccessful;	// check after constructing. If
									// something was wrong, delete it right away.
	Bitu Read_PR();
	Bitu Read_COM();
	Bitu Read_SR();

	void Write_PR(Bitu);
	void Write_CON(Bitu);
	void Write_IOSEL(Bitu);
	bool Putchar(uint8_t);

	void handleUpperEvent(Bit16u type);
};

#endif	// include guard
