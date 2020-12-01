
/* Define temporary pagesize so the MPROTECT case and the regular case share as much code as possible */
#if (C_HAVE_MPROTECT)
#define PAGESIZE_TEMP PAGESIZE
#else 
#define PAGESIZE_TEMP 4096
#endif

#include <unistd.h>

static int cache_fd = -1;

static void cache_dynamic_common_alloc(Bitu allocsz) {
    Bitu actualsz = allocsz+PAGESIZE_TEMP;

    assert(cache_code_start_ptr == NULL);
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
#if defined(C_HAVE_MMAP)
    if (cache_code_start_ptr == NULL) {
        cache_code_start_ptr=(uint8_t*)mmap(NULL,actualsz,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if (cache_code_start_ptr != MAP_FAILED) dyncore_alloc = DYNCOREALLOC_MMAP_ANON;
        else cache_code_start_ptr = NULL; /* MAP_FAILED is NOT NULL (or at least we cannot assume that) */
    }
#endif
#if defined(C_HAVE_MEMFD_CREATE)
    if (cache_code_start_ptr == NULL) {
        assert(cache_fd < 0);
        cache_fd = memfd_create("dosbox-dynamic-core-cache",MFD_CLOEXEC);
        if (cache_fd >= 0) {
            if (ftruncate(cache_fd,actualsz) == 0) {
                cache_code_start_ptr=(uint8_t*)mmap(NULL,actualsz,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_SHARED,cache_fd,0);
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
#if defined(C_HAVE_MMAP) /* try again, this time detect if read/write and read/execute are allowed (W^X) */
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
    if (cache_code_start_ptr == NULL) {
        cache_code_start_ptr=(uint8_t*)malloc(actualsz);
        if (cache_code_start_ptr != NULL) dyncore_alloc = DYNCOREALLOC_MALLOC;
    }

    if (cache_code_start_ptr == NULL)
        E_Exit("Allocating dynamic cache failed");

    cache_code=(uint8_t*)(((Bitu)cache_code_start_ptr+(PAGESIZE_TEMP-1)) & ~(PAGESIZE_TEMP-1)); //Bitu is same size as a pointer.

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
        default:                            LOG(LOG_MISC,LOG_DEBUG)("dyncore method: ?"); break;
    };

    assert((cache_code+allocsz) <= (cache_code_start_ptr+actualsz));
}

