/////////////////////////////////////////////////////////////////////////
// $Id: ne2k.h,v 1.11.2.3 2003/04/06 17:29:49 bdenney Exp $
/////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2001  MandrakeSoft S.A.
//
//    MandrakeSoft S.A.
//    43, rue d'Aboukir
//    75002 Paris - France
//    http://www.linux-mandrake.com/
//    http://www.mandrakesoft.com/
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Peter Grehan (grehan@iprg.nokia.com) coded all of this
// NE2000/ether stuff.

//
// An implementation of an ne2000 ISA ethernet adapter. This part uses
// a National Semiconductor DS-8390 ethernet MAC chip, with some h/w
// to provide a windowed memory region for the chip and a MAC address.
//

#include "dosbox.h"

#define bx_bool int
#define bx_param_c uint8_t


#  define BX_NE2K_SMF
#  define BX_NE2K_THIS_PTR 
#  define BX_NE2K_THIS	
//#define BX_INFO 
//LOG_MSG
//#define BX_DEBUG 
//LOG_MSG

#define  BX_NE2K_MEMSIZ    (32*1024)
#define  BX_NE2K_MEMSTART  (16*1024)
#define  BX_NE2K_MEMEND    (BX_NE2K_MEMSTART + BX_NE2K_MEMSIZ)

typedef struct {
  //
  // ne2k register state
   
  //
  // Page 0
  //
  //  Command Register - 00h read/write
  struct CR_t {
    bx_bool  stop;		// STP - Software Reset command
    bx_bool  start;		// START - start the NIC
    bx_bool  tx_packet;	// TXP - initiate packet transmission
    uint8_t    rdma_cmd;      // RD0,RD1,RD2 - Remote DMA command
    uint8_t	 pgsel;		// PS0,PS1 - Page select
  } CR;
  // Interrupt Status Register - 07h read/write
  struct ISR_t {  
    bx_bool  pkt_rx;       	// PRX - packet received with no errors
    bx_bool  pkt_tx;       	// PTX - packet transmitted with no errors
    bx_bool  rx_err;	// RXE - packet received with 1 or more errors
    bx_bool  tx_err;	// TXE - packet tx'd       "  " "    "    "
    bx_bool  overwrite;	// OVW - rx buffer resources exhausted
    bx_bool  cnt_oflow;     // CNT - network tally counter MSB's set
    bx_bool  rdma_done;     // RDC - remote DMA complete
    bx_bool  reset;		// RST - reset status
  } ISR;
  // Interrupt Mask Register - 0fh write
  struct IMR_t {
    bx_bool  rx_inte;	// PRXE - packet rx interrupt enable
    bx_bool  tx_inte;	// PTXE - packet tx interrput enable
    bx_bool  rxerr_inte;	// RXEE - rx error interrupt enable
    bx_bool  txerr_inte;	// TXEE - tx error interrupt enable
    bx_bool  overw_inte;	// OVWE - overwrite warn int enable
    bx_bool  cofl_inte;	// CNTE - counter o'flow int enable
    bx_bool  rdma_inte;	// RDCE - remote DMA complete int enable
    bx_bool  reserved;	//  D7 - reserved
  } IMR;
  // Data Configuration Register - 0eh write
  struct DCR_t {
    bx_bool  wdsize;	// WTS - 8/16-bit select
    bx_bool  endian;	// BOS - byte-order select
    bx_bool  longaddr;	// LAS - long-address select
    bx_bool  loop;		// LS  - loopback select
    bx_bool  auto_rx;	// AR  - auto-remove rx packets with remote DMA
    uint8_t    fifo_size;	// FT0,FT1 - fifo threshold
  } DCR;
  // Transmit Configuration Register - 0dh write
  struct TCR_t {
    bx_bool  crc_disable;	// CRC - inhibit tx CRC
    uint8_t    loop_cntl;	// LB0,LB1 - loopback control
    bx_bool  ext_stoptx;    // ATD - allow tx disable by external mcast
    bx_bool  coll_prio;	// OFST - backoff algorithm select
    uint8_t    reserved;      //  D5,D6,D7 - reserved
  } TCR;
  // Transmit Status Register - 04h read
  struct TSR_t {
    bx_bool  tx_ok;		// PTX - tx complete without error
    bx_bool  reserved;	//  D1 - reserved
    bx_bool  collided;	// COL - tx collided >= 1 times
    bx_bool  aborted;	// ABT - aborted due to excessive collisions
    bx_bool  no_carrier;	// CRS - carrier-sense lost
    bx_bool  fifo_ur;	// FU  - FIFO underrun
    bx_bool  cd_hbeat;	// CDH - no tx cd-heartbeat from transceiver
    bx_bool  ow_coll;	// OWC - out-of-window collision
  } TSR;
  // Receive Configuration Register - 0ch write
  struct RCR_t {
    bx_bool  errors_ok;	// SEP - accept pkts with rx errors
    bx_bool  runts_ok;	// AR  - accept < 64-byte runts
    bx_bool  broadcast;	// AB  - accept eth broadcast address
    bx_bool  multicast;	// AM  - check mcast hash array
    bx_bool  promisc;	// PRO - accept all packets
    bx_bool  monitor;	// MON - check pkts, but don't rx
    uint8_t    reserved;	//  D6,D7 - reserved
  } RCR;
  // Receive Status Register - 0ch read
  struct RSR_t {
    bx_bool  rx_ok;		// PRX - rx complete without error
    bx_bool  bad_crc;	// CRC - Bad CRC detected
    bx_bool  bad_falign;	// FAE - frame alignment error
    bx_bool  fifo_or;	// FO  - FIFO overrun
    bx_bool  rx_missed;	// MPA - missed packet error
    bx_bool  rx_mbit;	// PHY - unicast or mcast/bcast address match
    bx_bool  rx_disabled;   // DIS - set when in monitor mode
    bx_bool  deferred;	// DFR - collision active
  } RSR;

  uint16_t local_dma;	// 01,02h read ; current local DMA addr
  uint8_t  page_start;  // 01h write ; page start register
  uint8_t  page_stop;   // 02h write ; page stop register
  uint8_t  bound_ptr;   // 03h read/write ; boundary pointer
  uint8_t  tx_page_start; // 04h write ; transmit page start register
  uint8_t  num_coll;    // 05h read  ; number-of-collisions register
  uint16_t tx_bytes;    // 05,06h write ; transmit byte-count register
  uint8_t  fifo;	// 06h read  ; FIFO
  uint16_t remote_dma;  // 08,09h read ; current remote DMA addr
  uint16_t remote_start;  // 08,09h write ; remote start address register
  uint16_t remote_bytes;  // 0a,0bh write ; remote byte-count register
  uint8_t  tallycnt_0;  // 0dh read  ; tally counter 0 (frame align errors)
  uint8_t  tallycnt_1;  // 0eh read  ; tally counter 1 (CRC errors)
  uint8_t  tallycnt_2;  // 0fh read  ; tally counter 2 (missed pkt errors)

  //
  // Page 1
  //
  //   Command Register 00h (repeated)
  //
  uint8_t  physaddr[6];  // 01-06h read/write ; MAC address
  uint8_t  curr_page;    // 07h read/write ; current page register
  uint8_t  mchash[8];    // 08-0fh read/write ; multicast hash array

  //
  // Page 2  - diagnostic use only
  // 
  //   Command Register 00h (repeated)
  //
  //   Page Start Register 01h read  (repeated)
  //   Page Stop Register  02h read  (repeated)
  //   Current Local DMA Address 01,02h write (repeated)
  //   Transmit Page start address 04h read (repeated)
  //   Receive Configuration Register 0ch read (repeated)
  //   Transmit Configuration Register 0dh read (repeated)
  //   Data Configuration Register 0eh read (repeated)
  //   Interrupt Mask Register 0fh read (repeated)
  //
  uint8_t  rempkt_ptr;   // 03h read/write ; remote next-packet pointer
  uint8_t  localpkt_ptr; // 05h read/write ; local next-packet pointer
  uint16_t address_cnt;  // 06,07h read/write ; address counter

    //
    // Page 3  - should never be modified.
    //

    // Novell ASIC state
  uint8_t  macaddr[32];          // ASIC ROM'd MAC address, even bytes
  uint8_t  mem[BX_NE2K_MEMSIZ];  // on-chip packet memory

    // ne2k internal state
  uint32_t base_address;
  int    base_irq;
  int    tx_timer_index;
  int    tx_timer_active;
} bx_ne2k_t;



