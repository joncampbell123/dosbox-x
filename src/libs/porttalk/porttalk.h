#ifndef PORTTALK_H
#define PORTTALK_H

#include "config.h"

bool initPorttalk(void);
bool setPermissionList();
void addIOPermission(uint16_t port);

void outportb(uint32_t portid, uint8_t value);
uint8_t inportb(uint32_t portid);

void outportd(uint32_t portid, uint32_t value);
uint32_t inportd(uint32_t portid);

void outportw(uint32_t portid, uint16_t value);
uint16_t inportw(uint32_t portid);

#endif
