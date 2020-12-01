
/* Define temporary pagesize so the MPROTECT case and the regular case share as much code as possible */
#if (C_HAVE_MPROTECT)
#define PAGESIZE_TEMP PAGESIZE
#else 
#define PAGESIZE_TEMP 4096
#endif

static void cache_dynamic_common_alloc(Bitu allocsz) {
    Bitu actualsz = allocsz+PAGESIZE_TEMP;

    assert(actualsz >= allocsz);

#if defined (WIN32)
    cache_code_start_ptr=(uint8_t*)VirtualAlloc(0,actualsz,MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    if (!cache_code_start_ptr)
        cache_code_start_ptr=(uint8_t*)malloc(actualsz);
#else
    cache_code_start_ptr=(uint8_t*)malloc(actualsz);
#endif
    if (!cache_code_start_ptr) E_Exit("Allocating dynamic cache failed");

    cache_code=(uint8_t*)(((Bitu)cache_code_start_ptr+(PAGESIZE_TEMP-1)) & ~(PAGESIZE_TEMP-1)); //Bitu is same size as a pointer.

    assert((cache_code+allocsz) <= (cache_code_start_ptr+actualsz));
}

