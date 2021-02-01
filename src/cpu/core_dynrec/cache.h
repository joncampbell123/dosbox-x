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

class CodePageHandlerDynRec;	// forward

// basic cache block representation
class CacheBlockDynRec {
public:
	void Clear(void);
	// link this cache block to another block, index specifies the code
	// path (always zero for unconditional links, 0/1 for conditional ones
	void LinkTo(Bitu index,CacheBlockDynRec * toblock) {
		assert(toblock);
		link[index].to=toblock;
		link[index].next=toblock->link[index].from;	// set target block
		toblock->link[index].from=this;				// remember who links me
	}
	struct {
		uint16_t start,end;		// where in the page is the original code
		CodePageHandlerDynRec * handler;			// page containing this code
	} page;
	struct {
		uint8_t * start;			// where in the cache are we
		uint8_t * xstart;			// where in the cache are we, executable mapping (because so much of the risc generation points at this pointer!)
		Bitu size;
		CacheBlockDynRec * next;
		// writemap masking maskpointer/start/length
		// to allow holes in the writemap
		uint8_t * wmapmask;
		uint16_t maskstart;
		uint16_t masklen;
	} cache;
	struct {
		Bitu index;
		CacheBlockDynRec * next;
	} hash;
	struct {
		CacheBlockDynRec * to;		// this block can transfer control to the to-block
		CacheBlockDynRec * next;
		CacheBlockDynRec * from;	// the from-block can transfer control to this block
	} link[2];	// maximum two links (conditional jumps)
	CacheBlockDynRec * crossblock;
};

static struct {
	struct {
		CacheBlockDynRec * first;		// the first cache block in the list
		CacheBlockDynRec * active;		// the current cache block
		CacheBlockDynRec * free;		// pointer to the free list
		CacheBlockDynRec * running;		// the last block that was entered for execution
	} block;
	uint8_t * pos;		// position in the cache block
	CodePageHandlerDynRec * free_pages;		// pointer to the free list
	CodePageHandlerDynRec * used_pages;		// pointer to the list of used pages
	CodePageHandlerDynRec * last_page;		// the last used page
} cache;


// cache memory pointers, to be malloc'd later
static uint8_t * cache_code_start_ptr=NULL;
static uint8_t * cache_code=NULL;
static uint8_t * cache_code_link_blocks=NULL;

static CacheBlockDynRec * cache_blocks=NULL;
static CacheBlockDynRec link_blocks[2];		// default linking (specially marked)


// the CodePageHandlerDynRec class provides access to the contained
// cache blocks and intercepts writes to the code for special treatment
class CodePageHandlerDynRec : public PageHandler {
public:
    CodePageHandlerDynRec() {
    }

	void SetupAt(Bitu _phys_page,PageHandler * _old_pagehandler) {
		// initialize this codepage handler
		phys_page=_phys_page;
		// save the old pagehandler to provide direct read access to the memory,
		// and to be able to restore it later on
		old_pagehandler=_old_pagehandler;

		// adjust flags
		flags=old_pagehandler->flags|PFLAG_HASCODE;
		flags&=~PFLAG_WRITEABLE;

		active_blocks=0;
		active_count=16;

		// initialize the maps with zero (no cache blocks as well as code present)
		memset(&hash_map,0,sizeof(hash_map));
		memset(&write_map,0,sizeof(write_map));
		if (invalidation_map!=NULL) {
			free(invalidation_map);
			invalidation_map=NULL;
		}
	}

