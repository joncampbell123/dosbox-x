
#include "dosbox.h"

#include <vector>

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

