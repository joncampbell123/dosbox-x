
#include "dosbox.h"

#include <vector>
#include <string>

#ifndef DOSBOX_REGIONALLOCTRACKING_H
#define DOSBOX_REGIONALLOCTRACKING_H

/* rombios memory block */
class RegionAllocTracking {
public:
	class Block {
public:
						Block();
public:
		std::string			who;
		Bitu				start;		/* start-end of the block inclusive */
		Bitu				end;
		bool				free;
	};
public:
						RegionAllocTracking();
public:
	Bitu					getMemory(Bitu bytes,const char *who,Bitu alignment,Bitu must_be_at);
	void					initSetRange(Bitu start,Bitu end);
	Bitu					freeUnusedMinToLoc(Bitu phys);
	bool					freeMemory(Bitu offset);
	Bitu					getMinAddress();	
	void					sanityCheck();
	void					logDump();
public:
	std::string				name;
	std::vector<Block>			alist;
	Bitu					min,max;
	bool					topDownAlloc;
public:
	static const Bitu			alloc_failed = ~((Bitu)0);
};

#endif /* DOSBOX_REGIONALLOCTRACKING_H */