	// clear out blocks that contain code which has been modified
	bool InvalidateRange(Bitu start,Bitu end) {
		Bits index=1+(Bits)(end>>(Bitu)DYN_HASH_SHIFT);
		bool is_current_block=false;	// if the current block is modified, it has to be exited as soon as possible

		uint32_t ip_point=SegPhys(cs)+reg_eip;
		ip_point=(uint32_t)((PAGING_GetPhysicalPage(ip_point)-(phys_page<<12))+(ip_point&0xfff));
		while (index>=0) {
			Bitu map=0;
			// see if there is still some code in the range
			for (Bitu count=start;count<=end;count++) map+=write_map[count];
			if (!map) return is_current_block;	// no more code, finished

			CacheBlockDynRec * block=hash_map[index];
			while (block) {
				CacheBlockDynRec * nextblock=block->hash.next;
				// test if this block is in the range
				if (start<=block->page.end && end>=block->page.start) {
					if (ip_point<=block->page.end && ip_point>=block->page.start) is_current_block=true;
					block->Clear();		// clear the block, decrements the write_map accordingly
				}
				block=nextblock;
			}
			index--;
		}
		return is_current_block;
	}

	// the following functions will clean all cache blocks that are invalid now due to the write
	void writeb(PhysPt addr,uint8_t val){
		addr&=4095;
		if (host_readb(hostmem+addr)==val) return;
		host_writeb(hostmem+addr,val);
		// see if there's code where we are writing to
		if (!host_readb(&write_map[addr])) {
			if (active_blocks) return;		// still some blocks in this page
			active_count--;
			if (!active_count) Release();	// delay page releasing until active_count is zero
			return;
		} else if (!invalidation_map) {
            invalidation_map = (uint8_t*)malloc(4096);
            if (invalidation_map != NULL)
                memset(invalidation_map, 0, 4096);
            else
                E_Exit("Memory allocation failed in writeb");
        }
        if (invalidation_map != NULL)
            invalidation_map[addr]++;
		InvalidateRange(addr,addr);
	}
	void writew(PhysPt addr,uint16_t val){
		addr&=4095;
		if (host_readw(hostmem+addr)==val) return;
		host_writew(hostmem+addr,val);
		// see if there's code where we are writing to
		if (!host_readw(&write_map[addr])) {
			if (active_blocks) return;		// still some blocks in this page
			active_count--;
			if (!active_count) Release();	// delay page releasing until active_count is zero
			return;
		} else if (!invalidation_map) {
			invalidation_map=(uint8_t*)malloc(4096);
            if (invalidation_map != NULL)
                memset(invalidation_map, 0, 4096);
            else
                E_Exit("Memory allocation failed in writew");
		}
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)
		host_writew(&invalidation_map[addr],
			host_readw(&invalidation_map[addr])+0x101);
#else
        if (invalidation_map != NULL)
            (*(uint16_t*)& invalidation_map[addr]) += 0x101;
#endif
		InvalidateRange(addr,addr+(Bitu)1);
	}
	void writed(PhysPt addr,uint32_t val){
		addr&=4095;
		if (host_readd(hostmem+addr)==val) return;
		host_writed(hostmem+addr,val);
		// see if there's code where we are writing to
		if (!host_readd(&write_map[addr])) {
			if (active_blocks) return;		// still some blocks in this page
			active_count--;
			if (!active_count) Release();	// delay page releasing until active_count is zero
			return;
		} else if (!invalidation_map) {
			invalidation_map=(uint8_t*)malloc(4096);
            if (invalidation_map != NULL)
                memset(invalidation_map, 0, 4096);
            else
                E_Exit("Memory allocation failed in writed");
		}
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)
		host_writed(&invalidation_map[addr],
			host_readd(&invalidation_map[addr])+0x1010101);
#else
        if (invalidation_map != NULL)
            (*(uint32_t*)& invalidation_map[addr]) += 0x1010101;
