#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "setup.h"

/*

This is DosBox handler of 93c46 copy-protection dongle connected to LPT port.
At least Rainbow Sentinel Cplus and MicroPhar are 93c46-based dongles.

93c46 memory chip contain 64*16 words. More on it:
http://www.atmel.com/dyn/resources/prod_documents/doc0172.pdf

Few notes:

* It is unable to detect dongle presence on software side, so, software
usually reading some cell (often 0x3F) and check magic value.

* Wiring scheme may differ from dongle to dongle, but usually, 
DI (data input), SK (clock), CS (chip select) and power lines are 
taken from D0..D7 in some order.

* DO (data output) may be connected to ACK or BUSY printer lines.

Add this file to DosBox project, patch dosbox.cpp patch and add to dosbox.conf
"dongle=true" under "[speaker]" section.

More information: http://blogs.conus.info/node/56

-- dennis@conus.info

*/

#define DONGLE_BASE 0x0378

static void DONGLE_disable(Bitu) {
}

static void DONGLE_enable(Bitu freq) {
}

#define IS_SET(flag, bit)       ((flag) & (bit))

static bool queue_filling=false;
static bool queue_filled=false;
static int queue_idx=0;
static int queue[3+6];
static int out_idx;

static int last_SK=0;
static bool ackbit=false;

static int ADR;

static unsigned short MEMORY[0x40]=
{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     // 0
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     // 8
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     // 10
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     // 18
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     // 20
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     // 28
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     // 30
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };   // 38

//#include <windows.h>

static void dongle_write(Bitu port,Bitu val,Bitu iolen) {
	static int DI, SK;
	/*
	LOG(LOG_MISC,LOG_NORMAL)("write dongle port=%x val=%d%d%d%d%d%d%d%d\n",
	port,
	IS_SET (val, 1<<7) ? 1 : 0,
	IS_SET (val, 1<<6) ? 1 : 0,
	IS_SET (val, 1<<5) ? 1 : 0,
	IS_SET (val, 1<<4) ? 1 : 0,
	IS_SET (val, 1<<3) ? 1 : 0,
	IS_SET (val, 1<<2) ? 1 : 0,
	IS_SET (val, 1<<1) ? 1 : 0,
	IS_SET (val, 1<<0) ? 1 : 0);
	*/
	DI=IS_SET (val, 1<<7) ? 1 : 0;
	SK=IS_SET (val, 1<<6) ? 1 : 0;

	if (last_SK==0 && SK==1) // posedge
	{
		if (queue_filled)
		{
			if (((MEMORY[ADR]>>out_idx)&1)==1)
				ackbit=false; // swap it if ACK is negated...
			else
				ackbit=true;

			if (out_idx==0)
				queue_filled=false;
			else
				out_idx--;
		};

		if (queue_filling==false && DI==1) // start bit
		{                                
			//got start bit
			queue_filling=true;
			queue_filled=false;
			queue_idx=0;
		};

		if (queue_filling)
		{
			queue[queue_idx]=DI;
			if (queue_idx==8)
			{ // last bit filled

				int OP1=queue[1];
				int OP2=queue[2];

				ADR=(queue[3]<<5) | (queue[4]<<4) | (queue[5]<<3) | (queue[6]<<2) | (queue[7]<<1) | (queue[8]);

				if (OP1==1 && OP2==0) // read
				{
					LOG(LOG_MISC,LOG_NORMAL)("93c46 dongle: trying to read at address 0x%x", ADR);

					queue_filling=false;
					queue_filled=true;
					out_idx=15;
				}
				else
					LOG(LOG_MISC,LOG_NORMAL)("93c46 dongle: OP1=%d; OP2=%d: this command is not handled yet", OP1, OP2);
			}
			else
				queue_idx++;
		};
	};

	last_SK=SK;
}

static Bitu dongle_read(Bitu port,Bitu iolen) {
	Bitu retval;
	switch (port-DONGLE_BASE) 
	{
	case 0:		/* Data Port */
		return 0;
		break;

	case 1:		/* Status Port */	
		if (ackbit==true)
			retval=0x40;
		else
			retval=0;

		return retval;
		break;

	case 2:		/* Control Port */
		return 0;
		break;
	}
	return 0xff;
}

class DONGLE: public Module_base {
private:
	IO_ReadHandleObject ReadHandler;
	IO_WriteHandleObject WriteHandler;
public:
	DONGLE(Section* configuration):Module_base(configuration) 
	{
		Section_prop * section=static_cast<Section_prop *>(configuration);
		if(!section->Get_bool("dongle")) return;

		WriteHandler.Install(DONGLE_BASE,dongle_write,IO_MB,3);
		ReadHandler.Install(DONGLE_BASE,dongle_read,IO_MB,3);

		DONGLE_disable(0);
	}
	~DONGLE(){
		DONGLE_disable(0);
	}
};

static DONGLE* test;

static void DONGLE_ShutDown(Section* sec){
	delete test;
}

void DONGLE_Init(Section* sec) {
	test = new DONGLE(sec);
	sec->AddDestroyFunction(&DONGLE_ShutDown,true);
}
