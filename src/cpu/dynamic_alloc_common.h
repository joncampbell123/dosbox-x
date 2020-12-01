
/* Define temporary pagesize so the MPROTECT case and the regular case share as much code as possible */
#if (C_HAVE_MPROTECT)
#define PAGESIZE_TEMP PAGESIZE
#else 
#define PAGESIZE_TEMP 4096
#endif

#include <unistd.h>

#if (C_HAVE_MPROTECT) && (C_HAVE_MACH_VM_REMAP)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

/* DEBUG: Force dual rw/rx on a Linux system that otherwise allows rwx */
//#define DEBUG_LINUX_FORCE_MEMFD_DUAL_RW_X

static int cache_fd = -1;
static uint8_t *cache_code_init = NULL; // NTS: Because dynamic code modifies cache_code as needed
static uint8_t *cache_exec_ptr = NULL;
static Bitu cache_map_size = 0;

static INLINE void *cache_rwtox(void *x) {
    return (void*)((uintptr_t)((char*)x) + (uintptr_t)((char*)cache_exec_ptr) - (uintptr_t)((char*)cache_code_init));
}

static void cache_remap_rx() {
    if (cache_code_start_ptr != NULL && cache_map_size != 0) {
        if (dyncore_method == DYNCOREM_MPROTECT_RW_RX) {
            if (mprotect(cache_code_start_ptr,cache_map_size,PROT_READ|PROT_EXEC) < 0)
                E_Exit("dyn cache remap rx failed");
        }
    }
}

static void cache_remap_rw() {
    if (cache_code_start_ptr != NULL && cache_map_size != 0) {
        if (dyncore_method == DYNCOREM_MPROTECT_RW_RX) {
            if (mprotect(cache_code_start_ptr,cache_map_size,PROT_READ|PROT_WRITE) < 0)
                E_Exit("dyn cache remap rw failed");
        }
    }
}