#endif
		InvalidateRange(addr,addr+(Bitu)3);
	}
	bool writeb_checked(PhysPt addr,uint8_t val) {
		addr&=4095;
		if (host_readb(hostmem+addr)==val) return false;
		// see if there's code where we are writing to
		if (!host_readb(&write_map[addr])) {
			if (!active_blocks) {
				// no blocks left in this page, still delay the page releasing a bit
				active_count--;
				if (!active_count) Release();
			}
		} else {
            if (!invalidation_map) {
                invalidation_map = (uint8_t*)malloc(4096);
                if (invalidation_map != NULL) {
                    memset(invalidation_map, 0, 4096);
                }
                else
                    E_Exit("Memory allocation failed in writeb_checked");
            }
            if (invalidation_map != NULL)
                invalidation_map[addr]++;
			if (InvalidateRange(addr,addr)) {
				cpu.exception.which=SMC_CURRENT_BLOCK;
				return true;
			}
		}
		host_writeb(hostmem+addr,val);
		return false;
	}
	bool writew_checked(PhysPt addr,uint16_t val) {
		addr&=4095;
		if (host_readw(hostmem+addr)==val) return false;
		// see if there's code where we are writing to
		if (!host_readw(&write_map[addr])) {
			if (!active_blocks) {
				// no blocks left in this page, still delay the page releasing a bit
				active_count--;
				if (!active_count) Release();
			}
		} else {
			if (!invalidation_map) {
				invalidation_map=(uint8_t*)malloc(4096);
                if (invalidation_map != NULL)
                    memset(invalidation_map, 0, 4096);
                else
                    E_Exit("Memory allocation failed in writew_checked");
			}
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)
			host_writew(&invalidation_map[addr],
				host_readw(&invalidation_map[addr])+0x101);
#else
            if (invalidation_map != NULL)
                (*(uint16_t*)& invalidation_map[addr]) += 0x101;
#endif
			if (InvalidateRange(addr,addr+(Bitu)1)) {
				cpu.exception.which=SMC_CURRENT_BLOCK;
				return true;
			}
		}
		host_writew(hostmem+addr,val);
		return false;
	}
	bool writed_checked(PhysPt addr,uint32_t val) {
		addr&=4095;
		if (host_readd(hostmem+addr)==val) return false;
		// see if there's code where we are writing to
		if (!host_readd(&write_map[addr])) {
			if (!active_blocks) {
				// no blocks left in this page, still delay the page releasing a bit
				active_count--;
				if (!active_count) Release();
			}
		} else {
			if (!invalidation_map) {
				invalidation_map=(uint8_t*)malloc(4096);
                if (invalidation_map != NULL)
                    memset(invalidation_map, 0, 4096);
                else
                    E_Exit("Memory allocation failed in writed_checked");
			}
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)
			host_writed(&invalidation_map[addr],
				host_readd(&invalidation_map[addr])+0x1010101);
#else
            if (invalidation_map != NULL)
                (*(uint32_t*)& invalidation_map[addr]) += 0x1010101;
#endif
			if (InvalidateRange(addr,addr+(Bitu)3)) {
				cpu.exception.which=SMC_CURRENT_BLOCK;
				return true;
			}
		}
		host_writed(hostmem+addr,val);
		return false;
	}

    // add a cache block to this page and note it in the hash map
	void AddCacheBlock(CacheBlockDynRec * block) {
		Bitu index=1u+(Bitu)(block->page.start>>(uint16_t)DYN_HASH_SHIFT);
		block->hash.next=hash_map[index];	// link to old block at index from the new block
		block->hash.index=index;
		hash_map[index]=block;				// put new block at hash position
		block->page.handler=this;
		active_blocks++;
	}
	// there's a block whose code started in a different page
    void AddCrossBlock(CacheBlockDynRec * block) {
		block->hash.next=hash_map[0];
		block->hash.index=0;
		hash_map[0]=block;
		block->page.handler=this;
		active_blocks++;
	}
	// remove a cache block
	void DelCacheBlock(CacheBlockDynRec * block) {
		active_blocks--;
		active_count=16;
		CacheBlockDynRec * * bwhere=&hash_map[block->hash.index];
		while (*bwhere!=block) {
			bwhere=&((*bwhere)->hash.next);
			//Will crash if a block isn't found, which should never happen.
		}
		*bwhere=block->hash.next;

		// remove the cleared block from the write map
		if (GCC_UNLIKELY(block->cache.wmapmask!=NULL)) {
			// first part is not influenced by the mask
			for (Bitu i=block->page.start;i<block->cache.maskstart;i++) {
				if (write_map[i]) write_map[i]--;
			}
			Bitu maskct=0;
			// last part sticks to the writemap mask
			for (Bitu i=block->cache.maskstart;i<=block->page.end;i++,maskct++) {
				if (write_map[i]) {
					// only adjust writemap if it isn't masked
					if ((maskct>=block->cache.masklen) || (!block->cache.wmapmask[maskct])) write_map[i]--;
				}
			}
			free(block->cache.wmapmask);
			block->cache.wmapmask=NULL;
		} else {
			for (Bitu i=block->page.start;i<=block->page.end;i++) {
				if (write_map[i]) write_map[i]--;
			}
		}
	}

	void Release(void) {
		MEM_SetPageHandler(phys_page,1,old_pagehandler);	// revert to old handler
		PAGING_ClearTLB();

		// remove page from the lists
		if (prev) prev->next=next;
		else cache.used_pages=next;
		if (next) next->prev=prev;
		else cache.last_page=prev;
		next=cache.free_pages;
		cache.free_pages=this;
		prev=0;
	}
	void ClearRelease(void) {
		// clear out all cache blocks in this page
		for (Bitu index=0;index<(1+DYN_PAGE_HASH);index++) {
			CacheBlockDynRec * block=hash_map[index];
			while (block) {
				CacheBlockDynRec * nextblock=block->hash.next;
				block->page.handler=0;			// no need, full clear
				block->Clear();
				block=nextblock;
			}
		}
		Release();	// now can release this page
	}

	CacheBlockDynRec * FindCacheBlock(Bitu start) {
		CacheBlockDynRec * block=hash_map[1+(start>>DYN_HASH_SHIFT)];
		// see if there's a cache block present at the start address
		while (block) {
			if (block->page.start==start) return block;	// found
			block=block->hash.next;
		}
		return 0;	// none found
	}

	HostPt GetHostReadPt(Bitu phys_page) { 
		hostmem=old_pagehandler->GetHostReadPt(phys_page);
		return hostmem;
	}
	HostPt GetHostWritePt(Bitu phys_page) { 
		return GetHostReadPt( phys_page );
	}
