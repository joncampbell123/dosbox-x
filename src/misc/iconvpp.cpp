
#include "dosbox.h"
#include "iconvpp.hpp"

const char *_Iconv_CommonBase::errstring(int x) {
    if (x >= 0)
        return "no error";
    else if (x == err_noinit)
        return "not initialized";
    else if (x == err_noroom)
        return "out of room";
    else if (x == err_notvalid)
        return "illegal multibyte sequence or invalid state";
    else if (x == err_incomplete)
        return "incomplete multibyte sequence";

    return "?";
}

