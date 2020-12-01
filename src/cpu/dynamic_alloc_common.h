
/* Define temporary pagesize so the MPROTECT case and the regular case share as much code as possible */
#if (C_HAVE_MPROTECT)
#define PAGESIZE_TEMP PAGESIZE
#else 
#define PAGESIZE_TEMP 4096
#endif

static void cache_dynamic_common_alloc(Bitu allocsz) {
    Bitu actualsz = allocsz+PAGESIZE_TEMP;

    assert(cache_code_start_ptr == NULL);
    assert(cache_code == NULL);
    assert(actualsz >= allocsz);

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
        default:                            LOG(LOG_MISC,LOG_DEBUG)("dyncore alloc: ?"); break;
    };

    assert((cache_code+allocsz) <= (cache_code_start_ptr+actualsz));
}

