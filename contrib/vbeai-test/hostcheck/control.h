#ifndef H_CONTROL_H
#define H_CONTROL_H
#include "dosbox.h"
#include "setup.h"
/* Mirrors the real split: dosbox.h only forward-declares Config, and its
 * definition lives here -- so a translation unit that uses `control` must
 * include control.h, not just setup.h. */
class Config { public: Section* GetSection(const char*) const; };
#endif
