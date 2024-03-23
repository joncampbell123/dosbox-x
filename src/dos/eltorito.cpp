
#include "dosbox.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <algorithm>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "menudef.h"
#include "programs.h"
#include "support.h"
#include "drives.h"
#include "cross.h"
#include "regs.h"
#include "ide.h"
#include "cpu.h"
#include "callback.h"
#include "cdrom.h"
#include "builtin.h"
#include "bios_disk.h"
#include "dos_system.h"
#include "dos_inc.h"
#include "bios.h"
#include "inout.h"
#include "dma.h"
#include "bios_disk.h"
#include "qcow2_disk.h"
#include "shell.h"
#include "setup.h"
#include "control.h"
#include <time.h>
#include "menu.h"
#include "render.h"
#include "mouse.h"
#include "eltorito.h"

bool ElTorito_ChecksumRecord(unsigned char *entry/*32 bytes*/) {
    unsigned int chk=0,i;

    for (i=0;i < 16;i++) {
        unsigned int word = ((unsigned int)entry[0]) + ((unsigned int)entry[1] << 8);
        chk += word;
        entry += 2;
    }
    chk &= 0xFFFF;
    return (chk == 0);
}

bool ElTorito_ScanForBootRecord(CDROM_Interface *drv,unsigned long &boot_record,unsigned long &el_torito_base) {
    unsigned char buffer[2048];
    unsigned int sec;

    for (sec=16;sec < 32;sec++) {
        if (!drv->ReadSectorsHost(buffer,false,sec,1))
            break;

        /* stop at terminating volume record */
        if (buffer[0] == 0xFF) break;

        /* match boot record and whether it conforms to El Torito */
        if (buffer[0] == 0x00 && memcmp(buffer+1,"CD001",5) == 0 && buffer[6] == 0x01 &&
            memcmp(buffer+7,"EL TORITO SPECIFICATION\0\0\0\0\0\0\0\0\0",32) == 0) {
            boot_record = sec;
            el_torito_base = (unsigned long)buffer[71] +
                    ((unsigned long)buffer[72] << 8UL) +
                    ((unsigned long)buffer[73] << 16UL) +
                    ((unsigned long)buffer[74] << 24UL);

            return true;
        }
    }

    return false;
}