public:
	// the write map, there are write_map[i] cache blocks that cover the byte at address i
    uint8_t write_map[4096] = {};
    uint8_t* invalidation_map = NULL;
    CodePageHandlerDynRec* next = NULL; // page linking
    CodePageHandlerDynRec* prev = NULL; // page linking
private:
    PageHandler* old_pagehandler = NULL;

	// hash map to quickly find the cache blocks in this page
    CacheBlockDynRec* hash_map[1 + DYN_PAGE_HASH] = {};

    Bitu active_blocks = 0;     // the number of cache blocks in this page
    Bitu active_count = 0;      // delaying parameter to not immediately release a page
    HostPt hostmem = NULL;
    Bitu phys_page = 0;
};


static INLINE void cache_addunusedblock(CacheBlockDynRec * block) {
	// block has become unused, add it to the freelist
	block->cache.next=cache.block.free;
	cache.block.free=block;
}

static CacheBlockDynRec * cache_getblock(void) {
	// get a free cache block and advance the free pointer
	CacheBlockDynRec * ret=cache.block.free;
    if (!ret)
        E_Exit("Ran out of CacheBlocks");
    else {
        cache.block.free = ret->cache.next;
        ret->cache.next = 0;
    }
	return ret;
}

void CacheBlockDynRec::Clear(void) {
	// check if this is not a cross page block
	if (hash.index) for (Bitu ind=0;ind<2;ind++) {
		CacheBlockDynRec * fromlink=link[ind].from;
		link[ind].from=0;
		while (fromlink) {
			CacheBlockDynRec * nextlink=fromlink->link[ind].next;
			// clear the next-link and let the block point to the standard linkcode
			fromlink->link[ind].next=0;
			fromlink->link[ind].to=&link_blocks[ind];

			fromlink=nextlink;
		}
		if (link[ind].to!=&link_blocks[ind]) {
			// not linked to the standard linkcode, find the block that links to this block
			CacheBlockDynRec * * wherelink=&link[ind].to->link[ind].from;
			while (*wherelink != this && *wherelink) {
				wherelink = &(*wherelink)->link[ind].next;
			}
			// now remove the link
			if(*wherelink) 
				*wherelink = (*wherelink)->link[ind].next;
			else {
				LOG(LOG_CPU,LOG_ERROR)("Cache anomaly. please investigate");
			}
		}
	} else 
		cache_addunusedblock(this);
	if (crossblock) {
		// clear out the crossblock (in the page before) as well
		crossblock->crossblock=0;
		crossblock->Clear();
		crossblock=0;
	}
	if (page.handler) {
		// clear out the code page handler
		page.handler->DelCacheBlock(this);
		page.handler=0;
	}
	if (cache.wmapmask){
		free(cache.wmapmask);
		cache.wmapmask=NULL;
	}
}

