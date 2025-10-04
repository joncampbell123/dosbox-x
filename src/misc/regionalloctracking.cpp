
#include <assert.h>
#include "dosbox.h"
#include "logging.h"
#include "mem.h"
#include "cpu.h"
#include "bios.h"
#include "regs.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "pic.h"
#include "hardware.h"
#include "pci_bus.h"
#include "joystick.h"
#include "mouse.h"
#include "callback.h"
#include "setup.h"
#include "serialport.h"
#include "mapper.h"
#include "vga.h"
#include "regionalloctracking.h"
#include "parport.h"
#include <time.h>
#if !defined(ANDROID) && !defined(__ANDROID__) && !defined(__OpenBSD__)
/* Newer NDKs doesn't have this header.
   It doesn't matter anyway */
#include <sys/timeb.h>
#endif

/* Really, Microsoft, Really?? You're the only compiler I know that doesn't understand ssize_t! */
#if defined(_MSC_VER)
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

RegionAllocTracking::Block::Block() : start(0), end(0), free(true), fixed(false) {
}

RegionAllocTracking::RegionAllocTracking() : _min(0), _max(~((Bitu)0)), _max_nonfixed(~((Bitu)0)), topDownAlloc(false) {
}

void RegionAllocTracking::setMaxDynamicAllocationAddress(Bitu _new_max) {
	if (_new_max != 0) /* nonzero to set */
		_max_nonfixed = _new_max;
	else /* zero to clear */
		_max_nonfixed = ~((Bitu)0);
}

Bitu RegionAllocTracking::getMemory(Bitu bytes,const char *who,Bitu alignment,Bitu must_be_at) {
	if (bytes == 0u) return alloc_failed;
	if (alignment > 1u && must_be_at != 0u) return alloc_failed; /* avoid nonsense! */
	if (who == NULL) who = "";
	if (alist.empty()) E_Exit("getMemory called when '%s' allocation list not initialized",name.c_str());

	/* alignment must be power of 2 */
	if (alignment == 0u)
		alignment = 1u;
	else if ((alignment & (alignment-1u)) != 0u)
		E_Exit("getMemory called with non-power of 2 alignment value %u on '%s'",(int)alignment,name.c_str());

	{
		/* allocate downward from the top */
		size_t si = topDownAlloc ? (alist.size() - 1u) : 0u;
		while ((ssize_t)si >= 0) {
			Block &blk = alist[si];
			Bitu base;

			if (!blk.free || (blk.end+1u-blk.start) < bytes) {
				if (topDownAlloc) si--;
				else si++;
				continue;
			}

			/* if must_be_at != 0 the caller wants a block at a very specific location */
			if (must_be_at != 0) {
				base = must_be_at;
				assert(alignment == 1u);
			}
			else {
				if (topDownAlloc) {
					base = blk.end + 1u - bytes; /* allocate downward from the top */
					assert(base >= blk.start);

					if (_max_nonfixed < _max) {
						/* if instructed to by caller, disallow any dynamic allocation above a certain memory limit */
						if ((_max_nonfixed + 1u) >= bytes) {
							const Bitu nbase = _max_nonfixed + 1u - bytes;
							if (base > nbase) base = nbase;
						}
						else {
							base = 0;
						}
					}
				}
				else {
					base = blk.start; /* allocate upward from the bottom */
					assert(base <= blk.end);
					base += alignment - 1u; /* alignment round up */

					/* TODO: max_nonfixed... */
				}
			}

			base &= ~(alignment - 1u); /* NTS: alignment == 16 means ~0xF or 0xFFFF0 */
			if (base < blk.start || (base+bytes-1u) > blk.end) {
				if (topDownAlloc) si--;
				else si++;
				continue;
			}

			if (base == blk.start && (base+bytes-1u) == blk.end) { /* easy case: perfect match */
				blk.fixed = (must_be_at != 0u);
				blk.free = false;
				blk.who = who;
			}
			else if (base == blk.start) { /* need to split */
				Block newblk = blk; /* this becomes the new block we insert */
				blk.start = base+bytes;
				newblk.end = base+bytes-1u;
				newblk.fixed = (must_be_at != 0u);
				newblk.free = false;
				newblk.who = who;
				alist.insert(alist.begin()+(std::vector<RegionAllocTracking::Block>::difference_type)si,newblk);
			}
			else if ((base+bytes-1) == blk.end) { /* need to split */
				Block newblk = blk; /* this becomes the new block we insert */
				blk.end = base-1;
				newblk.start = base;
				newblk.fixed = (must_be_at != 0u);
				newblk.free = false;
				newblk.who = who;
				alist.insert(alist.begin()+(std::vector<RegionAllocTracking::Block>::difference_type)si+1u,newblk);
			}
			else { /* complex split */
				Block newblk = blk,newblk2 = blk; /* this becomes the new block we insert */
				Bitu orig_end = blk.end;
				blk.end = base-1u;
				newblk.start = base+bytes;
				newblk.end = orig_end;
				alist.insert(alist.begin()+(std::vector<RegionAllocTracking::Block>::difference_type)si+1u,newblk);
				newblk2.start = base;
				newblk2.end = base+bytes-1u;
				newblk2.fixed = (must_be_at != 0u);
				newblk2.free = false;
				newblk2.who = who;
				alist.insert(alist.begin()+(std::vector<RegionAllocTracking::Block>::difference_type)si+1u,newblk2);
			}

			LOG(LOG_BIOS,LOG_DEBUG)("getMemory in '%s' (0x%05x bytes,\"%s\",align=%u,mustbe=0x%05x) = 0x%05x",name.c_str(),(int)bytes,who,(int)alignment,(int)must_be_at,(int)base);
			sanityCheck();
			return base;
		}
	}

	LOG(LOG_BIOS,LOG_DEBUG)("getMemory in '%s' (0x%05x bytes,\"%s\",align=%u,mustbe=0x%05x) = FAILED",name.c_str(),(int)bytes,who,(int)alignment,(int)must_be_at);
	sanityCheck();
	return alloc_failed;
}

