#ifndef PORTTALK_H
#define PORTTALK_H

#include "config.h"

bool initPorttalk(void);
bool setPermissionList();
void addIOPermission(Bit16u port);

void outportb(Bit32u portid, Bit8u value);
Bit8u inportb(Bit32u portid);

void outportd(Bit32u portid, Bit32u value);
Bit32u inportd(Bit32u portid);

void outportw(Bit32u portid, Bit16u value);
Bit16u inportw(Bit32u portid);

#endif