static INLINE void *cache_rwtox(void *x);

static CacheBlockDynRec * cache_openblock(void) {
	CacheBlockDynRec * block=cache.block.active;
	// check for enough space in this block
	Bitu size=block->cache.size;
	CacheBlockDynRec * nextblock=block->cache.next;
	if (block->page.handler) 
		block->Clear();
	// block size must be at least CACHE_MAXSIZE
	while (size<CACHE_MAXSIZE) {
		if (!nextblock)
			goto skipresize;
		// merge blocks
		size+=nextblock->cache.size;
		CacheBlockDynRec * tempblock=nextblock->cache.next;
		if (nextblock->page.handler) 
			nextblock->Clear();
		// block is free now
		cache_addunusedblock(nextblock);
		nextblock=tempblock;
	}
skipresize:
	// adjust parameters and open this block
	block->cache.size=size;
	block->cache.next=nextblock;
	cache.pos=block->cache.start;
	return block;
}

static void cache_closeblock(void) {
	CacheBlockDynRec * block=cache.block.active;
	// links point to the default linking code
	block->link[0].to=&link_blocks[0];
	block->link[1].to=&link_blocks[1];
	block->link[0].from=0;
	block->link[1].from=0;
	block->link[0].next=0;
	block->link[1].next=0;
	// close the block with correct alignment
	Bitu written=(Bitu)(cache.pos-block->cache.start);
	if (written>block->cache.size) {
		if (!block->cache.next) {
			if (written>block->cache.size+CACHE_MAXSIZE) E_Exit("CacheBlock overrun 1 %lu",(unsigned long)written-block->cache.size);
		} else E_Exit("CacheBlock overrun 2 written %lu size %lu",(unsigned long)written,(unsigned long)block->cache.size);
	} else {
		Bitu left=block->cache.size-written;
		// smaller than cache align then don't bother to resize
		if (left>CACHE_ALIGN) {
			Bitu new_size=((written-1)|(CACHE_ALIGN-1))+1;
			CacheBlockDynRec * newblock=cache_getblock();
			// align block now to CACHE_ALIGN
			newblock->cache.start=block->cache.start+new_size;
			newblock->cache.size=block->cache.size-new_size;
			newblock->cache.next=block->cache.next;
			newblock->cache.xstart=(uint8_t*)cache_rwtox(newblock->cache.start);
			block->cache.next=newblock;
			block->cache.size=new_size;
		}
	}
	// advance the active block pointer
	if (!block->cache.next || (block->cache.next->cache.start>(cache_code_start_ptr + CACHE_TOTAL - CACHE_MAXSIZE))) {
//		LOG_MSG("Cache full restarting");
		cache.block.active=cache.block.first;
	} else {
		cache.block.active=block->cache.next;
	}
}