Bitu RegionAllocTracking::getMinAddress() {
	size_t si = 0;
	Bitu r = _max;

	while (si < alist.size()) {
		Block &blk = alist[si];
		if (blk.free) {
			si++;
			continue;
		}

		r = blk.start;
		break;
	}

	return r;
}

void RegionAllocTracking::initSetRange(Bitu start,Bitu end) {
	Block x;

	assert(start <= end);

	alist.clear();
	_min = start;
	_max = end;
	_max_nonfixed = end;

	x.end = _max;
	x.free = true;
	x.start = _min;
	alist.push_back(x);
}

void RegionAllocTracking::logDump() {
	size_t si;

	LOG(LOG_MISC,LOG_DEBUG)("%s dump:",name.c_str());
	for (si=0;si < alist.size();si++) {
		Block &blk = alist[si];
		LOG(LOG_MISC,LOG_DEBUG)("     0x%08x-0x%08x free=%u %s",(int)blk.start,(int)blk.end,blk.free?1:0,blk.who.c_str());
	}
	LOG(LOG_MISC,LOG_DEBUG)("[end dump]");
}

void RegionAllocTracking::sanityCheck() {
	Block *pblk,*blk;
	size_t si;

	if (alist.size() <= 1)
		return;

	pblk = &alist[0];
	for (si=1;si < alist.size();si++) {
		blk = &alist[si];
		if (blk->start != (pblk->end+1) || blk->start > blk->end || blk->start < _min || blk->end > _max) {
			LOG(LOG_MISC,LOG_DEBUG)("RegionAllocTracking sanity check failure in '%s'",name.c_str());
			logDump();
			E_Exit("ROMBIOS sanity check failed");
		}

		pblk = blk;
	}
}

Bitu RegionAllocTracking::freeUnusedMinToLoc(Bitu phys) {
	if (phys <= _min) return _min;
	if ((_max+(Bitu)1) != (Bitu)0 && phys > (_max+1)) phys = _max+1;

	/* scan bottom-up */
	while (alist.size() != 0) {
		RegionAllocTracking::Block &blk = alist[0];
		if (!blk.free) {
			if (phys > blk.start) phys = blk.start;
			break;
		}
		if (phys > blk.end) {
			/* remove entirely */
			alist.erase(alist.begin());
			continue;
		}
		if (phys <= blk.start) break;
		blk.start = phys;
		break;
	}

	assert(phys >= _min);
	assert(_max == (Bitu)0 || phys < _max);
	return phys;
}

void RegionAllocTracking::compactFree() {
	size_t si=0;

	while ((si+1u) < alist.size()) {
		RegionAllocTracking::Block &blk1 = alist[si];
		RegionAllocTracking::Block &blk2 = alist[si+1u];

		if (blk1.free && blk2.free) {
			if ((blk1.end+(Bitu)1u) == blk2.start) {
				blk1.end = blk2.end;
				alist.erase(alist.begin()+(std::vector<RegionAllocTracking::Block>::difference_type)si+1u);
				continue;
			}
		}

		si++;
	}

	sanityCheck();
}

bool RegionAllocTracking::freeMemory(Bitu offset) {
	size_t si=0;

	if (offset < _min || offset > _max)
		return false;

	while (si < alist.size()) {
		RegionAllocTracking::Block &blk = alist[si];

		if (offset >= blk.start && offset <= blk.end) {
			LOG(LOG_BIOS,LOG_DEBUG)("freeMemory in '%s' (address=0x%08lx block='%s' range=0x%08lx-0x%08lx) success",
				name.c_str(),(unsigned long)offset,blk.who.c_str(),(unsigned long)blk.start,(unsigned long)blk.end);

			if (!blk.free) {
				blk.fixed = false;
				blk.free = true;
				blk.who.clear();
				compactFree();
			}

			return true;
		}

		si++;
	}

	LOG(LOG_BIOS,LOG_DEBUG)("freeMemory in '%s' (address=0x%08lx) FAILED",name.c_str(),(unsigned long)offset);
	return false;
}

