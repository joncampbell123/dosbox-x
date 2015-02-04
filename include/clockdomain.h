
/* DOSBox-X clock domain class.
 * The clock domain implementation allows DOSBox-X to accurately
 * track time in clocks instead of less precise floating point
 * intervals, and to more accurately emulate hardware in terms of
 * the reference clock.
 *
 * (C) 2014 Jonathan Campbell */

#include <stdint.h>
#include <assert.h>
#include <math.h>

#include <string>
#include <vector>
#include <list>

#ifndef DOSBOX_CLOCKDOMAIN_H
#define DOSBOX_CLOCKDOMAIN_H

#include "dosbox.h"
/* this code contains support for existing DOSBox code that uses PIC_AddEvent, etc. callbacks */
#include "pic.h"

class ClockDomain {
public:
	ClockDomain() {
		freq = 0;
		freq_div = 1;
	}
	ClockDomain(unsigned long long freq_new) {
		freq = freq_new;
		freq_div = 1;
	}
	/* we allow non-integer frequencies as integer fractions.
	 * example: 33.3333333...MHz as 100,000,000Hz / 3 */
	ClockDomain(unsigned long long freq_new,unsigned long long div) {
		freq = freq_new;
		freq_div = div;
	}
public:
	void set_name(const char *s) {
		name = s;
	}
	void set_frequency(unsigned long long freq_new,unsigned long long div_new=1) {
		counter = 0;
		freq = freq_new;
		freq_div = div_new;
	}
	const char *get_name() {
		return name.c_str();
	}
public:
	/* NTS: Slave clock rules:
	 *       - Must have the same "freq" value as master
	 *       - Do not set clock time by floating point time (only the toplevel clocks in the tree should do that)
	 *       - Must rebase at the same reference time as the master
	 *       - Must maintain time according to master time divided by master's clock divider */
	unsigned long long		freq,freq_div;	/* frequency of clock as integer ratio */
	unsigned long long		counter;	/* in units of freq */
	std::string			name;
};

#endif //DOSBOX_CLOCKDOMAIN_H

