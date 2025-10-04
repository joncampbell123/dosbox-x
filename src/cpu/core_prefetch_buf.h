
#define PREFETCH_CORE

/* WARNING: This code needs MORE TESTING. So far, it seems to work fine. */

template <class T> static inline bool prefetch_hit(const Bitu w) {
    return pq_valid && (w >= pq_start && (w + sizeof(T)) <= pq_fill);
}

template <class T> static inline T prefetch_read(const Bitu w);

template <class T> static inline void prefetch_read_check(const Bitu w) {
    (void)w;//POSSIBLY UNUSED
#ifdef PREFETCH_DEBUG
    if (!pq_valid) E_Exit("CPU: Prefetch read when not valid!");
    if (w < pq_start) E_Exit("CPU: Prefetch read below prefetch base");
    if ((w+sizeof(T)) > pq_fill) E_Exit("CPU: Prefetch read beyond prefetch fill");
#endif
}

template <> uint8_t prefetch_read<uint8_t>(const Bitu w) {
    prefetch_read_check<uint8_t>(w);
    return prefetch_buffer[w - pq_start];
}

template <> uint16_t prefetch_read<uint16_t>(const Bitu w) {
    prefetch_read_check<uint16_t>(w);
    return host_readw(&prefetch_buffer[w - pq_start]);
}

template <> uint32_t prefetch_read<uint32_t>(const Bitu w) {
    prefetch_read_check<uint32_t>(w);
    return host_readd(&prefetch_buffer[w - pq_start]);
}

static inline void prefetch_init(const Bitu start) {
    /* start must be DWORD aligned */
    pq_start = pq_fill = start;
    pq_valid = true;
}

static inline void prefetch_filldword(void) {
    host_writed(&prefetch_buffer[pq_fill - pq_start],LoadMd((PhysPt)pq_fill));
    pq_fill += prefetch_unit;
}

static inline void prefetch_refill(const Bitu stop) {
    while (pq_fill < stop) prefetch_filldword();
}

static inline void prefetch_lazyflush(const Bitu w) {
    /* assume: prefetch buffer hit.
     * assume: w >= pq_start + sizeof(T) and w + sizeof(T) <= pq_fill
     * assume: prefetch buffer is full.
     * assume: w is the memory address + sizeof(T)
     * assume: pq_start is DWORD aligned.
     * assume: CPU_PrefetchQueueSize >= 4 */
    if ((w - pq_start) >= pq_limit) {
        memmove(prefetch_buffer,prefetch_buffer+prefetch_unit,pq_limit-prefetch_unit);
        pq_start += prefetch_unit;

        prefetch_filldword();
    }

#ifdef PREFETCH_DEBUG
    assert(pq_fill >= pq_start);
    assert((pq_fill - pq_start) <= pq_limit);
#endif
}

/* this implementation follows what I think the Intel 80386/80486 is more likely
 * to do when fetching from prefetch and refilling prefetch --J.C. */
template <class T> static inline T Fetch(void) {
    T temp;

    if (prefetch_hit<T>(core.cseip)) {
        /* as long as prefetch hits are occurring, keep loading more! */
        prefetch_lazyflush(core.cseip+sizeof(T));
        if ((pq_fill - pq_start) < pq_limit)
            prefetch_filldword();

        if (sizeof(T) >= prefetch_unit) {
            if ((pq_fill - pq_start) < pq_limit)
                prefetch_filldword();
        }

        temp = prefetch_read<T>(core.cseip);
#ifdef PREFETCH_DEBUG
        pq_hit++;
#endif
    }
    else {
        prefetch_init(core.cseip & (~(prefetch_unit-1ul))); /* fill prefetch starting on DWORD boundary */
        prefetch_refill(pq_start + pq_reload); /* perhaps in the time it takes for a prefetch miss the 80486 can load two DWORDs */
        temp = prefetch_read<T>(core.cseip);
#ifdef PREFETCH_DEBUG
        pq_miss++;
#endif
    }

#ifdef PREFETCH_DEBUG
    if (pq_valid) {
        assert(core.cseip >= pq_start && (core.cseip+sizeof(T)) <= pq_fill);
        assert(pq_fill >= pq_start && (pq_fill - pq_start) <= pq_limit);
    }
#endif

    core.cseip += sizeof(T);
    return temp;
}

template <class T> static inline void FetchDiscard(void) {
    core.cseip += sizeof(T);
}

template <class T> static inline T FetchPeek(void) {
    T temp;

    if (prefetch_hit<T>(core.cseip)) {
        /* as long as prefetch hits are occurring, keep loading more! */
        prefetch_lazyflush(core.cseip+sizeof(T));
        if ((pq_fill - pq_start) < pq_limit)
            prefetch_filldword();

        if (sizeof(T) >= prefetch_unit) {
            if ((pq_fill - pq_start) < pq_limit)
                prefetch_filldword();
        }

        temp = prefetch_read<T>(core.cseip);
#ifdef PREFETCH_DEBUG
        pq_hit++;
#endif
    }
    else {
        prefetch_init(core.cseip & (~(prefetch_unit-1ul))); /* fill prefetch starting on DWORD boundary */
        prefetch_refill(pq_start + pq_reload); /* perhaps in the time it takes for a prefetch miss the 80486 can load two DWORDs */
        temp = prefetch_read<T>(core.cseip);
#ifdef PREFETCH_DEBUG
        pq_miss++;
#endif
    }

#ifdef PREFETCH_DEBUG
    if (pq_valid) {
        assert(core.cseip >= pq_start && (core.cseip+sizeof(T)) <= pq_fill);
        assert(pq_fill >= pq_start && (pq_fill - pq_start) <= pq_limit);
    }
#endif

    return temp;
}

