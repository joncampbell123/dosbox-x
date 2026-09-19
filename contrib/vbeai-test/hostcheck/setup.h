#ifndef H_SETUP_H
#define H_SETUP_H
#include "dosbox.h"
class Section { public: virtual ~Section(){} };
class Section_prop : public Section { public: bool Get_bool(const char*) const; };
class Config { public: Section* GetSection(const char*) const; };
#endif
