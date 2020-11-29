/*
 *  Copyright (C) 2002-2020  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef DOSBOX_SERIALPORT_H
#define DOSBOX_SERIALPORT_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_INOUT_H
#include "inout.h"
#endif
#ifndef DOSBOX_TIMER_H
#include "timer.h"
#endif
#ifndef DOSBOX_DOS_INC_H
#include "dos_inc.h"
#endif
#ifndef DOSBOX_PROGRAMS_H
#include "programs.h"
#endif

// set this to 1 for serial debugging in release mode
#define SERIAL_DBG_FORCED 1

#if (C_DEBUG || SERIAL_DBG_FORCED)
#define SERIAL_DEBUG 1
#endif

#if SERIAL_DEBUG
#include "hardware.h"
#endif

// Serial port interface 

class MyFifo {
public:
	MyFifo(Bitu maxsize_) {
		maxsize=size=maxsize_;
		pos=used=0;
		data=new uint8_t[size];
	}
	~MyFifo() {
		if (data != NULL) {
			delete[] data;
			data = NULL;
		}
	}
	INLINE Bitu getFree(void) {
		return size-used;
	}
	bool isEmpty() {
		return used==0;
	}
	bool isFull() {
		return (size-used)==0;
	}

	INLINE Bitu getUsage(void) {
		return used;
	}
	void setSize(Bitu newsize)
	{
		size=newsize;
		pos=used=0;
	}
	void clear(void) {
		pos=used=0;
		data[0]=0;
	}

	bool addb(uint8_t _val) {
		Bitu where=pos+used;
		if (where>=size) where-=size;
		if(used>=size) {
			// overwrite last byte
			if(where==0) where=size-1;
			else where--;
			data[where]=_val;
			return false;
		}
		data[where]=_val;
		used++;
		return true;
	}
	uint8_t getb() {
		if (!used) return data[pos];
		Bitu where=pos;
		used--;
		if(used) pos++;
		if (pos>=size) pos-=size;
		return data[where];
	}
	uint8_t getTop() {
		Bitu where=pos+used;
		if (where>=size) where-=size;
		if(used>=size) {
			if(where==0) where=size-1;
			else where--;
		}
		return data[where];
	}

	uint8_t probeByte() {
		return data[pos];
	}
private:
	uint8_t * data;
	Bitu maxsize,size,pos,used;
};

class CSerial {
public:

#if SERIAL_DEBUG
	FILE * debugfp;
	bool dbg_modemcontrol; // RTS,CTS,DTR,DSR,RI,CD
	bool dbg_serialtraffic;
	bool dbg_register;
	bool dbg_interrupt;
	bool dbg_aux;
	void log_ser(bool active, char const* format,...);
#endif

	static bool getBituSubstring(const char* name,Bitu* data, CommandLine* cmd);

	bool InstallationSuccessful;// check after constructing. If
								// something was wrong, delete it right away.

	// Constructor takes com port number (0-3)
	CSerial(Bitu id, CommandLine* cmd);
	
	virtual ~CSerial();
		
	IO_ReadHandleObject ReadHandler[8];
	IO_WriteHandleObject WriteHandler[8];

	float bytetime; // how long a byte takes to transmit/receive in milliseconds
	void changeLineProperties();
	Bitu idnumber;

	void setEvent(uint16_t type, float duration);
	void removeEvent(uint16_t type);
	void handleEvent(uint16_t type);
	virtual void handleUpperEvent(uint16_t type)=0;
	
	// defines for event type
#define SERIAL_TX_LOOPBACK_EVENT 0
#define SERIAL_THR_LOOPBACK_EVENT 1
#define SERIAL_ERRMSG_EVENT 2

#define SERIAL_TX_EVENT 3
#define SERIAL_RX_EVENT 4
#define SERIAL_POLLING_EVENT 5
#define SERIAL_THR_EVENT 6
#define SERIAL_RX_TIMEOUT_EVENT 7

#define	SERIAL_BASE_EVENT_COUNT 7

#define COMNUMBER idnumber+1

	Bitu irq;
	
	// CSerial requests an update of the input lines
	virtual void updateMSR()=0;

	// Control lines from prepherial to serial port
	bool getDTR();
	bool getRTS();

	bool getRI();
	bool getCD();
	bool getDSR();
	bool getCTS();

	void setRI(bool value);
	void setDSR(bool value);
	void setCD(bool value);
	void setCTS(bool value);

	// From serial port to prepherial
	// set output lines
	virtual void setRTSDTR(bool rts, bool dtr)=0;
	virtual void setRTS(bool val)=0;
	virtual void setDTR(bool val)=0;

	// Register access
	void Write_THR(uint8_t data);
	void Write_IER(uint8_t data);
	void Write_FCR(uint8_t data);
	void Write_LCR(uint8_t data);
	void Write_MCR(uint8_t data);
	// Really old hardware seems to have the delta part of this register writable
	void Write_MSR(uint8_t val);
	void Write_SPR(uint8_t data);
	void Write_reserved(uint8_t data, uint8_t address);

	Bitu Read_RHR();
	Bitu Read_IER();
	Bitu Read_ISR();
	Bitu Read_LCR();
	Bitu Read_MCR();
	Bitu Read_LSR();
	Bitu Read_MSR();
	Bitu Read_SPR();
	
	// If a byte comes from loopback or prepherial, put it in here.
	void receiveByte(uint8_t data);
	void receiveByteEx(uint8_t data, uint8_t error);

	// depratched
	// connected device checks, if port can receive data:
	bool CanReceiveByte();
	
	// when THR was shifted to TX
	void ByteTransmitting();
	
	// When done sending, notify here
	void ByteTransmitted();

	// Transmit byte to prepherial
	virtual void transmitByte(uint8_t val, bool first)=0;

	// switch break state to the passed value
	virtual void setBreak(bool value)=0;
	
	// change baudrate, number of bits, parity, word length al at once
	virtual void updatePortConfig(uint16_t divider, uint8_t lcr)=0;
	
	void Init_Registers();
	
	bool Putchar(uint8_t data, bool wait_dsr, bool wait_cts, Bitu timeout);
	bool Getchar(uint8_t* data, uint8_t* lsr, bool wait_dsr, Bitu timeout);

	DOS_Device* mydosdevice;

	void registerDOSDevice();
	void unregisterDOSDevice();

private:

	// I used this spec: st16c450v420.pdf

	void ComputeInterrupts();
	
	// a sub-interrupt is triggered
	void rise(uint8_t priority);

	// clears the pending sub-interrupt
	void clear(uint8_t priority);
	
	#define ERROR_PRIORITY 4	// overrun, parity error, frame error, break
	#define RX_PRIORITY 1		// a byte has been received
	#define TX_PRIORITY 2		// tx buffer has become empty
	#define MSR_PRIORITY 8		// CRS, DSR, RI, DCD change 
	#define TIMEOUT_PRIORITY 0x10
	#define NONE_PRIORITY 0

	uint8_t waiting_interrupts;	// these are on, but maybe not enabled
	
	// 16C550
	//				read/write		name

	uint16_t baud_divider;
	#define RHR_OFFSET 0	// r Receive Holding Register, also LSB of Divisor Latch (r/w)
							// Data: whole byte
	#define THR_OFFSET 0	// w Transmit Holding Register
							// Data: whole byte
	uint8_t IER;	//	r/w		Interrupt Enable Register, also MSB of Divisor Latch
	#define IER_OFFSET 1

	bool irq_active;
				
	#define RHR_INT_Enable_MASK				0x1
	#define THR_INT_Enable_MASK				0x2
	#define Receive_Line_INT_Enable_MASK	0x4
	#define Modem_Status_INT_Enable_MASK	0x8

	uint8_t ISR;	//	r				Interrupt Status Register
	#define ISR_OFFSET 2

	#define ISR_CLEAR_VAL 0x1
	#define ISR_FIFOTIMEOUT_VAL 0xc
	#define ISR_ERROR_VAL 0x6
	#define ISR_RX_VAL 0x4
	#define ISR_TX_VAL 0x2
	#define ISR_MSR_VAL 0x0
public:	
	uint8_t LCR;	//	r/w				Line Control Register
private:
	#define LCR_OFFSET 3
						// bit0: word length bit0
						// bit1: word length bit1
						// bit2: stop bits
						// bit3: parity enable
						// bit4: even parity
						// bit5: set parity
						// bit6: set break
						// bit7: divisor latch enable

	
	#define	LCR_BREAK_MASK 0x40
	#define LCR_DIVISOR_Enable_MASK 0x80
	#define LCR_PORTCONFIG_MASK 0x3F
	
	#define LCR_PARITY_NONE		0x0
	#define LCR_PARITY_ODD		0x8
	#define LCR_PARITY_EVEN		0x18
	#define LCR_PARITY_MARK		0x28
	#define LCR_PARITY_SPACE	0x38

	#define LCR_DATABITS_5		0x0
	#define LCR_DATABITS_6		0x1
	#define LCR_DATABITS_7		0x2
	#define LCR_DATABITS_8		0x3

	#define LCR_STOPBITS_1		0x0
	#define LCR_STOPBITS_MORE_THAN_1 0x4

	// Modem Control Register
	// r/w				
	#define MCR_OFFSET 4
	bool dtr;			// bit0: DTR
	bool rts;			// bit1: RTS
	bool op1;			// bit2: OP1
	bool op2;			// bit3: OP2
	bool loopback;		// bit4: loop back enable

	#define MCR_DTR_MASK 0x1
	#define MCR_RTS_MASK 0x2	
	#define MCR_OP1_MASK 0x4	
	#define MCR_OP2_MASK 0x8
	#define MCR_LOOPBACK_Enable_MASK 0x10
public:	
	uint8_t LSR;	//	r				Line Status Register
private:

	#define LSR_OFFSET 5

	#define LSR_RX_DATA_READY_MASK 0x1
	#define LSR_OVERRUN_ERROR_MASK 0x2
	#define LSR_PARITY_ERROR_MASK 0x4
	#define LSR_FRAMING_ERROR_MASK 0x8
	#define LSR_RX_BREAK_MASK 0x10
	#define LSR_TX_HOLDING_EMPTY_MASK 0x20
	#define LSR_TX_EMPTY_MASK 0x40

	#define LSR_ERROR_MASK 0x1e

	// error printing
	bool errormsg_pending;
	Bitu framingErrors;
	Bitu parityErrors;
	Bitu overrunErrors;
	Bitu txOverrunErrors;
	Bitu overrunIF0;
	Bitu breakErrors;


	// Modem Status Register
	//	r
	#define MSR_OFFSET 6
	bool d_cts;			// bit0: deltaCTS
	bool d_dsr;			// bit1: deltaDSR
	bool d_ri;			// bit2: deltaRI
	bool d_cd;			// bit3: deltaCD
	bool cts;			// bit4: CTS
	bool dsr;			// bit5: DSR
	bool ri;			// bit6: RI
	bool cd;			// bit7: CD
	
	#define MSR_delta_MASK 0xf
	#define MSR_LINE_MASK 0xf0

	#define MSR_dCTS_MASK 0x1
	#define MSR_dDSR_MASK 0x2
	#define MSR_dRI_MASK 0x4
	#define MSR_dCD_MASK 0x8
	#define MSR_CTS_MASK 0x10
	#define MSR_DSR_MASK 0x20
	#define MSR_RI_MASK 0x40
	#define MSR_CD_MASK 0x80

	uint8_t SPR;	//	r/w				Scratchpad Register
	#define SPR_OFFSET 7


	// For loopback purposes...
	uint8_t loopback_data;
	void transmitLoopbackByte(uint8_t val, bool value);

	// 16C550 (FIFO)
	public: // todo remove
	MyFifo* rxfifo;
	private:
	MyFifo* txfifo;
	MyFifo* errorfifo;
	Bitu errors_in_fifo;
	Bitu rx_interrupt_threshold;
	Bitu fifosize;
	uint8_t FCR;
	bool sync_guardtime;
	#define FIFO_STATUS_ACTIVE 0xc0 // FIFO is active AND works ;)
	#define FIFO_ERROR 0x80
	#define FCR_ACTIVATE 0x01
	#define FCR_CLEAR_RX 0x02
	#define FCR_CLEAR_TX 0x04
	#define FCR_OFFSET 2
	#define FIFO_FLOWCONTROL 0x20
};

extern CSerial* serialports[];
const uint8_t serial_defaultirq[] = { 4, 3, 4, 3 };
const uint16_t serial_baseaddr[] = {0x3f8,0x2f8,0x3e8,0x2e8};
const char* const serial_comname[]={"COM1","COM2","COM3","COM4"};

// the COM devices

class device_COM : public DOS_Device {
public:
	// Creates a COM device that communicates with the num-th parallel port, i.e. is LPTnum
	device_COM(class CSerial* sc);
	virtual ~device_COM();
	bool Read(uint8_t * data,uint16_t * size);
	bool Write(const uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * pos,uint32_t type);
	bool Close();
	uint16_t GetInformation(void);
private:
	CSerial* sclass;
};

#endif
