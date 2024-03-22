
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
	Bitu Read_PR() override;
	Bitu Read_COM() override;
	Bitu Read_SR() override;

	void Write_PR(Bitu) override;
	void Write_CON(Bitu) override;
	void Write_IOSEL(Bitu) override;
	bool Putchar(uint8_t) override;

	void handleUpperEvent(uint16_t type) override;
};

#endif	// include guard
