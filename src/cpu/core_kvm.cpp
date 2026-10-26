/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

/* The KVM CPU core (see core_kvm.h and KVM-TODO.md) */

#include "dosbox.h"

#if C_KVM

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <linux/kvm.h>

#include <algorithm>
#include <string>
#include <vector>

#include "callback.h"
#include "core_kvm.h"
#include "cpu.h"
#include "cpu_state.h"
#include "debug.h"
#include "inout.h"
#include "lazyflags.h"
#include "logging.h"
#include "mem.h"
#include "paging.h"
#include "pic.h"
#include "regs.h"
#include "setup.h"
#include "vga.h"

#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id _sigev_un._tid
#endif

Bitu MEM_PageMask(void);
void On_Software_CPU_Reset();
extern bool cpu_triple_fault_reset;

/* Guest physical address of the 3 pages that Intel hosts need for the TSS KVM uses to run real
 * mode code without unrestricted guest support (KVM_SET_TSS_ADDR), and of the page KVM uses for
 * its identity map below it (KVM's default, 0xfffbc000). No memory slot may cover
 * 0xfffbc000-0xfffbffff. DOSBox-X maps nothing there (the ROM BIOS alias is at 0xffff0000). */
static const unsigned long kvm_tss_addr = 0xfffbd000ul;

static struct {
	int		dev = -1;	/* /dev/kvm */
	int		vm = -1;
	int		vcpu = -1;
	struct kvm_run*	run = NULL;	/* the vCPU's shared kvm_run area */
	size_t		run_size = 0;
	int		memslots = 0;	/* how many memory slots the host allows */
	bool		exit_registered = false;
	bool		failed = false;	/* gave up, running the normal core instead */

	/* The CPU state is in the vCPU (and DOSBox-X's copy is out of date) between KVM_PutState()
	 * and CPU_SyncState() or CPU_SyncRegisters(), which call KVM_SyncHook() (see cpu.h):
	 * in_kvm for the registers, rest_in_kvm for the FPU, debug registers and MSRs */
	bool		in_kvm = false;
	bool		rest_in_kvm = false;
	bool		sync_regs = false;	/* registers through kvm_run (KVM_CAP_SYNC_REGS) */
	bool		exit_on_emulation_failure = false;	/* KVM_CAP_EXIT_ON_EMULATION_FAILURE is on */
	bool		emulate_one = false;	/* KVM could not emulate an instruction: the normal core runs it */

	/* Coalesced MMIO: writes to the video memory window are queued in this ring instead of
	 * each exiting, and applied when KVM_RUN returns (see KVM_FlushWrites()) */
	struct kvm_coalesced_mmio_ring*	ring = NULL;
	uint32_t	ring_max = 0;
	bool		pio_queue = false;	/* port writes are queued too (KVM_CAP_COALESCED_PIO) */
	bool		vga_data_queued = false;	/* also the VGA sequencer and graphics controller data ports */

	/* An I/O or MMIO exit is the middle of an instruction: KVM finishes it (with the data the
	 * handler put in kvm_run) at the next KVM_RUN, and until then the vCPU state is not
	 * consistent. See KVM_CompletePending(). */
	bool		pending = false;
	bool		guest_if = false;	/* the guest's IF as of the last exit */

	/* KVM may still be delivering an event (see KVM_DropEvent()): only after a KVM_RUN that a
	 * signal ended (an exit for I/O, MMIO, a callback or HLT happens between instructions,
	 * and an interrupt window opens only when nothing is being delivered), and after a full
	 * state fetch, whose state DOSBox-X may replace entirely (save states, the debugger) */
	bool		event_in_flight = true;

	/* KVM host time, nanoseconds: what going into the guest and out again costs, which is not
	 * charged to the guest (see KVM_Overhead()): for port I/O, MMIO, callback (debug) and HLT
	 * exits, and for each write queued in the coalesced MMIO ring, one instruction each or a
	 * string instruction's (burst) */
	struct Cost {
		uint64_t	avg = 0,min = 0;
	}		exit_cost[4],write_cost,burst_cost;
	uint64_t	last_exit_cost = 0;	/* the estimate for the last exit, 0 for a signal */

	/* statistics, logged when DOSBox-X exits */
	uint64_t	runs = 0,io = 0,mmio = 0,coalesced = 0,callbacks = 0,int3 = 0,hlt = 0,windows = 0,slices = 0,puts = 0,gets = 0,emulated = 0;
} kvm;

/* Guest physical memory as KVM memory slots (see KVM-TODO.md, step 8). A slot maps a run of
 * pages that are host memory to KVM (read-only for MEM_PAGE_READONLY); other pages have no slot,
 * so the guest's accesses to them exit as MMIO to the page handlers. */
struct KVM_Slot {
	uint32_t	id;
	Bitu		page,pages;
	HostPt		host;
	bool		readonly;
	bool		dirtylog;	/* KVM logs the pages the guest writes (the mode 13h shadow) */
};

static struct {
	std::vector<KVM_Slot>	slots;		/* sorted by page */
	std::vector<uint32_t>	free_ids;
	uint32_t		next_id = 0;
	Bitu			pages = 0;	/* guest physical pages covered */
	bool			dirty = true;	/* pages [dirty_lo,dirty_hi] need reclassifying */
	Bitu			dirty_lo = 0,dirty_hi = ~(Bitu)0;
	bool			paging = false;	/* guest paging state the first MB layout is for */

	/* pages left as MMIO because their handler was not known yet (see KVM_ApplyMapChanges()),
	 * one bit per page of the 4GB */
	std::vector<uint64_t>	deferred = std::vector<uint64_t>(0x100000u / 64u,0);
} kvm_mem;

/* KVM's own pages (identity map and TSS, see kvm_tss_addr) */
static inline bool KVM_ReservedPage(Bitu page) {
	return page >= (0xfffbc000ul >> 12u) && page <= (0xfffbf000ul >> 12u);
}

static void KVM_MarkDirty(Bitu lo,Bitu hi) {
	if (!kvm_mem.dirty) {
		kvm_mem.dirty = true;
		kvm_mem.dirty_lo = lo;
		kvm_mem.dirty_hi = hi;
	}
	else {
		if (lo < kvm_mem.dirty_lo) kvm_mem.dirty_lo = lo;
		if (hi > kvm_mem.dirty_hi) kvm_mem.dirty_hi = hi;
	}
}

static void KVM_MapChange(MEM_MapChangeKind kind,Bitu page,Bitu pages) {
	if (pages == 0) {
		if (kind == MEM_MAPCHANGE_FIRSTMB) KVM_MarkDirty(0,LINK_START-1u);
		else KVM_MarkDirty(0,~(Bitu)0);
	}
	else {
		KVM_MarkDirty(page,page+pages-1u);
	}
}

/* What guest physical page 'page' is in KVM's view. Without paging, DOSBox-X remaps the first
 * MB+64KB through paging.firstmb[] (EMS), which KVM's guest physical address space must show. */
/* Chained 256-colour memory (mode 13h) through the memory handler makes every guest write an
 * exit that KVM handles itself, since chain 4 does not map linearly onto VGA memory. While the
 * plain chained handler covers A0000-AFFFF (VGA_ChainedShadowable()), a 64KB shadow in the
 * guest's byte order is mapped there instead, with dirty logging: the pages the guest wrote go
 * to VGA memory when the core returns (and before the memory map changes), so that drawing,
 * callbacks and everything else in DOSBox-X see them, and the shadow is loaded again before
 * the guest runs when DOSBox-X wrote video memory through the memory handlers. */
static struct {
	HostPt			mem = NULL;	/* 64KB, page-aligned */
	bool			active = false;	/* mapped at A0000-AFFFF */
	VGA_ChainedShadowMap	map;		/* where its bytes are in VGA memory */
	uint32_t		writes = 0;	/* VGA_MemWriteCount when it was last loaded */
} kvm_shadow;

static MEM_PageInfo KVM_PageAt(Bitu page) {
	if (page < LINK_START && !kvm_mem.paging)
		page = paging.firstmb[page];
	if (kvm_shadow.active && page >= 0xA0u && page < 0xB0u) {
		MEM_PageInfo info = MEM_ClassifyPage(page);
		info.kind = MEM_PAGE_RAM;
		info.host = kvm_shadow.mem + (page - 0xA0u) * 4096u;
		return info;
	}
	return MEM_ClassifyPage(page);
}

/* The pages of the shadow the guest wrote since the last time, to VGA memory */
static void KVM_ShadowStore(void) {
	if (!kvm_shadow.active) return;
	for (const auto &sl : kvm_mem.slots) {
		if (!sl.dirtylog) continue;
		uint64_t bitmap[1] = {0};	/* one bit per page, at most 16 */
		struct kvm_dirty_log log;
		memset(&log,0,sizeof(log));
		log.slot = sl.id;
		log.dirty_bitmap = bitmap;
		if (ioctl(kvm.vm,KVM_GET_DIRTY_LOG,&log) < 0) return;
		for (Bitu i=0;i < sl.pages && i < 64u;i++) {
			if (bitmap[0] & ((uint64_t)1u << i))
				VGA_ChainedShadowStore(kvm_shadow.map,kvm_shadow.mem + (sl.page + i - 0xA0u) * 4096u,(unsigned int)(sl.page + i - 0xA0u));
		}
	}
}

/* All of the shadow from VGA memory */
static void KVM_ShadowLoad(void) {
	for (unsigned int page=0;page < 16u;page++)
		VGA_ChainedShadowLoad(kvm_shadow.map,kvm_shadow.mem + page * 4096u,page);
	kvm_shadow.writes = VGA_MemWriteCount;
}

/* The map changed at A0000-AFFFF: store what the guest wrote with the old mapping, then
 * decide again whether to map the shadow, and load it */
