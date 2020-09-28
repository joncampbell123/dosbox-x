#ifndef PORTTALK_H
#define PORTTALK_H

#include "config.h"

bool initPorttalk(void);
bool setPermissionList();
void addIOPermission(uint16_t port);

void outportb(Bit32u portid, uint8_t value);
uint8_t inportb(Bit32u portid);

void outportd(Bit32u portid, Bit32u value);
Bit32u inportd(Bit32u portid);

void outportw(Bit32u portid, uint16_t value);
uint16_t inportw(Bit32u portid);

#endif
