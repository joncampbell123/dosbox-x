#ifndef H_CALLBACK_H
#define H_CALLBACK_H
#include "dosbox.h"
typedef Bitu (*CallBack_Handler)(void);
enum { CB_RETN, CB_RETF, CB_IRET };
enum { CBRET_NONE=0, CBRET_STOP=1 };
uint8_t CALLBACK_Allocate();
bool CALLBACK_Setup(Bitu,CallBack_Handler,Bitu,const char*);
#endif