class bx_ne2k_c  {
public:
  bx_ne2k_c(void);
  virtual ~bx_ne2k_c(void);
  virtual void init(void);
  virtual void reset(unsigned type);

public:
  bx_ne2k_t s;

  /* TODO: Setup SDL */
  //eth_pktmover_c *ethdev;

  BX_NE2K_SMF uint32_t read_cr(void);
  BX_NE2K_SMF void   write_cr(uint32_t value);

  BX_NE2K_SMF uint32_t chipmem_read(uint32_t address, unsigned int io_len);
  BX_NE2K_SMF uint32_t asic_read(uint32_t offset, unsigned int io_len);
  BX_NE2K_SMF uint32_t page0_read(uint32_t offset, unsigned int io_len);
  BX_NE2K_SMF uint32_t page1_read(uint32_t offset, unsigned int io_len);
  BX_NE2K_SMF uint32_t page2_read(uint32_t offset, unsigned int io_len);
  BX_NE2K_SMF uint32_t page3_read(uint32_t offset, unsigned int io_len);

  BX_NE2K_SMF void chipmem_write(uint32_t address, uint32_t value, unsigned io_len);
  BX_NE2K_SMF void asic_write(uint32_t address, uint32_t value, unsigned io_len);
  BX_NE2K_SMF void page0_write(uint32_t address, uint32_t value, unsigned io_len);
  BX_NE2K_SMF void page1_write(uint32_t address, uint32_t value, unsigned io_len);
  BX_NE2K_SMF void page2_write(uint32_t address, uint32_t value, unsigned io_len);
  BX_NE2K_SMF void page3_write(uint32_t address, uint32_t value, unsigned io_len);

public:
  static void tx_timer_handler(void *);
  BX_NE2K_SMF void tx_timer(void);

  //static void rx_handler(void *arg, const void *buf, unsigned len);
  BX_NE2K_SMF unsigned mcast_index(const void *dst);
  BX_NE2K_SMF void rx_frame(const void *buf, unsigned io_len);


  static uint32_t read_handler(void *this_ptr, uint32_t address, unsigned io_len);
  static void   write_handler(void *this_ptr, uint32_t address, uint32_t value, unsigned io_len);
#if !BX_USE_NE2K_SMF
  uint32_t read(uint32_t address, unsigned io_len);
  void   write(uint32_t address, uint32_t value, unsigned io_len);
#endif


};

