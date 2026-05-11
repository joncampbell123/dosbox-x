
bool ElTorito_ChecksumRecord(unsigned char *entry/*32 bytes*/);
bool ElTorito_ScanForBootRecord(CDROM_Interface *drv,unsigned long &boot_record,unsigned long &el_torito_base);
 
