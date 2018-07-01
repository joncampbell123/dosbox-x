#ifndef DOSBOX_OUTPUT_TOOLS_H
#define DOSBOX_OUTPUT_TOOLS_H

// common headers and static routines reused in different outputs go there

static inline int int_log2(int val)
{
    int log = 0;
    while ((val >>= 1) != 0) log++;
    return log;
}

#endif