// place an 8bit value into the cache
static INLINE void cache_addb(uint8_t val,uint8_t *pos) {
	*pos=val;
}
static INLINE void cache_addb(uint8_t val) {
	uint8_t *pos=cache.pos+1;
	cache_addb(val,cache.pos);
	cache.pos=pos;
}

// place a 16bit value into the cache
static INLINE void cache_addw(uint16_t val,uint8_t *pos) {
	*(uint16_t*)pos=val;
}
static INLINE void cache_addw(uint16_t val) {
	uint8_t *pos=cache.pos+2;
	cache_addw(val,cache.pos);
	cache.pos=pos;
}

// place a 32bit value into the cache
static INLINE void cache_addd(uint32_t val,uint8_t *pos) {
	*(uint32_t*)pos=val;
}
static INLINE void cache_addd(uint32_t val) {
	uint8_t *pos=cache.pos+4;
	cache_addd(val,cache.pos);
	cache.pos=pos;
}

// place a 64bit value into the cache
static INLINE void cache_addq(uint64_t val,uint8_t *pos) {
	*(uint64_t*)pos=val;
}
static INLINE void cache_addq(uint64_t val) {
	uint8_t *pos=cache.pos+8;
	cache_addq(val,cache.pos);
	cache.pos=pos;
}


static void dyn_return(BlockReturn retcode,bool ret_exception);
static void dyn_run_code(void);

static bool cache_initialized = false;

#include "cpu/dynamic_alloc_common.h"

static void cache_ensure_allocation(void) {
	if (cache_code_start_ptr==NULL) {
        cache_dynamic_common_alloc(CACHE_TOTAL+CACHE_MAXSIZE); /* sets cache_code_start_ptr/cache_code */
 
		cache_code_link_blocks=cache_code;
		cache_code+=PAGESIZE_TEMP;
	}
}

static void cache_reset(void) {
	if (cache_initialized) {
		for (;;) {
			if (cache.used_pages) {
				CodePageHandlerDynRec * cpage=cache.used_pages;
				CodePageHandlerDynRec * npage=cache.used_pages->next;
				cpage->ClearRelease();
				delete cpage;
				cache.used_pages=npage;
			} else break;
		}

		if (cache_blocks == NULL) {
			cache_blocks=(CacheBlockDynRec*)malloc(CACHE_BLOCKS*sizeof(CacheBlockDynRec));
			if(!cache_blocks) E_Exit("Allocating cache_blocks has failed");
		}
		memset(cache_blocks,0,sizeof(CacheBlockDynRec)*CACHE_BLOCKS);
		cache.block.free=&cache_blocks[0];
		for (Bits i=0;i<CACHE_BLOCKS-1;i++) {
			cache_blocks[i].link[0].to=(CacheBlockDynRec *)1;
			cache_blocks[i].link[1].to=(CacheBlockDynRec *)1;
			cache_blocks[i].cache.next=&cache_blocks[i+1];
		}

		cache_remap_rw();

		cache_ensure_allocation();

		CacheBlockDynRec * block=cache_getblock();
		cache.block.first=block;
		cache.block.active=block;
		block->cache.start=&cache_code[0];
		block->cache.xstart=(uint8_t*)cache_rwtox(block->cache.start);
		block->cache.size=CACHE_TOTAL;
		block->cache.next=0;								//Last block in the list

		/* Setup the default blocks for block linkage returns */
		cache.pos=&cache_code_link_blocks[0];
		link_blocks[0].cache.start=cache.pos;
		link_blocks[0].cache.xstart=(uint8_t*)cache_rwtox(link_blocks[0].cache.start);
		dyn_return(BR_Link1,false);
		cache.pos=&cache_code_link_blocks[32];
		link_blocks[1].cache.start=cache.pos;
		link_blocks[1].cache.xstart=(uint8_t*)cache_rwtox(link_blocks[1].cache.start);
		dyn_return(BR_Link2,false);
		cache.free_pages=0;
		cache.last_page=0;
		cache.used_pages=0;
		/* Setup the code pages */
		for (Bitu i=0;i<CACHE_PAGES;i++) {
			CodePageHandlerDynRec * newpage=new CodePageHandlerDynRec();
			newpage->next=cache.free_pages;
			cache.free_pages=newpage;
		}

		cache_remap_rx();
	}
}