static void KVM_ShadowUpdate(void) {
	KVM_ShadowStore();
	kvm_shadow.active = false;
	VGA_ChainedShadowMap m;
	if (!VGA_ChainedShadowable(m)) return;
	if (kvm_shadow.mem == NULL) {
		void *p = mmap(NULL,0x10000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
		if (p == MAP_FAILED) return;
		kvm_shadow.mem = (HostPt)p;
	}
	kvm_shadow.map = m;
	kvm_shadow.active = true;
	KVM_ShadowLoad();
}

static bool KVM_SetSlot(uint32_t id,Bitu page,Bitu pages,HostPt host,bool readonly,bool dirtylog) {
	struct kvm_userspace_memory_region r;
	memset(&r,0,sizeof(r));
	r.slot = id;
	r.flags = (readonly ? KVM_MEM_READONLY : 0) | (dirtylog ? KVM_MEM_LOG_DIRTY_PAGES : 0);
	r.guest_phys_addr = (uint64_t)page << 12u;
	r.memory_size = (uint64_t)pages << 12u;	/* 0 deletes the slot */
	r.userspace_addr = (uint64_t)(uintptr_t)host;
	if (ioctl(kvm.vm,KVM_SET_USER_MEMORY_REGION,&r) < 0) {
		LOG_MSG("KVM: KVM_SET_USER_MEMORY_REGION slot %u pages %lx+%lx: %s",(unsigned int)id,
			(unsigned long)page,(unsigned long)pages,strerror(errno));
		return false;
	}
	return true;
}

/* Bring the memory slots up to date with the memory map. Called before each KVM_RUN. */
/* The VGA sequencer and graphics controller data ports (3C5h, 3CFh: map mask, bit mask,
 * set/reset, write mode, read map select, ...) are queued as well while no page of A0000-BFFFF
 * is mapped directly (planar 16-colour modes, mode X, text modes). All video memory accesses
 * then exit or are queued, so they are applied in order with the port writes, also when a
 * port write changes the memory map; the slots follow at the next KVM_RUN. While pages are
 * mapped directly (the mode 13h shadow, the SVGA bank map), a data port write has to take
 * effect before the guest's next access, and only the index ports are queued. */
static void KVM_QueueVGAData(bool on) {
	if (!kvm.pio_queue || on == kvm.vga_data_queued) return;
	for (const uint16_t port : {(uint16_t)0x3C4,(uint16_t)0x3CE}) {
		struct kvm_coalesced_mmio_zone zone;
		memset(&zone,0,sizeof(zone));
		zone.addr = port;
		zone.size = 2;
		zone.pio = 1;
		ioctl(kvm.vm,KVM_UNREGISTER_COALESCED_MMIO,&zone); /* the zones within: index, or index and data */
		zone.size = on ? 2 : 1;
		ioctl(kvm.vm,KVM_REGISTER_COALESCED_MMIO,&zone);
	}
	kvm.vga_data_queued = on;
}

static bool KVM_ApplyMapChanges(void) {
	/* everything the address lines reach, up to 4GB */
	Bitu pages = MEM_PageMask() + 1u;
	if (pages == 0 || pages > 0x100000u) pages = 0x100000u;
	if (kvm_mem.pages != pages) {
		kvm_mem.pages = pages;
		KVM_MarkDirty(0,~(Bitu)0);
	}

	if (kvm_mem.paging != paging.enabled) {
		kvm_mem.paging = paging.enabled;
		KVM_MarkDirty(0,LINK_START-1u);
	}
	if (!kvm_mem.dirty) return true;
	kvm_mem.dirty = false;

	Bitu lo = kvm_mem.dirty_lo,hi = kvm_mem.dirty_hi;
	if (hi >= kvm_mem.pages) hi = kvm_mem.pages - 1u;
	if (lo > hi) return true;

	/* the mode 13h shadow: all of A0000-AFFFF again */
	if (lo <= 0xAFu && hi >= 0xA0u) {
		if (lo > 0xA0u) lo = 0xA0u;
		if (hi < 0xAFu) hi = 0xAFu;
		KVM_ShadowUpdate();
	}

	/* slots that overlap the range are rebuilt whole */
	for (const auto &sl : kvm_mem.slots) {
		if (sl.page <= hi && (sl.page+sl.pages-1u) >= lo) {
			if (sl.page < lo) lo = sl.page;
			if ((sl.page+sl.pages-1u) > hi) hi = sl.page+sl.pages-1u;
		}
	}

	/* the runs of host memory in [lo,hi] */
	std::vector<KVM_Slot> runs;
	KVM_Slot run = {0,0,0,NULL,false,false};
	/* Above the first MB: a page whose handler is not known yet (MEM_PageHandlerKnown()) is not
	 * looked up, which would ask every device about it and makes the first build of all 4GB
	 * slow; it is MMIO until the guest touches it (see KVM_HandleMMIO()). Most of the rest is one
	 * handler that has no memory behind it (nothing there, or a device that is all I/O): a page
	 * whose handler is the last one seen that is not readable is not looked at further. */
	PageHandler *no_memory = NULL;
	for (Bitu page = lo;page <= hi;page++) {
		if (page >= LINK_START) {
			const uint64_t bit = (uint64_t)1u << (page & 63u);
			if (!MEM_PageHandlerKnown(page)) {
				kvm_mem.deferred[page >> 6u] |= bit;
				if (run.pages != 0) runs.push_back(run);
				run.pages = 0;
				continue;
			}
			kvm_mem.deferred[page >> 6u] &= ~bit;
		}
		if (page >= LINK_START && no_memory != NULL && MEM_GetPageHandler(page) == no_memory) {
			if (run.pages != 0) runs.push_back(run);
			run.pages = 0;
			continue;
		}
		const MEM_PageInfo info = KVM_PageAt(page);
		if (page >= LINK_START && !(info.handler->getFlags() & PFLAG_READABLE)) no_memory = info.handler;
		const bool mappable = info.kind != MEM_PAGE_DEVICE && info.host != NULL &&
			((uintptr_t)info.host & 0xFFFu) == 0 && !KVM_ReservedPage(page);
		const bool readonly = info.kind == MEM_PAGE_READONLY;

		const bool dirtylog = kvm_shadow.active && info.host >= kvm_shadow.mem && info.host < kvm_shadow.mem + 0x10000u;

		if (run.pages != 0 && mappable && readonly == run.readonly && dirtylog == run.dirtylog &&
			info.host == run.host + (page - run.page) * 4096u) {
			run.pages++;
			continue;
		}
		if (run.pages != 0) runs.push_back(run);
		run.pages = 0;
		if (mappable) {
			run.page = page;
			run.pages = 1;
			run.host = info.host;
			run.readonly = readonly;
			run.dirtylog = dirtylog;
		}
	}
	if (run.pages != 0) runs.push_back(run);

	/* keep the slots that have not changed, delete the others, add the new runs */
	std::vector<KVM_Slot> kept;
	for (const auto &sl : kvm_mem.slots) {
		const bool in_range = sl.page >= lo && (sl.page+sl.pages-1u) <= hi;
		bool same = !in_range;
		if (in_range) {
			for (auto r = runs.begin();r != runs.end();++r) {
				if (r->page == sl.page && r->pages == sl.pages && r->host == sl.host && r->readonly == sl.readonly &&
					r->dirtylog == sl.dirtylog) {
					runs.erase(r);
					same = true;
					break;
				}
			}
		}
		if (same) {
			kept.push_back(sl);
		}
		else {
			if (!KVM_SetSlot(sl.id,sl.page,0,sl.host,sl.readonly,false)) return false;
			kvm_mem.free_ids.push_back(sl.id);
		}
	}
	for (auto &r : runs) {
		if (!kvm_mem.free_ids.empty()) {
			r.id = kvm_mem.free_ids.back();
			kvm_mem.free_ids.pop_back();
		}
		else {
			if (kvm_mem.next_id >= (uint32_t)kvm.memslots) {
				LOG_MSG("KVM: out of memory slots");
				return false;
			}
			r.id = kvm_mem.next_id++;
		}
		if (!KVM_SetSlot(r.id,r.page,r.pages,r.host,r.readonly,r.dirtylog)) return false;
		kept.push_back(r);
	}
	std::sort(kept.begin(),kept.end(),[](const KVM_Slot &a,const KVM_Slot &b) { return a.page < b.page; });
	kvm_mem.slots.swap(kept);

	bool vga_direct = false;
	for (const auto &sl : kvm_mem.slots) {
		if (sl.page <= 0xBFu && (sl.page+sl.pages-1u) >= 0xA0u) vga_direct = true;
	}
	KVM_QueueVGAData(!vga_direct);
	return true;
}

/* Moving the CPU state between DOSBox-X and the vCPU (step 11) */
static void KVM_SegmentToKVM(const CPU_FullSegment &f,struct kvm_segment &k) {
	k.base = f.base;
	k.limit = f.limit;
	k.selector = f.selector;
	k.type = f.type;
	k.present = f.present ? 1 : 0;
	k.dpl = f.dpl;
	k.db = f.db ? 1 : 0;
	k.s = f.s ? 1 : 0;
	k.l = 0;
	k.g = f.g ? 1 : 0;
	k.avl = f.avl ? 1 : 0;
	k.unusable = f.unusable ? 1 : 0;
	/* a code or data segment in use is marked accessed, as the CPU does when it loads one */
	if (f.s && !f.unusable) k.type |= 1;
}

static void KVM_SegmentFromKVM(const struct kvm_segment &k,CPU_FullSegment &f) {
	f.base = (uint32_t)k.base;
	f.limit = k.limit;
	f.selector = k.selector;
	f.type = k.type;
	f.present = k.present != 0;
	f.dpl = k.dpl;
	f.db = k.db != 0;
	f.s = k.s != 0;
	f.g = k.g != 0;
	f.avl = k.avl != 0;
	f.unusable = k.unusable != 0;
}

/* The MSRs the state has: SYSENTER CS, ESP, EIP */
struct KVM_MSRs {
	alignas(8) uint8_t	buf[sizeof(struct kvm_msrs) + 3 * sizeof(struct kvm_msr_entry)];
	struct kvm_msrs*	h;
	struct kvm_msr_entry*	e;

	KVM_MSRs() {
		memset(buf,0,sizeof(buf));
		h = (struct kvm_msrs*)buf;
		e = (struct kvm_msr_entry*)(buf + sizeof(struct kvm_msrs));
		h->nmsrs = 3;
		e[0].index = 0x174; e[1].index = 0x175; e[2].index = 0x176;
	}
};

/* The general registers and segment registers. With KVM_CAP_SYNC_REGS they go through
 * kvm_run->s.regs: KVM stores them there on every return from KVM_RUN and loads what
 * kvm_dirty_regs marks before the next, so no ioctl is needed. */
static bool KVM_ReadRegs(struct kvm_regs &r,struct kvm_sregs &sr) {
	if (kvm.sync_regs) {
		r = kvm.run->s.regs.regs;
		sr = kvm.run->s.regs.sregs;
		return true;
	}
	return ioctl(kvm.vcpu,KVM_GET_REGS,&r) >= 0 && ioctl(kvm.vcpu,KVM_GET_SREGS,&sr) >= 0;
}

static bool KVM_WriteRegs(const struct kvm_regs &r,const struct kvm_sregs &sr) {
	if (kvm.sync_regs) {
		kvm.run->s.regs.regs = r;
		kvm.run->kvm_dirty_regs |= KVM_SYNC_X86_REGS;
		/* loading the segment registers is the expensive part, and they often stay the same */
		if (memcmp(&kvm.run->s.regs.sregs,&sr,sizeof(sr)) != 0) {
			kvm.run->s.regs.sregs = sr;
			kvm.run->kvm_dirty_regs |= KVM_SYNC_X86_SREGS;
		}
		return true;
	}
	return ioctl(kvm.vcpu,KVM_SET_SREGS,&sr) >= 0 && ioctl(kvm.vcpu,KVM_SET_REGS,&r) >= 0;
}

/* An event the vCPU was delivering when KVM_RUN returned: a software interrupt or an exception
 * whose delivery was cut short (for example by a page fault on the stack, after which KVM saw the
 * timer signal). KVM delivers it again at the next KVM_RUN, on top of whatever state is there.
 * The instruction that raised it has not completed, RIP still points at it. When DOSBox-X has
 * changed the state (it delivered an interrupt), the event would be delivered in the wrong place,
 * with the wrong return address on the stack, so drop it: the guest runs the instruction again
 * when it comes back to it, which raises the event again. An NMI is kept as pending instead.
 * KVM_GET_VCPU_EVENTS does not report software interrupts and software exceptions (INT n,
 * INT3, INTO) at all, but KVM_SET_VCPU_EVENTS replaces them too, so the events are always
 * written back here, with nothing being delivered. */
static bool KVM_DropEvent(void) {
	struct kvm_vcpu_events ev;
	if (ioctl(kvm.vcpu,KVM_GET_VCPU_EVENTS,&ev) < 0) return false;

	ev.exception.injected = 0;
	ev.interrupt.injected = 0;
	if (ev.nmi.injected) {
		ev.nmi.injected = 0;
		ev.nmi.pending = 1;
		ev.flags |= KVM_VCPUEVENT_VALID_NMI_PENDING;
	}

	return ioctl(kvm.vcpu,KVM_SET_VCPU_EVENTS,&ev) >= 0;
}

/* Put the state into the vCPU: the registers (general, segment, control, flags) and/or the rest
 * (FPU, debug registers, MSRs), which only changes when DOSBox-X itself runs the guest or
 * replaces the state, so it is moved only when needed */
static bool KVM_PutState(bool regs,bool rest) {
	CPU_FullState st;
	CPU_GetFullState(st);

	if (regs) {
		struct kvm_regs r;
		struct kvm_sregs sr;
		if (!KVM_ReadRegs(r,sr)) return false;
		const struct kvm_regs old_r = r;
		const struct kvm_sregs old_sr = sr;

		for (unsigned int i=0;i < 6;i++) {
			static const unsigned int order[6] = {es,cs,ss,ds,fs,gs};
			struct kvm_segment *k[6] = {&sr.es,&sr.cs,&sr.ss,&sr.ds,&sr.fs,&sr.gs};
			KVM_SegmentToKVM(st.seg[order[i]],*k[i]);
			/* virtual 8086 mode: the CPU requires what loading the selector gives (VM entry checks) */
			if (st.eflags & FLAG_VM) {
				k[i]->base = (uint64_t)k[i]->selector << 4u;
				k[i]->limit = 0xFFFF;
				k[i]->type = 3; k[i]->s = 1; k[i]->dpl = 3; k[i]->present = 1;
				k[i]->db = 0; k[i]->g = 0; k[i]->avl = 0; k[i]->unusable = 0;
			}
		}
		KVM_SegmentToKVM(st.tr,sr.tr);
		if (st.tr.unusable) {
			/* no task register loaded: the CPU still needs a usable busy TSS there */
			sr.tr.base = 0; sr.tr.limit = 0xFFFF; sr.tr.type = 0xB; sr.tr.present = 1;
			sr.tr.s = 0; sr.tr.unusable = 0;
		}
		KVM_SegmentToKVM(st.ldt,sr.ldt);

		/* Where DOSBox-X has not loaded a segment register since the state was fetched (same
		 * selector, base and limit, same mode), keep the vCPU's own descriptor cache. DOSBox-X
		 * does not keep the access rights and rebuilds them from the mode and the descriptor
		 * tables, which is wrong where the cache does not match them: between setting or
		 * clearing CR0.PE and the far jump after it (a real mode CS became a not present
		 * segment, and the guest triple faulted), or after the guest changed a descriptor that
		 * is loaded. Not when DOSBox-X changed the mode itself (its EMM386 entering virtual 8086
		 * mode loads the same selectors), and not in virtual 8086 mode, where the normalised
		 * segments above are exact. */
		if ((old_sr.cr0 & 1u) == (st.cr0 & 1u) && (old_r.rflags & FLAG_VM) == (st.eflags & FLAG_VM) && !(st.eflags & FLAG_VM)) {
			struct kvm_segment *n[8] = {&sr.es,&sr.cs,&sr.ss,&sr.ds,&sr.fs,&sr.gs,&sr.tr,&sr.ldt};
			const struct kvm_segment *o[8] = {&old_sr.es,&old_sr.cs,&old_sr.ss,&old_sr.ds,&old_sr.fs,&old_sr.gs,&old_sr.tr,&old_sr.ldt};
			for (unsigned int i=0;i < 8;i++) {
				if (n[i]->selector == o[i]->selector && n[i]->base == o[i]->base && n[i]->limit == o[i]->limit)
					*n[i] = *o[i];
			}
		}
		sr.gdt.base = st.gdt.base; sr.gdt.limit = st.gdt.limit;
		sr.idt.base = st.idt.base; sr.idt.limit = st.idt.limit;
		sr.cr0 = st.cr0; sr.cr2 = st.cr2; sr.cr3 = st.cr3; sr.cr4 = st.cr4;

		memset(&r,0,sizeof(r));
		r.rax = st.eax; r.rcx = st.ecx; r.rdx = st.edx; r.rbx = st.ebx;
		r.rsp = st.esp; r.rbp = st.ebp; r.rsi = st.esi; r.rdi = st.edi;
		r.rip = st.eip;
		r.rflags = st.eflags | 2u;
		if (kvm.event_in_flight && (memcmp(&r,&old_r,sizeof(r)) != 0 || memcmp(&sr,&old_sr,sizeof(sr)) != 0)) {
			if (!KVM_DropEvent()) return false;
			kvm.event_in_flight = false;
		}
		if (!KVM_WriteRegs(r,sr)) return false;
	}

	if (rest) {
		struct kvm_fpu f;
		memset(&f,0,sizeof(f));
		f.fcw = st.fcw; f.fsw = st.fsw; f.ftwx = st.ftw;
		for (unsigned int i=0;i < 8;i++) {
			memcpy(f.fpr[i],st.st[i],10);
			memcpy(f.xmm[i],st.xmm[i],16);
		}
		f.mxcsr = st.mxcsr;
		if (ioctl(kvm.vcpu,KVM_SET_FPU,&f) < 0) return false;

		struct kvm_debugregs d;
		memset(&d,0,sizeof(d));
		for (unsigned int i=0;i < 4;i++) d.db[i] = st.dr[i];
		d.dr6 = st.dr[6]; d.dr7 = st.dr[7];
		if (ioctl(kvm.vcpu,KVM_SET_DEBUGREGS,&d) < 0) return false;

		KVM_MSRs m;
		m.e[0].data = st.sysenter_cs;
		m.e[1].data = st.sysenter_esp;
		m.e[2].data = st.sysenter_eip;
		if (ioctl(kvm.vcpu,KVM_SET_MSRS,m.h) != 3) return false;
	}

	return true;
}

/* Bring the state from the vCPU into DOSBox-X: the registers and/or the rest (see above) */
static bool KVM_GetState(bool regs,bool rest) {
	CPU_FullState st;
	CPU_GetFullState(st);	/* keeps what is not fetched (and DR4/DR5) */

	if (regs) {
		struct kvm_regs r;
		struct kvm_sregs sr;
		if (!KVM_ReadRegs(r,sr)) return false;

		st.eax = (uint32_t)r.rax; st.ecx = (uint32_t)r.rcx; st.edx = (uint32_t)r.rdx; st.ebx = (uint32_t)r.rbx;
		st.esp = (uint32_t)r.rsp; st.ebp = (uint32_t)r.rbp; st.esi = (uint32_t)r.rsi; st.edi = (uint32_t)r.rdi;
		st.eip = (uint32_t)r.rip;
		st.eflags = (uint32_t)r.rflags;

		static const unsigned int order[6] = {es,cs,ss,ds,fs,gs};
		const struct kvm_segment *k[6] = {&sr.es,&sr.cs,&sr.ss,&sr.ds,&sr.fs,&sr.gs};
		for (unsigned int i=0;i < 6;i++)
			KVM_SegmentFromKVM(*k[i],st.seg[order[i]]);
		KVM_SegmentFromKVM(sr.tr,st.tr);
		KVM_SegmentFromKVM(sr.ldt,st.ldt);
		/* a null selector is no task register or LDT for DOSBox-X */
		if ((st.tr.selector & 0xFFFCu) == 0) st.tr.unusable = true;
		if ((st.ldt.selector & 0xFFFCu) == 0) st.ldt.unusable = true;
		st.gdt.base = (uint32_t)sr.gdt.base; st.gdt.limit = sr.gdt.limit;
		st.idt.base = (uint32_t)sr.idt.base; st.idt.limit = sr.idt.limit;
		st.cr0 = (uint32_t)sr.cr0; st.cr2 = (uint32_t)sr.cr2; st.cr3 = (uint32_t)sr.cr3; st.cr4 = (uint32_t)sr.cr4;
		if (!(st.cr0 & 1u)) st.cpl = 0;
		else if (st.eflags & FLAG_VM) st.cpl = 3;
		else st.cpl = sr.ss.dpl;
	}

	if (rest) {
		struct kvm_fpu f;
		struct kvm_debugregs d;
		if (ioctl(kvm.vcpu,KVM_GET_FPU,&f) < 0) return false;
		if (ioctl(kvm.vcpu,KVM_GET_DEBUGREGS,&d) < 0) return false;

		st.fcw = f.fcw; st.fsw = f.fsw; st.ftw = f.ftwx;
		for (unsigned int i=0;i < 8;i++) {
			memcpy(st.st[i],f.fpr[i],10);
			memcpy(st.xmm[i],f.xmm[i],16);
		}
		st.mxcsr = f.mxcsr;

		for (unsigned int i=0;i < 4;i++) st.dr[i] = (uint32_t)d.db[i];
		st.dr[6] = (uint32_t)d.dr6; st.dr[7] = (uint32_t)d.dr7;

		KVM_MSRs m;
		if (ioctl(kvm.vcpu,KVM_GET_MSRS,m.h) != 3) return false;
		st.sysenter_cs = (uint32_t)m.e[0].data;
		st.sysenter_esp = (uint32_t)m.e[1].data;
		st.sysenter_eip = (uint32_t)m.e[2].data;
	}

	CPU_SetFullState(st);

	/* The guest may have changed its page tables while it ran (and invalidated TLB entries or
	 * reloaded CR3 with the same value, which DOSBox-X does not see), so DOSBox-X's own TLB,
	 * which callbacks and devices use for linear addresses, may be out of date */
	if (regs && (st.cr0 & CR0_PAGING)) PAGING_ClearTLB();
	return true;
}

/* A one-shot timer ends KVM_RUN when the time slice is up: its signal sets immediate_exit,
 * which also covers it arriving just before KVM_RUN */
static struct {
	bool		created = false;
	timer_t		timer;
	uint64_t	deadline = 0;	/* KVM host time: when it fires, nanoseconds (PIC_HostTimeNow()), 0 = disarmed */
	volatile sig_atomic_t fired = 0;	/* it fired, maybe outside KVM_RUN (see KVM_Run()) */
} kvm_timer;

static void KVM_TimerSignal(int sig) {
	(void)sig;
	kvm_timer.fired = 1;
	if (kvm.run != NULL) kvm.run->immediate_exit = 1;
}

static bool KVM_TimerCreate(void) {
	struct sigaction sa;
	memset(&sa,0,sizeof(sa));
	sa.sa_handler = KVM_TimerSignal;
	sa.sa_flags = SA_RESTART; /* other system calls the signal interrupts carry on */
	sigemptyset(&sa.sa_mask);
	const int sig = SIGRTMIN + 4;
	if (sigaction(sig,&sa,NULL) < 0) return false;

	struct sigevent sev;
	memset(&sev,0,sizeof(sev));
	sev.sigev_notify = SIGEV_THREAD_ID;
	sev.sigev_signo = sig;
	sev.sigev_notify_thread_id = (pid_t)syscall(SYS_gettid); /* the thread that runs the vCPU */
	if (timer_create(CLOCK_MONOTONIC,&sev,&kvm_timer.timer) < 0) return false;
	kvm_timer.created = true;
	return true;
}

/* Arm the timer to fire 'ns' nanoseconds from now (KVM host time), unless it already fires
 * about then: the timer is armed once per slice, not for every KVM_RUN. It is moved earlier when
 * the slice gets shorter (an earlier PIC event), and later when it would fire more than a
 * quarter of the time early (at least 20 us): the exits during the slice are not charged to the
 * guest (KVM_Overhead()), so the slice takes longer than its budget, and a timer firing early
 * costs an exit of its own. */
static void KVM_TimerArm(uint64_t ns) {
	const uint64_t deadline = PIC_HostTimeNow() + ns; /* KVM host time: nanoseconds */
	if (kvm_timer.deadline != 0 && kvm_timer.deadline <= deadline &&
		deadline - kvm_timer.deadline <= std::max(ns / 4u,(uint64_t)20000u)) return;

	/* PIC_HostTimeNow() is CLOCK_MONOTONIC (steady_clock), so the deadline is absolute */
	struct itimerspec its;
	memset(&its,0,sizeof(its));
	its.it_value.tv_sec = (time_t)(deadline / 1000000000u);
	its.it_value.tv_nsec = (long)(deadline % 1000000000u);
	timer_settime(kvm_timer.timer,TIMER_ABSTIME,&its,NULL);
	kvm_timer.deadline = deadline;
}

static void KVM_TimerDisarm(void) {
	if (kvm_timer.deadline == 0) return;
	struct itimerspec its;
	memset(&its,0,sizeof(its));
	timer_settime(kvm_timer.timer,0,&its,NULL);
	kvm_timer.deadline = 0;
	kvm_timer.fired = 0;
	if (kvm.run != NULL) kvm.run->immediate_exit = 0;
}

static void KVM_FlushWrites(void);
static void KVM_HandleIO(void);
static void KVM_HandleMMIO(void);

/* Finish the instruction of the last I/O or MMIO exit without running anything else: KVM_RUN
 * with immediate_exit set (the method the KVM API documentation gives), until it returns EINTR.
 * A string instruction with a REP prefix exits again for each further element, which is
 * handled here too. Before the state leaves the vCPU or the core returns. */
static void KVM_CompletePending(void) {
	while (kvm.pending) {
		kvm.pending = false;
		kvm.run->immediate_exit = 1;
		const int r = ioctl(kvm.vcpu,KVM_RUN,0);
		kvm.run->immediate_exit = 0;
		KVM_FlushWrites();
		if (r == 0 && kvm.run->exit_reason == KVM_EXIT_MMIO) {
			kvm.mmio++;
			KVM_HandleMMIO();
			kvm.pending = true;
		}
		else if (r == 0 && kvm.run->exit_reason == KVM_EXIT_IO) {
			kvm.io++;
			KVM_HandleIO();
			kvm.pending = true;
		}
	}
}

/* CPU_SyncState() (full) and CPU_SyncRegisters(): bring the state from the vCPU into DOSBox-X,
 * whose copy counts from then on */
static void KVM_SyncHook(bool full) {
	const bool regs = kvm.in_kvm,rest = full && kvm.rest_in_kvm;
	if (!regs && !rest) return;
	KVM_CompletePending();

	kvm.in_kvm = kvm.in_kvm && !regs;
	kvm.rest_in_kvm = kvm.rest_in_kvm && !rest;
	if (!kvm.in_kvm) CPU_StateIFHook = NULL;
	if (!kvm.in_kvm && !kvm.rest_in_kvm) CPU_StateSyncHook = NULL;
	kvm.gets++;
	if (rest) kvm.event_in_flight = true;
	if (!KVM_GetState(regs,rest)) {
		LOG_MSG("KVM: cannot read the vCPU state: %s",strerror(errno));
		kvm.failed = true;
	}
}

/* The guest's interrupt flag while the registers are in the vCPU (see CPU_InterruptFlag()) */
static bool KVM_IFHook(void) {
	return kvm.guest_if;
}

/* Make the vCPU's copy of the state the one that counts */
static bool KVM_EnterState(void) {
	if (kvm.in_kvm && kvm.rest_in_kvm) return true;
	const bool regs = !kvm.in_kvm,rest = !kvm.rest_in_kvm;
	kvm.puts++;
	if (!KVM_PutState(regs,rest)) return false;
	if (regs) kvm.guest_if = (reg_flags & FLAG_IF) != 0;
	kvm.in_kvm = true;
	kvm.rest_in_kvm = true;
	CPU_StateSyncHook = KVM_SyncHook;
	CPU_StateIFHook = KVM_IFHook;
	return true;
}

/* Give up: log why, and run the normal core from now on */
static Bits KVM_Fail(const std::string &why) {
	LOG_MSG("KVM: %s. Switching to the normal core.",why.c_str());
	CPU_SyncState();
	if (!kvm.failed) {
		char code[3*16+1] = "";
		for (unsigned int i=0;i < 16;i++) {
			uint8_t b = 0;
			if (mem_readb_checked((LinearPt)(SegPhys(cs)+((reg_eip+i)&(cpu.code.big?0xFFFFFFFFu:0xFFFFu))),&b)) break;
			sprintf(code+i*3,"%02x ",b);
		}
		LOG_MSG("KVM: at %04x:%08x (linear %08x) CR0=%08x EFLAGS=%08x code %s",(unsigned int)SegValue(cs),
			(unsigned int)reg_eip,(unsigned int)(SegPhys(cs)+reg_eip),(unsigned int)cpu.cr0,(unsigned int)reg_flags,code);
	}
	KVM_ShadowStore(); /* the normal core uses VGA memory through the memory handlers */
	kvm_shadow.active = false;
	kvm.failed = true;
	kvm.in_kvm = false;
	kvm.rest_in_kvm = false;
	CPU_StateSyncHook = NULL;
	CPU_StateIFHook = NULL;
	cpudecoder = &CPU_Core_Normal_Run;
	void menu_update_core(void);
	menu_update_core(); /* the CPU core menu shows the normal core now */
	return CBRET_NONE;
}

/* How many writes are queued in the coalesced MMIO ring */
static uint32_t KVM_QueuedWrites(void) {
	const struct kvm_coalesced_mmio_ring *ring = kvm.ring;
	if (ring == NULL) return 0;
	return (ring->last + kvm.ring_max - ring->first) % kvm.ring_max;
}

/* Apply the writes KVM queued (coalesced MMIO) since the last time, in order. Called as soon as
 * KVM_RUN returns, before anything else: the guest's later accesses (reads exit) and anything
 * DOSBox-X does with the device then see them. */
/* Port I/O exits per port, reads [0] and writes [1], for the statistics logged at exit: the
 * ports a guest polls or writes most are where port I/O exits cost the most time */
static uint32_t kvm_port_exits[2][0x10000];

static void KVM_FlushWrites(void) {
	struct kvm_coalesced_mmio_ring *ring = kvm.ring;
	if (ring == NULL) return;

	while (ring->first != ring->last) {
		const struct kvm_coalesced_mmio &w = ring->coalesced_mmio[ring->first];
		const PhysPt64 a = (PhysPt64)w.phys_addr;
		if (w.pio) { /* a port write (coalesced PIO) */
			const Bitu port = (Bitu)w.phys_addr;
			switch (w.len) {
				case 1: IO_WriteB_Checked(port,w.data[0]); break;
				case 2: IO_WriteW_Checked(port,host_readw((HostPt)w.data)); break;
				case 4: IO_WriteD_Checked(port,host_readd((HostPt)w.data)); break;
			}
		}
		else switch (w.len) {
			case 1: physdev_writeb(a,w.data[0]); break;
			case 2: physdev_writew(a,host_readw((HostPt)w.data)); break;
			case 4: physdev_writed(a,host_readd((HostPt)w.data)); break;
			case 8: physdev_writed(a,host_readd((HostPt)w.data)); physdev_writed(a+4u,host_readd((HostPt)w.data+4)); break;
			default: for (uint32_t i=0;i < w.len && i < 8;i++) physdev_writeb(a+i,w.data[i]); break;
		}
		__sync_synchronize();
		ring->first = (ring->first + 1u) % kvm.ring_max;
		kvm.coalesced++;
	}
}

static void KVM_HandleIO(void) {
	struct kvm_run *run = kvm.run;
	uint8_t *data = (uint8_t*)run + run->io.data_offset;

	kvm_port_exits[run->io.direction == KVM_EXIT_IO_OUT ? 1 : 0][run->io.port]++;

	for (uint32_t n=0;n < run->io.count;n++,data += run->io.size) {
		if (run->io.direction == KVM_EXIT_IO_OUT) {
			switch (run->io.size) {
				case 1: IO_WriteB_Checked(run->io.port,data[0]); break;
				case 2: IO_WriteW_Checked(run->io.port,(uint16_t)(data[0] | (data[1] << 8u))); break;
				case 4: IO_WriteD_Checked(run->io.port,(uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
						((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u)); break;
			}
		}
		else {
			switch (run->io.size) {
				case 1: data[0] = IO_ReadB_Checked(run->io.port); break;
				case 2: { const uint16_t v = IO_ReadW_Checked(run->io.port); data[0] = (uint8_t)v; data[1] = (uint8_t)(v >> 8u); break; }
				case 4: { const uint32_t v = IO_ReadD_Checked(run->io.port);
					data[0] = (uint8_t)v; data[1] = (uint8_t)(v >> 8u); data[2] = (uint8_t)(v >> 16u); data[3] = (uint8_t)(v >> 24u); break; }
			}
		}
	}
}

static void KVM_HandleMMIO(void) {
	struct kvm_run *run = kvm.run;
	const PhysPt64 a = (PhysPt64)run->mmio.phys_addr;
	uint8_t *data = run->mmio.data;

	/* a page whose handler was not known when the slots were built: the access below finds
	 * it, and then it may turn out to be memory that can be mapped (a PCI LFB, for example) */
	const Bitu page = (Bitu)(a >> 12u);
	const bool deferred = page < 0x100000u && (kvm_mem.deferred[page >> 6u] & ((uint64_t)1u << (page & 63u)));

	if (run->mmio.is_write) {
		switch (run->mmio.len) {
			case 1: physdev_writeb(a,data[0]); break;
			case 2: physdev_writew(a,host_readw(data)); break;
			case 4: physdev_writed(a,host_readd(data)); break;
			case 8: physdev_writed(a,host_readd(data)); physdev_writed(a+4u,host_readd(data+4)); break;
			default: for (uint32_t i=0;i < run->mmio.len;i++) physdev_writeb(a+i,data[i]); break;
		}
	}
	else {
		switch (run->mmio.len) {
			case 1: data[0] = physdev_readb(a); break;
			case 2: host_writew(data,physdev_readw(a)); break;
			case 4: host_writed(data,physdev_readd(a)); break;
			case 8: host_writed(data,physdev_readd(a)); host_writed(data+4,physdev_readd(a+4u)); break;
			default: for (uint32_t i=0;i < run->mmio.len;i++) data[i] = physdev_readb(a+i); break;
		}
	}

	if (deferred && MEM_PageHandlerKnown(page)) {
		kvm_mem.deferred[page >> 6u] &= ~((uint64_t)1u << (page & 63u));
		KVM_MarkDirty(page,page);
	}
}

/* #BP: the INT3 prefix of a callback instruction, a debugger breakpoint, or the guest's own INT3 */
static Bits KVM_HandleBreakpoint(void) {
	CPU_SyncRegisters();
	if (kvm.failed) return KVM_Fail("cannot read the vCPU state");

	/* debug.arch.pc is the linear address of the INT3 */
	const LinearPt pc = (LinearPt)kvm.run->debug.arch.pc;
	const uint32_t ip_mask = cpu.code.big ? 0xFFFFFFFFu : 0xFFFFu;
	reg_eip = (uint32_t)(pc - SegPhys(cs)) & ip_mask;

#if C_DEBUG && !C_HEAVY_DEBUG
	/* a debugger breakpoint, also one on the INT3 prefix of a callback instruction (the
	 * emulated cores check it there too) */
	FillFlags();
	if (DEBUG_Breakpoint())
		return (Bits)debugCallback;
#endif

	if (CALLBACK_Int3PrefixFollows(pc + 1u)) {
		const Bitu cb = mem_readw(pc + 3u);
		reg_eip = (reg_eip + 5u) & ip_mask; /* past INT3 + FE 38 imm16 */
		kvm.callbacks++;
		return (Bits)cb;
	}

#if C_DEBUG
	FillFlags();
	if (DEBUG_IntBreakpoint(3))
		return (Bits)debugCallback;
#endif

	/* the guest's own INT3: interrupt 3, returning to the next instruction */
	kvm.int3++;
	CPU_SW_Interrupt_NoIOPLCheck(3,(reg_eip + 1u) & ip_mask);
	return CBRET_NONE;
}

static Bits KVM_Run(void);

/* Update a cost estimate with a sample, return the estimate (KVM host time: nanoseconds): the
 * average over about the last 64 samples, of those that took at most four times the shortest
 * seen if 'filter' is set (so that samples with more guest execution in them hardly count),
 * at most 20 us */
static uint64_t KVM_Estimate(decltype(kvm.write_cost) &c,uint64_t sample,bool filter) {
	if (c.min == 0 || sample < c.min) c.min = sample;
	if (!filter || sample <= c.min * 4u) c.avg = (c.avg == 0) ? sample : (c.avg - c.avg / 64u + sample / 64u);
	return std::min(c.avg,(uint64_t)20000u);
}

/* The cost of going into the guest and out again is host time the guest did not run. Charged
 * as guest time, it would make every exit cost thousands of cycles where an emulated core
 * counts one instruction, so an exit-heavy guest (VGA memory, port I/O) would crawl at low
 * cycle counts. The same goes for the writes queued in the coalesced MMIO ring, which KVM
 * handles itself within KVM_RUN: a write instruction is an exit of its own (a few us), while
 * a string instruction's writes are queued by KVM's instruction emulator in one go (well under
 * a microsecond each). Estimated for each kind of exit that comes back at once (port I/O,
 * MMIO, which KVM decodes and emulates and so costs more, a callback, HLT) from the KVM_RUNs
 * that ended with it and queued no writes, for a write from those that queued a few, and for
 * a write of a string instruction from those that queued at least 64, where the guest did
 * little besides writing. Returns how many of the 'elapsed' nanoseconds (KVM host time) of the
 * KVM_RUN were not guest execution. */
static uint64_t KVM_Overhead(int r,uint64_t elapsed) {
	uint64_t overhead = 0; /* KVM host time: nanoseconds */

	if (r >= 0) {
		int i;
		switch (kvm.run->exit_reason) {
			case KVM_EXIT_IO:	i = 0; break;
			case KVM_EXIT_MMIO:	i = 1; break;
			case KVM_EXIT_DEBUG:	i = 2; break;
			case KVM_EXIT_HLT:	i = 3; break;
			/* an MMIO access that KVM failed to emulate costs about what an MMIO exit does */
			case KVM_EXIT_INTERNAL_ERROR:
				i = (kvm.run->internal.suberror == KVM_INTERNAL_ERROR_EMULATION) ? 1 : -1;
				break;
			default:		i = -1; break;
		}
		if (i >= 0) {
			if (KVM_QueuedWrites() == 0) overhead = KVM_Estimate(kvm.exit_cost[i],elapsed,true);
			else overhead = std::min(kvm.exit_cost[i].avg,(uint64_t)20000u);
		}
	}
	kvm.last_exit_cost = overhead;

	const uint32_t writes = KVM_QueuedWrites();
	if (writes != 0) {
		auto &c = (writes >= 64) ? kvm.burst_cost : kvm.write_cost;
		if (elapsed > overhead) KVM_Estimate(c,(elapsed - overhead) / writes,writes < 64);
		overhead += (uint64_t)writes * std::min(c.avg,(uint64_t)20000u);
	}

	return overhead;
}

/* Run the instruction that KVM could not emulate on the normal core, which accesses MMIO
 * through the page handlers, then go on in KVM. Without KVM_CAP_EXIT_ON_EMULATION_FAILURE, KVM
 * has queued a #UD for the guest, which is taken back first. (Without the capability KVM exits
 * to DOSBox-X only at CPL 0; at CPL 3 the guest gets the #UD.) */
static Bits KVM_EmulateOne(void) {
	if (!kvm.exit_on_emulation_failure) {
		struct kvm_vcpu_events ev;
		if (ioctl(kvm.vcpu,KVM_GET_VCPU_EVENTS,&ev) < 0)
			return KVM_Fail(std::string("KVM_GET_VCPU_EVENTS: ") + strerror(errno));
		if (ev.exception.nr == EXCEPTION_UD && (ev.exception.injected || ev.exception.pending)) {
			ev.exception.injected = 0;
			ev.exception.pending = 0;
			if (ioctl(kvm.vcpu,KVM_SET_VCPU_EVENTS,&ev) < 0)
				return KVM_Fail(std::string("KVM_SET_VCPU_EVENTS: ") + strerror(errno));
		}
	}
	CPU_SyncState();
	if (kvm.failed) return KVM_Fail("cannot read the vCPU state");
	kvm.emulated++;

	/* one instruction, and the single step trap after it if TF is set. The normal core may
	 * change cpudecoder (the trap decoder after a POPF that sets TF), which KVM does itself. */
	CPU_Decoder * const old_decoder = cpudecoder;
	const Bits old_cycles = CPU_Cycles;
	Bits ret;
	try {
		if (reg_flags & FLAG_TF) {
			ret = CPU_Core_Normal_Trap_Run();
		}
		else {
			CPU_Cycles = 1;
			ret = CPU_Core_Normal_Run();
			CPU_Cycles = old_cycles - 1;
		}
	}
	catch (...) {
		/* a guest page fault, handled by the main loop */
		CPU_Cycles = old_cycles - 1;
		if (cpudecoder == &CPU_Core_Normal_Run || cpudecoder == &CPU_Core_Normal_Trap_Run)
			cpudecoder = old_decoder;
		throw;
	}
	if (cpudecoder == &CPU_Core_Normal_Run || cpudecoder == &CPU_Core_Normal_Trap_Run)
		cpudecoder = old_decoder;
	return ret;
}

Bits CPU_Core_KVM_Run(void) {
	Bits r = KVM_Run();
	KVM_ShadowStore(); /* what the guest wrote to mode 13h memory, for DOSBox-X */
	if (kvm.emulate_one) {
		kvm.emulate_one = false;
		if (r == CBRET_NONE) r = KVM_EmulateOne();
	}
	/* The slice timer stays armed: the core is mostly called again within microseconds (a
	 * callback, an interrupt), usually with about the same deadline, and arming and disarming
	 * it are two system calls per call. If it fires in between, the next KVM_RUN returns at
	 * once (see KVM_Run()). */
	/* An instruction of an I/O or MMIO exit may still be unfinished: the next KVM_RUN finishes
	 * it, and whatever needs the state before that syncs it, which finishes it first. */
	return r;
}

static Bits KVM_Run(void) {
#if C_DEBUG
	/* Stopped in the debugger, or the debugger needs to see every instruction (heavy debugging
	 * breakpoints, interrupt breakpoints, CPU logging): use the emulated core. This can change
	 * at any time, since the state comes back into DOSBox-X first. */
	/* Also when the debugger steps over one instruction (RUN, F5, F10 step past a
	 * breakpoint with inhibit_int_breakpoint set): KVM cannot run just one. */
	bool IsDebuggerActive(void), IsDebuggerRunwatch(void);
	extern bool inhibit_int_breakpoint;
	if ((IsDebuggerActive() && !IsDebuggerRunwatch()) || inhibit_int_breakpoint || DEBUG_NeedsEmulatedCore()) {
		CPU_SyncState();
		return CPU_Core_Normal_Run();
	}
#endif
	if (kvm.failed) {
		CPU_SyncState();
		cpudecoder = &CPU_Core_Normal_Run;
		return CPU_Core_Normal_Run();
	}
	if (CPU_Cycles <= 0) return CBRET_NONE;

	if (!kvm_timer.created && !KVM_TimerCreate())
		return KVM_Fail(std::string("cannot create the time slice timer: ") + strerror(errno));

	struct kvm_run *run = kvm.run;

	for (;;) {
		/* the state stays in the vCPU from slice to slice; whatever needs it in DOSBox-X
		 * (interrupt delivery, callbacks, the debugger, save states) calls CPU_SyncState(),
		 * also from the exit handlers below, and then it is put back here */
		if (!KVM_EnterState())
			return KVM_Fail(std::string("cannot set the vCPU state: ") + strerror(errno));
		if (!KVM_ApplyMapChanges())
			return KVM_Fail("cannot set up the guest memory");
		/* DOSBox-X wrote to video memory through the memory handlers: load the shadow again */
		if (kvm_shadow.active && kvm_shadow.writes != VGA_MemWriteCount) {
			KVM_ShadowStore();
			KVM_ShadowLoad();
		}

		/* an IRQ the guest cannot take yet: come back as soon as it can (DOSBox-X delivers it) */
		run->request_interrupt_window = (PIC_IRQCheck && !kvm.guest_if) ? 1 : 0;

		/* KVM host time: nanoseconds, at least 2 us so that the guest gets somewhere even when
		 * the slice is shorter than going into and out of the guest takes */
		uint64_t budget = PIC_HostTimeBudget();
		if (budget == 0) break;
		if (budget < 2000) budget = 2000;

		/* the timer fired outside KVM_RUN (between core calls, or while KVM_CompletePending()
		 * used immediate_exit itself): it is no longer armed */
		if (kvm_timer.fired) {
			kvm_timer.fired = 0;
			kvm_timer.deadline = 0;
			run->immediate_exit = 0;
		}
		KVM_TimerArm(budget);
		const uint64_t mark = PIC_HostTimeMark(); /* KVM host time: nanoseconds */
		const int r = ioctl(kvm.vcpu,KVM_RUN,0);
		const int err = errno;
		kvm.runs++;
		kvm.pending = false;
		kvm.event_in_flight = r < 0 || run->exit_reason == KVM_EXIT_INTR;
		/* KVM host time: nanoseconds since PIC_HostTimeMark(), less the cost of the exit */
		PIC_HostTimeAccount(KVM_Overhead(r,PIC_HostTimeNow() - mark));
		KVM_FlushWrites();
		kvm.guest_if = run->if_flag != 0;

		if (r < 0) {
			if (err == EINTR || err == EAGAIN) {
				/* the timer (or another signal): the timer has fired, arm it again if the
				 * slice is not over */
				if (run->immediate_exit || kvm_timer.fired) {
					run->immediate_exit = 0;
					kvm_timer.deadline = 0;
					kvm_timer.fired = 0;
				}
				if (CPU_Cycles <= 0) break;
				continue;
			}
			KVM_TimerDisarm();
			return KVM_Fail(std::string("KVM_RUN: ") + strerror(err));
		}

		switch (run->exit_reason) {
			case KVM_EXIT_IO:
				kvm.io++;
				kvm.pending = true;
				KVM_HandleIO();
				break;
			case KVM_EXIT_MMIO:
				kvm.mmio++;
				kvm.pending = true;
				KVM_HandleMMIO();
				break;
			case KVM_EXIT_INTR:
				break;
			case KVM_EXIT_IRQ_WINDOW_OPEN:
				/* the guest can take the IRQ now: return so that DOSBox-X delivers it */
				kvm.windows++;
				return CBRET_NONE;
			case KVM_EXIT_HLT:
				/* the emulated HLT waits for the interrupt (see HLT_Decode) and comes back here */
				CPU_SyncRegisters();
				if (kvm.failed) return KVM_Fail("cannot read the vCPU state");
				kvm.hlt++;
				CPU_HLT(reg_eip);
				return CBRET_NONE;
			case KVM_EXIT_DEBUG:
				if (run->debug.arch.exception == 3)
					return KVM_HandleBreakpoint();
				return KVM_Fail("unexpected debug exit, exception " + std::to_string(run->debug.arch.exception));
			case KVM_EXIT_SHUTDOWN:
				/* triple fault */
				CPU_SyncState();
				LOG_MSG("KVM: triple fault at %04x:%08x",(unsigned int)SegValue(cs),(unsigned int)reg_eip);
				if (cpu_triple_fault_reset) {
					On_Software_CPU_Reset();
					E_Exit("Triple fault reset call unexpectedly returned");
				}
				E_Exit("KVM: triple fault");
				break;
			case KVM_EXIT_FAIL_ENTRY:
				return KVM_Fail("the vCPU state was refused (KVM_EXIT_FAIL_ENTRY, reason 0x" +
					std::to_string((unsigned long long)run->fail_entry.hardware_entry_failure_reason) + ")");
			case KVM_EXIT_INTERNAL_ERROR: {
				/* An instruction that KVM's instruction emulator cannot do on MMIO, an x87 store
				 * for example (Glide writes floats to the Voodoo registers with FSTP): the normal
				 * core runs it (see CPU_Core_KVM_Run()) */
				if (run->internal.suberror == KVM_INTERNAL_ERROR_EMULATION) {
					kvm.emulate_one = true;
					return CBRET_NONE;
				}
				std::string why = "KVM internal error, suberror " + std::to_string(run->internal.suberror);
				for (uint32_t i=0;i < run->internal.ndata && i < 16;i++) {
					char tmp[32];
					sprintf(tmp," %llx",(unsigned long long)run->internal.data[i]);
					why += tmp;
				}
				/* the event being delivered, if KVM reports it */
				struct kvm_vcpu_events ev;
				if (ioctl(kvm.vcpu,KVM_GET_VCPU_EVENTS,&ev) >= 0) {
					char tmp[160];
					sprintf(tmp,"; events: exception %u/%u nr %u, interrupt %u nr %u soft %u, nmi %u/%u",
						ev.exception.injected,ev.exception.pending,ev.exception.nr,
						ev.interrupt.injected,ev.interrupt.nr,ev.interrupt.soft,ev.nmi.injected,ev.nmi.pending);
					why += tmp;
				}
				if (kvm.sync_regs) {
					char tmp[96];
					sprintf(tmp,"; IDT base %llx limit %x",(unsigned long long)run->s.regs.sregs.idt.base,(unsigned int)run->s.regs.sregs.idt.limit);
					why += tmp;
				}
				return KVM_Fail(why);
			}
			default:
				return KVM_Fail("unhandled exit reason " + std::to_string(run->exit_reason));
		}

		if (kvm.failed) return KVM_Fail("cannot read the vCPU state");
		if (CPU_Cycles <= 0) break;

		/* Less of the slice left than this exit cost: another KVM_RUN would most likely be cut
		 * short by the slice timer, which costs an exit of its own, so end the slice here (the
		 * cycles left over stay for the next one). KVM host time: nanoseconds. */
		if (PIC_HostTimeBudget() < kvm.last_exit_cost) break;
	}

	kvm.slices++;
	return CBRET_NONE;
}

static std::string KVM_Error(const char *what) {
	return std::string(what) + ": " + strerror(errno);
}

static void KVM_CoreShutdownSection(Section *sec) {
	(void)sec;
	KVM_CoreShutdown();
}

void KVM_CoreShutdown(void) {
	if (kvm.runs != 0)
		LOG_MSG("KVM: %llu KVM_RUN calls: %llu port I/O, %llu MMIO (and %llu queued writes), %llu callbacks, %llu guest INT3, %llu HLT, %llu interrupt windows, %llu slices ended, %llu instructions emulated; state put %llu times, fetched %llu times; exit cost %llu/%llu/%llu/%llu/%llu/%llu ns (I/O, MMIO, callback, HLT, queued write, string write)",
			(unsigned long long)kvm.runs,(unsigned long long)kvm.io,(unsigned long long)kvm.mmio,(unsigned long long)kvm.coalesced,(unsigned long long)kvm.callbacks,
			(unsigned long long)kvm.int3,(unsigned long long)kvm.hlt,(unsigned long long)kvm.windows,(unsigned long long)kvm.slices,
			(unsigned long long)kvm.emulated,(unsigned long long)kvm.puts,(unsigned long long)kvm.gets,
			(unsigned long long)kvm.exit_cost[0].avg,(unsigned long long)kvm.exit_cost[1].avg,(unsigned long long)kvm.exit_cost[2].avg,
			(unsigned long long)kvm.exit_cost[3].avg,(unsigned long long)kvm.write_cost.avg,(unsigned long long)kvm.burst_cost.avg);
	if (kvm.io != 0) {
		/* the busiest ports */
		std::string busy;
		for (unsigned int n=0;n < 8;n++) {
			unsigned int best_dir = 0,best_port = 0;
			uint32_t best = 0;
			for (unsigned int dir=0;dir < 2;dir++) {
				for (unsigned int port=0;port < 0x10000u;port++) {
					if (kvm_port_exits[dir][port] > best) {
						best = kvm_port_exits[dir][port];
						best_dir = dir;
						best_port = port;
					}
				}
			}
			if (best == 0) break;
			char tmp[48];
			sprintf(tmp,"%s%s %03xh %lu",busy.empty() ? "" : ", ",best_dir ? "out" : "in",best_port,(unsigned long)best);
			busy += tmp;
			kvm_port_exits[best_dir][best_port] = 0;
		}
		LOG_MSG("KVM: port I/O exits: %s",busy.c_str());
	}
	if (kvm.run != NULL) {
		munmap(kvm.run,kvm.run_size);
		kvm.run = NULL;
	}
	if (kvm.vcpu >= 0) { close(kvm.vcpu); kvm.vcpu = -1; }
	if (kvm.vm >= 0) { close(kvm.vm); kvm.vm = -1; }
	if (kvm.dev >= 0) { close(kvm.dev); kvm.dev = -1; }
}

/* Whether the host can run this core: /dev/kvm, the API version, the capabilities the core
 * needs, and real mode support */
static bool KVM_CheckHost(std::string &reason) {
	kvm.dev = open("/dev/kvm",O_RDWR|O_CLOEXEC);
	if (kvm.dev < 0) {
		if (errno == ENOENT)
			reason = "this host has no /dev/kvm (KVM is not enabled, or the CPU has no hardware virtualization)";
		else if (errno == EACCES)
			reason = "no permission to use /dev/kvm (the user is usually given access through the kvm group)";
		else
			reason = KVM_Error("cannot open /dev/kvm");
		return false;
	}

	const int api = ioctl(kvm.dev,KVM_GET_API_VERSION,0);
	if (api != KVM_API_VERSION) {
		reason = "the host KVM API version is " + std::to_string(api) + ", not " + std::to_string(KVM_API_VERSION);
		return false;
	}

	static const struct {
		int		cap;
		const char*	name;
	} needed[] = {
		{ KVM_CAP_USER_MEMORY,		"memory slots in user memory (KVM_CAP_USER_MEMORY)" },
		{ KVM_CAP_READONLY_MEM,		"read-only memory slots (KVM_CAP_READONLY_MEM)" },
		{ KVM_CAP_SET_GUEST_DEBUG,	"guest debugging (KVM_CAP_SET_GUEST_DEBUG), for callback instructions" },
		{ KVM_CAP_EXT_CPUID,		"setting the CPUID (KVM_CAP_EXT_CPUID)" },
		{ KVM_CAP_SET_TSS_ADDR,		"KVM_CAP_SET_TSS_ADDR" },
	};
	for (const auto &n : needed) {
		if (ioctl(kvm.dev,KVM_CHECK_EXTENSION,n.cap) <= 0) {
			reason = std::string("the host KVM does not support ") + n.name;
			return false;
		}
	}

	/* Intel CPUs run real mode and big real mode guests only with "unrestricted guest" (AMD
	 * CPUs always can). Without it, KVM can only emulate such code, slowly and incompletely. */
	FILE *fp = fopen("/sys/module/kvm_intel/parameters/unrestricted_guest","r");
	if (fp != NULL) {
		const int c = fgetc(fp);
		fclose(fp);
		if (c == 'N' || c == 'n' || c == '0') {
			reason = "the host KVM runs without unrestricted guest support (kvm_intel unrestricted_guest=N), which real mode code needs";
			return false;
		}
	}

	kvm.memslots = ioctl(kvm.dev,KVM_CHECK_EXTENSION,KVM_CAP_NR_MEMSLOTS);
	if (kvm.memslots < 32) {
		reason = "the host KVM allows only " + std::to_string(kvm.memslots) + " memory slots";
		return false;
	}

	return true;
}

static uint32_t KVM_FindSupported(const struct kvm_cpuid2 *sup,uint32_t function,int reg) {
	for (uint32_t i=0;i < sup->nent;i++) {
		const struct kvm_cpuid_entry2 &e = sup->entries[i];
		if (e.function == function && e.index == 0)
			return reg == 2 ? e.ecx : e.edx;
	}
	return 0;
}

/* Give the vCPU the CPUID of the emulated CPU type (DOSBox-X's own CPUID instruction), without
 * the features the host cannot provide */
static bool KVM_SetCPUID(std::string &reason) {
	std::vector<uint8_t> supbuf;
	struct kvm_cpuid2 *sup = NULL;
	for (uint32_t nent=64;;nent*=2) {
		supbuf.assign(sizeof(struct kvm_cpuid2) + nent*sizeof(struct kvm_cpuid_entry2),0);
		sup = (struct kvm_cpuid2*)supbuf.data();
		sup->nent = nent;
		if (ioctl(kvm.dev,KVM_GET_SUPPORTED_CPUID,sup) == 0) break;
		if (errno != E2BIG || nent >= 4096) {
			reason = KVM_Error("KVM_GET_SUPPORTED_CPUID");
			return false;
		}
	}

	std::vector<struct kvm_cpuid_entry2> entries;
	const uint32_t save_eax = reg_eax,save_ebx = reg_ebx,save_ecx = reg_ecx,save_edx = reg_edx;

	reg_eax = 0; reg_ecx = 0;
	if (CPU_CPUID()) {
		const uint32_t max_leaf = reg_eax;
		for (uint32_t leaf=0;leaf <= max_leaf && leaf < 32;leaf++) {
			reg_eax = leaf; reg_ebx = 0; reg_ecx = 0; reg_edx = 0;
			CPU_CPUID();

			struct kvm_cpuid_entry2 e;
			memset(&e,0,sizeof(e));
			e.function = leaf;
			e.eax = reg_eax; e.ebx = reg_ebx; e.ecx = reg_ecx; e.edx = reg_edx;
			if (leaf == 1) {
				const uint32_t sup_edx = KVM_FindSupported(sup,1,3),sup_ecx = KVM_FindSupported(sup,1,2);
				if ((e.edx & ~sup_edx) != 0 || (e.ecx & ~sup_ecx) != 0)
					LOG_MSG("KVM: the host cannot provide CPUID features EDX=%08x ECX=%08x of the emulated CPU, the guest will not see them",
						(unsigned int)(e.edx & ~sup_edx),(unsigned int)(e.ecx & ~sup_ecx));
				e.edx &= sup_edx;
				e.ecx &= sup_ecx;
			}
			entries.push_back(e);
		}
	}
	else {
		/* An emulated CPU without CPUID: the instruction still works under KVM (and the
		 * EFLAGS ID bit can be toggled), but it reports nothing */
		LOG_MSG("KVM: the emulated CPU type has no CPUID instruction, but under KVM CPUID still runs and returns zeros");
	}

	reg_eax = save_eax; reg_ebx = save_ebx; reg_ecx = save_ecx; reg_edx = save_edx;

	std::vector<uint8_t> setbuf(sizeof(struct kvm_cpuid2) + entries.size()*sizeof(struct kvm_cpuid_entry2),0);
	struct kvm_cpuid2 *set = (struct kvm_cpuid2*)setbuf.data();
	set->nent = (uint32_t)entries.size();
	for (size_t i=0;i < entries.size();i++) set->entries[i] = entries[i];
	if (ioctl(kvm.vcpu,KVM_SET_CPUID2,set) < 0) {
		reason = KVM_Error("KVM_SET_CPUID2");
		return false;
	}

	return true;
}

bool KVM_CoreInit(std::string &reason) {
	/* already created: only the CPU settings may have changed */
	if (kvm.vcpu >= 0)
		return KVM_SetCPUID(reason);

	if (!KVM_CheckHost(reason)) {
		KVM_CoreShutdown();
		return false;
	}

	kvm.vm = ioctl(kvm.dev,KVM_CREATE_VM,0);
	if (kvm.vm < 0) {
		reason = KVM_Error("KVM_CREATE_VM");
		KVM_CoreShutdown();
		return false;
	}
	if (ioctl(kvm.vm,KVM_SET_TSS_ADDR,kvm_tss_addr) < 0) {
		reason = KVM_Error("KVM_SET_TSS_ADDR");
		KVM_CoreShutdown();
		return false;
	}

	/* No KVM_CREATE_IRQCHIP: DOSBox-X emulates the PIC and PIT itself */

	kvm.vcpu = ioctl(kvm.vm,KVM_CREATE_VCPU,0);
	if (kvm.vcpu < 0) {
		reason = KVM_Error("KVM_CREATE_VCPU");
		KVM_CoreShutdown();
		return false;
	}

	const int run_size = ioctl(kvm.dev,KVM_GET_VCPU_MMAP_SIZE,0);
	if (run_size < (int)sizeof(struct kvm_run)) {
		reason = KVM_Error("KVM_GET_VCPU_MMAP_SIZE");
		KVM_CoreShutdown();
		return false;
	}
	kvm.run_size = (size_t)run_size;
	void *run = mmap(NULL,kvm.run_size,PROT_READ|PROT_WRITE,MAP_SHARED,kvm.vcpu,0);
	if (run == MAP_FAILED) {
		reason = KVM_Error("mmap of the vCPU run area");
		KVM_CoreShutdown();
		return false;
	}
	kvm.run = (struct kvm_run*)run;

	if (!KVM_SetCPUID(reason)) {
		KVM_CoreShutdown();
		return false;
	}

	/* the registers through kvm_run if the host can (KVM_CAP_SYNC_REGS), starting from the
	 * vCPU's current ones */
	const int sync = ioctl(kvm.dev,KVM_CHECK_EXTENSION,KVM_CAP_SYNC_REGS);
	if (sync > 0 && (sync & (KVM_SYNC_X86_REGS|KVM_SYNC_X86_SREGS)) == (KVM_SYNC_X86_REGS|KVM_SYNC_X86_SREGS) &&
		ioctl(kvm.vcpu,KVM_GET_REGS,&kvm.run->s.regs.regs) >= 0 && ioctl(kvm.vcpu,KVM_GET_SREGS,&kvm.run->s.regs.sregs) >= 0) {
		kvm.run->kvm_valid_regs = KVM_SYNC_X86_REGS | KVM_SYNC_X86_SREGS;
		kvm.sync_regs = true;
		/* not the vCPU events (KVM_SYNC_X86_EVENTS): KVM would fetch them at every exit, and
		 * they are needed only after a KVM_RUN that a signal ended (KVM_DropEvent()) */
	}

	/* Writes to the VGA memory window (A0000-BFFFF) are queued instead of exiting one by one.
	 * Reads still exit, after the queued writes are applied, so the order is kept. On PC-98 the
	 * same goes for the fourth graphics plane (E0000-E7FFF). */
	const int ring_page = ioctl(kvm.dev,KVM_CHECK_EXTENSION,KVM_CAP_COALESCED_MMIO);
	kvm.pio_queue = false;
	kvm.vga_data_queued = false;
	if (ring_page > 0) {
		struct kvm_coalesced_mmio_zone zone;
		memset(&zone,0,sizeof(zone));
		zone.addr = 0xA0000;
		zone.size = 0x20000;
		if (ioctl(kvm.vm,KVM_REGISTER_COALESCED_MMIO,&zone) >= 0) {
			const long page_size = sysconf(_SC_PAGESIZE);
			kvm.ring = (struct kvm_coalesced_mmio_ring*)((uint8_t*)kvm.run + (size_t)ring_page * (size_t)page_size);
			kvm.ring_max = (uint32_t)(((size_t)page_size - sizeof(struct kvm_coalesced_mmio_ring)) / sizeof(struct kvm_coalesced_mmio));
			if (IS_PC98_ARCH) {
				zone.addr = 0xE0000;
				zone.size = 0x8000;
				ioctl(kvm.vm,KVM_REGISTER_COALESCED_MMIO,&zone);
			}

			/* Writes to ports with no effect that the guest could see before its next exit are
			 * queued too (coalesced PIO), in the same ring and so in order with the video memory
			 * writes; reads still exit, after the queued writes are applied. The I/O delay ports
			 * 80h and EDh; the VGA DAC (3C6h-3C9h, palette loads); the PIT command port 43h
			 * (a counter latch takes effect at the read that follows); the DMA flip-flop resets
			 * (0Ch, D8h) that come before reading a DMA address or count; and the VGA index
			 * registers (3B4h, 3C4h, 3CEh, 3D4h), which only select the register that the data
			 * port that follows reads or writes. Not the VGA data registers: they can change the
			 * memory map (an SVGA bank, the memory window), which has to take effect before the
			 * guest's next access to directly mapped video memory (the sequencer and graphics
			 * controller ones are queued while there is none). The IDE data port and the
			 * taskfile registers after it (1F0h-1F3h, 170h-173h): writing a sector is one exit
			 * per word otherwise, since KVM emulates REP OUTSW one element at a time (REP INSW
			 * it does in batches); a write takes effect at the command register (1F7h, 177h),
			 * or the guest reads the status, and both exit. Only the IRQ at the end of a sector
			 * comes at the next exit instead, at most a time slice later. Ports polled most, by kind
			 * of program: PIT channel 0 reads for timing (games, Linux), the MPU-401 status
			 * port, DMA address/count reads (sound), the PIC, the RTC (see the log at exit). */
			if (!IS_PC98_ARCH && ioctl(kvm.dev,KVM_CHECK_EXTENSION,KVM_CAP_COALESCED_PIO) > 0) {
				static const struct { uint16_t port,count; } ports[] = {
					{0x80,1},{0xED,1},{0x3C6,4},{0x43,1},{0x0C,1},{0xD8,1},
					{0x3B4,1},{0x3C4,1},{0x3CE,1},{0x3D4,1},{0x1F0,4},{0x170,4}};
				for (const auto &pr : ports) {
					memset(&zone,0,sizeof(zone));
					zone.addr = pr.port;
					zone.size = pr.count;
					zone.pio = 1;
					ioctl(kvm.vm,KVM_REGISTER_COALESCED_MMIO,&zone);
				}
				/* and the sequencer and graphics controller data ports while no video memory
				 * is mapped directly (see KVM_QueueVGAData()) */
				kvm.pio_queue = true;
				kvm.vga_data_queued = false;
			}
		}
	}

	/* An instruction that KVM cannot emulate (on MMIO) exits to DOSBox-X at any CPL, without a
	 * #UD for the guest (see KVM_EmulateOne()) */
	kvm.exit_on_emulation_failure = false;
#ifdef KVM_CAP_EXIT_ON_EMULATION_FAILURE
	if (ioctl(kvm.dev,KVM_CHECK_EXTENSION,KVM_CAP_EXIT_ON_EMULATION_FAILURE) > 0) {
		struct kvm_enable_cap cap;
		memset(&cap,0,sizeof(cap));
		cap.cap = KVM_CAP_EXIT_ON_EMULATION_FAILURE;
		cap.args[0] = 1;
		kvm.exit_on_emulation_failure = ioctl(kvm.vm,KVM_ENABLE_CAP,&cap) >= 0;
	}
#endif
	if (!kvm.exit_on_emulation_failure)
		LOG_MSG("KVM: the host KVM cannot report instructions it fails to emulate at CPL 3 (KVM_CAP_EXIT_ON_EMULATION_FAILURE): a guest program that uses x87 instructions on device memory gets an invalid opcode exception");

	/* INT3 exits to DOSBox-X (callback instructions, see callback.h) */
	struct kvm_guest_debug dbg;
	memset(&dbg,0,sizeof(dbg));
	dbg.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_SW_BP;
	if (ioctl(kvm.vcpu,KVM_SET_GUEST_DEBUG,&dbg) < 0) {
		reason = KVM_Error("KVM_SET_GUEST_DEBUG");
		KVM_CoreShutdown();
		return false;
	}

	/* guest physical memory, built at the first KVM_RUN (the memory system is set up after
	 * the CPU) */
	kvm_mem.dirty = true;
	kvm_mem.dirty_lo = 0;
	kvm_mem.dirty_hi = ~(Bitu)0;
	MEM_AddMapChangeHandler(KVM_MapChange);

	if (!kvm.exit_registered) {
		kvm.exit_registered = true;
		AddExitFunction(AddExitFunctionFuncPair(KVM_CoreShutdownSection));
	}

	LOG_MSG("KVM: virtual machine and vCPU created (API %d, %d memory slots, immediate exit %s, registers through %s, %s video memory writes)",
		KVM_API_VERSION,kvm.memslots,ioctl(kvm.dev,KVM_CHECK_EXTENSION,KVM_CAP_IMMEDIATE_EXIT) > 0 ? "supported" : "not supported",
		kvm.sync_regs ? "kvm_run" : "ioctls",kvm.ring != NULL ? "queued" : "exiting");
	return true;
}

#endif /* C_KVM */