static void cache_dynamic_common_alloc(Bitu allocsz) {
    Bitu actualsz = allocsz+PAGESIZE_TEMP;

    assert(cache_code_start_ptr == NULL);
    assert(cache_exec_ptr == NULL);
    assert(cache_code == NULL);
    assert(actualsz >= allocsz);

    dyncore_flags = 0;
    dyncore_method = DYNCOREM_RWX;
    dyncore_alloc = DYNCOREALLOC_NONE;

#if defined (WIN32)
    if (cache_code_start_ptr == NULL) {
        cache_code_start_ptr=(uint8_t*)VirtualAlloc(0,actualsz,MEM_COMMIT,PAGE_EXECUTE_READWRITE);
        if (cache_code_start_ptr != NULL) dyncore_alloc = DYNCOREALLOC_VIRTUALALLOC;
    }
#endif
#if defined(C_HAVE_MMAP) && !defined(DEBUG_LINUX_FORCE_MEMFD_DUAL_RW_X) /* Try straightforward read/write/execute mmap first. */
    if (cache_code_start_ptr == NULL) {
        cache_code_start_ptr=(uint8_t*)mmap(NULL,actualsz,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if (cache_code_start_ptr != MAP_FAILED) dyncore_alloc = DYNCOREALLOC_MMAP_ANON;
        else cache_code_start_ptr = NULL; /* MAP_FAILED is NOT NULL (or at least we cannot assume that) */
    }
#endif
#if defined(C_HAVE_MEMFD_CREATE) /* Try a Linux memfd which we can mmap twice, one read/write, one read/execute */
    if (cache_code_start_ptr == NULL) {
        assert(cache_fd < 0);
        cache_fd = memfd_create("dosbox-dynamic-core-cache",MFD_CLOEXEC);
        if (cache_fd >= 0) {
            if (ftruncate(cache_fd,actualsz) == 0) {
#if defined(DEBUG_LINUX_FORCE_MEMFD_DUAL_RW_X)
                cache_code_start_ptr=(uint8_t*)MAP_FAILED; // fail on purpose, trigger dual map mode
#else
                cache_code_start_ptr=(uint8_t*)mmap(NULL,actualsz,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_SHARED,cache_fd,0);
#endif
                if (cache_code_start_ptr == MAP_FAILED) {
                    cache_code_start_ptr=(uint8_t*)mmap(NULL,actualsz,PROT_READ|PROT_EXEC,MAP_SHARED,cache_fd,0);
                    if (cache_code_start_ptr != MAP_FAILED) {
                        cache_exec_ptr=(uint8_t*)mmap(cache_code_start_ptr/*place after this!*/,actualsz,PROT_READ|PROT_WRITE,MAP_SHARED,cache_fd,0);
                        if (cache_exec_ptr != MAP_FAILED) {
                            std::swap(cache_exec_ptr,cache_code_start_ptr);
                            dyncore_method = DYNCOREM_DUAL_RW_X;
                            dyncore_flags |= DYNCOREF_W_XOR_X;
                        }
                        else {
                            assert(munmap(cache_code_start_ptr,actualsz) == 0);
                            cache_code_start_ptr = NULL;
                        }
                    }
                }
                if (cache_code_start_ptr != MAP_FAILED) {
                    dyncore_alloc = DYNCOREALLOC_MEMFD;
                }
            }

            if (cache_code_start_ptr == MAP_FAILED)
                cache_code_start_ptr = NULL;

            if (cache_code_start_ptr == NULL) {
                close(cache_fd);
                cache_fd = -1;
            }
        }
    }
#endif
#if defined(C_HAVE_MMAP) && defined(C_HAVE_MPROTECT) /* try again, this time detect if read/write and read/execute are allowed (W^X), and if so, call mprotect() each time it is necessary to modify */
    if (cache_code_start_ptr == NULL) {
        cache_code_start_ptr=(uint8_t*)mmap(NULL,actualsz,PROT_READ|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if (cache_code_start_ptr != MAP_FAILED) {
            if (munmap(cache_code_start_ptr,actualsz) == 0) {
                cache_code_start_ptr=(uint8_t*)mmap(NULL,actualsz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
                if (cache_code_start_ptr != MAP_FAILED) {
                    dyncore_alloc = DYNCOREALLOC_MMAP_ANON;
                    dyncore_method = DYNCOREM_MPROTECT_RW_RX;
                    dyncore_flags |= DYNCOREF_W_XOR_X;
                }
            }
            else {
                cache_code_start_ptr = (uint8_t*)MAP_FAILED;
            }
        }
        if (cache_code_start_ptr == MAP_FAILED)
            cache_code_start_ptr = NULL;
    }
#endif
#if defined(C_HAVE_POSIX_MEMALIGN)
    if (cache_code_start_ptr == NULL) { /* try again, use posix_memalign for an aligned pointer, mprotect */
        void *p = NULL;
        if (posix_memalign(&p,PAGESIZE_TEMP,actualsz) == 0) {
            cache_code_start_ptr=(uint8_t*)p;
            if (cache_code_start_ptr != NULL) dyncore_alloc = DYNCOREALLOC_MALLOC; /* return value of posix_memalign() can be passed to free() */
        }
    }
#endif
    if (cache_code_start_ptr == NULL) { /* try again, just use malloc, align ptr, mprotect */
        cache_code_start_ptr=(uint8_t*)malloc(actualsz);
        if (cache_code_start_ptr != NULL) dyncore_alloc = DYNCOREALLOC_MALLOC;
    }

    if (cache_code_start_ptr == NULL)
        E_Exit("Allocating dynamic cache failed");

    cache_code=(uint8_t*)(((Bitu)cache_code_start_ptr+(PAGESIZE_TEMP-1)) & ~(PAGESIZE_TEMP-1)); //Bitu is same size as a pointer.

#if (C_HAVE_MPROTECT) && (C_HAVE_MACH_VM_REMAP)
    if (dyncore_method == DYNCOREM_MPROTECT_RW_RX && (dyncore_flags&DYNCOREF_W_XOR_X)) {
        /* Darwin has a kernel call to remap our own process space twice. Try it. */
        mach_vm_address_t rw = (mach_vm_address_t)cache_code;
        mach_vm_address_t rx = (mach_vm_address_t)0;
        vm_prot_t cur_prot, max_prot;
        kern_return_t ret;

        ret = mach_vm_remap(
            mach_task_self(),&rx,actualsz,              /* to */
            PAGESIZE_TEMP - 1,                          /* align to PAGESIZE_TEMP */
            VM_FLAGS_ANYWHERE,                          /* don't care where */
            mach_task_self(), rw,                       /* from */
            false,                                      /* don't copy */
            &cur_prot,&max_prot,                        /* protections */
            VM_INHERIT_NONE);

        if (ret == KERN_SUCCESS) {
            if (mprotect((void*)rx,actualsz,PROT_READ|PROT_EXEC) == 0) {
                LOG(LOG_MISC,LOG_DEBUG)("Darwin approves, dual mapping method (r/x=%p r/w=%p)",(void*)rw,(void*)rx);
                dyncore_method = DYNCOREM_DUAL_RW_X;
                cache_exec_ptr = (uint8_t*)rx;
            }
        }
    }
#endif
    if (dyncore_alloc == DYNCOREALLOC_MALLOC && dyncore_method == DYNCOREM_RWX && !(dyncore_flags&DYNCOREF_W_XOR_X)) {
#if (C_HAVE_MPROTECT)
		if (mprotect(cache_code,actualsz,PROT_WRITE|PROT_READ|PROT_EXEC)) {
			E_Exit("Setting execute permission on the code cache has failed (malloc)! err=%s",strerror(errno));
		}
#endif
    }

    switch (dyncore_alloc) {
        case DYNCOREALLOC_NONE:             LOG(LOG_MISC,LOG_DEBUG)("dyncore alloc: none"); break;
        case DYNCOREALLOC_VIRTUALALLOC:     LOG(LOG_MISC,LOG_DEBUG)("dyncore alloc: VirtualAlloc"); break;
        case DYNCOREALLOC_MMAP_ANON:        LOG(LOG_MISC,LOG_DEBUG)("dyncore alloc: mmap using MAP_PRIVATE|MAP_ANONYMOUS"); break;
        case DYNCOREALLOC_MALLOC:           LOG(LOG_MISC,LOG_DEBUG)("dyncore alloc: malloc"); break;
        case DYNCOREALLOC_MEMFD:            LOG(LOG_MISC,LOG_DEBUG)("dyncore alloc: memfd"); break;
        default:                            LOG(LOG_MISC,LOG_DEBUG)("dyncore alloc: ?"); break;
    };

    switch (dyncore_method) {
        case DYNCOREM_NONE:                 LOG(LOG_MISC,LOG_DEBUG)("dyncore method: none"); break;
        case DYNCOREM_RWX:                  LOG(LOG_MISC,LOG_DEBUG)("dyncore method: rwx"); break;
        case DYNCOREM_MPROTECT_RW_RX:       LOG(LOG_MISC,LOG_DEBUG)("dyncore method: mprotect rw/rx"); break;
        case DYNCOREM_DUAL_RW_X:            LOG(LOG_MISC,LOG_DEBUG)("dyncore method: dual rw/rx"); break;
        default:                            LOG(LOG_MISC,LOG_DEBUG)("dyncore method: ?"); break;
    };

    /* if no separate exec pointer, then exec == cache_code (rw ptr) */
    if (cache_exec_ptr == NULL)
        cache_exec_ptr = cache_code;

#if defined(__GNUC__)
    /* make sure dual mapping works */
    if (cache_exec_ptr != cache_code) {
        *cache_code = 0xAA;
        __builtin___clear_cache((char*)cache_code,(char*)(cache_code+128));
        if (*cache_exec_ptr == 0xAA) {
            LOG(LOG_MISC,LOG_DEBUG)("dyncore: dual map test passed. adj=0x%lx",(unsigned long)((uintptr_t)((char*)cache_exec_ptr) - (uintptr_t)((char*)cache_code)));
        }
        else {
            E_Exit("dyncore: dual map test failed");
        }
    }
#endif

    cache_map_size = actualsz;
    cache_code_init = cache_code;
    assert((cache_code+allocsz) <= (cache_code_start_ptr+actualsz));

    LOG(LOG_MISC,LOG_DEBUG)("dyncore: allocated cache size=%lu rw=%p rx=%p",
        (unsigned long)cache_map_size,(void*)cache_code,(void*)cache_exec_ptr);
}