static void cache_init(bool enable) {
	if (enable) {
		Bits i;
		// see if cache is already initialized
		if (cache_initialized) return;
		cache_initialized = true;
		if (cache_blocks == NULL) {
			// allocate the cache blocks memory
			cache_blocks=(CacheBlockDynRec*)malloc(CACHE_BLOCKS*sizeof(CacheBlockDynRec));
            if (!cache_blocks)
                E_Exit("Allocating cache_blocks has failed");
            else
                memset(cache_blocks, 0, sizeof(CacheBlockDynRec) * CACHE_BLOCKS);
			cache.block.free=&cache_blocks[0];
			// initialize the cache blocks
            if (cache_blocks != NULL) {
                for (i = 0; i < CACHE_BLOCKS - 1; i++) {
                    cache_blocks[i].link[0].to = (CacheBlockDynRec*)1;
                    cache_blocks[i].link[1].to = (CacheBlockDynRec*)1;
                    cache_blocks[i].cache.next = &cache_blocks[i + 1];
                }
            }
		}

		if (cache_code_start_ptr==NULL) {
			cache_ensure_allocation();

			CacheBlockDynRec * block=cache_getblock();
			cache.block.first=block;
			cache.block.active=block;
			block->cache.start=&cache_code[0];
			block->cache.xstart=(uint8_t*)cache_rwtox(block->cache.start);
			block->cache.size=CACHE_TOTAL;
			block->cache.next=0;						// last block in the list
		}
		// setup the default blocks for block linkage returns
		cache.pos=&cache_code_link_blocks[0];
		link_blocks[0].cache.start=cache.pos;
		link_blocks[0].cache.xstart=(uint8_t*)cache_rwtox(link_blocks[0].cache.start);
		// link code that returns with a special return code
		dyn_return(BR_Link1,false);
		cache.pos=&cache_code_link_blocks[32];
		link_blocks[1].cache.start=cache.pos;
		link_blocks[1].cache.xstart=(uint8_t*)cache_rwtox(link_blocks[1].cache.start);
		// link code that returns with a special return code
		dyn_return(BR_Link2,false);

		cache.pos=&cache_code_link_blocks[64];
		*(void**)(&core_dynrec.runcode) = (void*)cache_rwtox(cache.pos);
//		link_blocks[1].cache.start=cache.pos;
		dyn_run_code();

		cache.free_pages=0;
		cache.last_page=0;
		cache.used_pages=0;
		// setup the code pages
		for (i=0;i<CACHE_PAGES;i++) {
			CodePageHandlerDynRec * newpage=new CodePageHandlerDynRec();
			newpage->next=cache.free_pages;
			cache.free_pages=newpage;
		}
	}
}

static void cache_close(void) {
/*	for (;;) {
		if (cache.used_pages) {
			CodePageHandler * cpage=cache.used_pages;
			CodePageHandler * npage=cache.used_pages->next;
			cpage->ClearRelease();
			delete cpage;
			cache.used_pages=npage;
		} else break;
	}
	if (cache_blocks != NULL) {
		free(cache_blocks);
		cache_blocks = NULL;
	}
	if (cache_code_start_ptr != NULL) {
		### care: under windows VirtualFree() has to be used if
		###       VirtualAlloc was used for memory allocation
		free(cache_code_start_ptr);
		cache_code_start_ptr = NULL;
	}
	cache_code = NULL;
	cache_code_link_blocks = NULL;
	cache_initialized = false; */
}
