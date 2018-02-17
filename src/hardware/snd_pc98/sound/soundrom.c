#include	"compiler.h"
#include	"dosio.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"soundrom.h"


	SOUNDROM	soundrom;


static const OEMCHAR file_sound[] = OEMTEXT("sound");
static const OEMCHAR file_extrom[] = OEMTEXT(".rom");
static const UINT8 defsoundrom[9] = {
							0x01,0x00,0x00,0x00,0xd2,0x00,0x08,0x00,0xcb};


static BRESULT loadsoundrom(UINT address, const OEMCHAR *name) {

	OEMCHAR	romname[24];
	OEMCHAR	path[MAX_PATH];
	FILEH	fh;
	UINT	rsize;

	file_cpyname(romname, file_sound, NELEMENTS(romname));
	if (name) {
		file_catname(romname, name, NELEMENTS(romname));
	}
	file_catname(romname, file_extrom, NELEMENTS(romname));
	getbiospath(path, romname, NELEMENTS(path));
	fh = file_open_rb(path);
	if (fh == FILEH_INVALID) {
		goto lsr_err;
	}
	rsize = file_read(fh, mem + address, 0x4000);
	file_close(fh);
	if (rsize != 0x4000) {
		goto lsr_err;
	}
	file_cpyname(soundrom.name, romname, NELEMENTS(soundrom.name));
	soundrom.address = address;
	if (address == 0xd0000) {
		CPU_RAM_D000 &= ~(0x0f << 0);
	}
	else if (address == 0xd4000) {
		CPU_RAM_D000 &= ~(0x0f << 4);
	}
	return(SUCCESS);

lsr_err:
	return(FAILURE);
}


// ----

void soundrom_reset(void) {

	ZeroMemory(&soundrom, sizeof(soundrom));
}

void soundrom_load(UINT32 address, const OEMCHAR *primary) {

	if (primary != NULL) {
		if (loadsoundrom(address, primary) == SUCCESS) {
			return;
		}
	}
	if (loadsoundrom(address, NULL) == SUCCESS) {
		return;
	}
	CopyMemory(mem + address + 0x2e00, defsoundrom, sizeof(defsoundrom));
	soundrom.name[0] = '\0';
	soundrom.address = address;
}

void soundrom_loadex(UINT sw, const OEMCHAR *primary) {

	if (sw < 4) {
		soundrom_load((0xc8000 + ((UINT32)sw << 14)), primary);
	}
	else {
		ZeroMemory(&soundrom, sizeof(soundrom));
	}
}

