
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

class ClockDomain;
class ClockDomainEvent;

typedef void (*ClockDomainEventHandler)(ClockDomainEvent *ev);

#define CLOCKDOM_DONTCARE		( ~((unsigned long long)0) )	/* a.k.a 0xFFFF'FFFF'FFFF'FFFF */

class ClockDomainEvent {
public:
	ClockDomainEvent() {
		domain = NULL;
		t_clock = 0;
		value = 0;
		cb = NULL;
		cb_pic = NULL;
		in_progress = false;
	}
public:
	bool				in_progress;
	unsigned long long		t_clock;	/* <- at what clock tick to fire (in freq ticks NOT freq/div) */
	unsigned long long		value;		/* <- this is up to the code assigning the event */
	ClockDomain*			domain;
	ClockDomainEventHandler		cb;
	PIC_EventHandler		cb_pic;
};

class ClockDomain {
public:
	ClockDomain() {
		freq = 0;
		freq_div = 1;
		rmaster_mult = 1;
		rmaster_div = 1;
	}
	ClockDomain(unsigned long long freq_new) {
		freq = freq_new;
		freq_div = 1;
		rmaster_mult = 1;
		rmaster_div = 1;
	}
	/* we allow non-integer frequencies as integer fractions.
	 * example: 33.3333333...MHz as 100,000,000Hz / 3 */
	ClockDomain(unsigned long long freq_new,unsigned long long div) {
		freq = freq_new;
		freq_div = div;
		rmaster_mult = 1;
		rmaster_div = 1;
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
	void remove_event(ClockDomainEventHandler cb,unsigned long long t_clk=CLOCKDOM_DONTCARE,unsigned long long val=CLOCKDOM_DONTCARE) {
		std::list<ClockDomainEvent>::iterator i;

		i=events.begin();
		while (i != events.end()) {
			if ((*i).cb == cb &&
				(t_clk == CLOCKDOM_DONTCARE || (*i).t_clock == t_clk) &&
				(val == CLOCKDOM_DONTCARE || (*i).value == val)) {
				if ((*i).in_progress)
					return; /* found it, but it's in progress now and will be deleted once the callback finishes */
				else
					break; /* found it, exit loop to erase() below */
			}

			i++;
		}

		if (i != events.end()) events.erase(i);
	}
	void remove_events(ClockDomainEventHandler cb,unsigned long long t_clk=CLOCKDOM_DONTCARE,unsigned long long val=CLOCKDOM_DONTCARE) {
		std::list<ClockDomainEvent>::iterator i;

		i=events.begin();
		while (i != events.end()) {
			if ((*i).cb == cb &&
				(t_clk == CLOCKDOM_DONTCARE || (*i).t_clock == t_clk) &&
				(val == CLOCKDOM_DONTCARE || (*i).value == val)) {
				if ((*i).in_progress)
					i++; /* skip over it, event handler will delete when callback finishes */
				else
					i = events.erase(i); /* NTS: stl::list.erase() is documented to return iterator to next element that followed the one you erased */
			}
			else {
				i++;
			}
		}
	}
	void remove_events_pic(PIC_EventHandler cb_pic,unsigned long long t_clk=CLOCKDOM_DONTCARE,unsigned long long val=CLOCKDOM_DONTCARE) {
		std::list<ClockDomainEvent>::iterator i;

		i=events.begin();
		while (i != events.end()) {
			if ((*i).cb_pic == cb_pic &&
				(t_clk == CLOCKDOM_DONTCARE || (*i).t_clock == t_clk) &&
				(val == CLOCKDOM_DONTCARE || (*i).value == val)) {
				if ((*i).in_progress)
					i++; /* skip over it, event handler will delete when callback finishes */
				else
					i = events.erase(i); /* NTS: stl::list.erase() is documented to return iterator to next element that followed the one you erased */
			}
			else {
				i++;
			}
		}
	}
	void add_event(ClockDomainEventHandler cb,unsigned long long t_clk,unsigned long long val=CLOCKDOM_DONTCARE) {
		std::list<ClockDomainEvent>::iterator i;
		ClockDomainEvent new_event;

		if (t_clk < counter) {
			LOG_MSG("Clock domain %s warning: attempt to add event prior to NOW\n",name.c_str());
			t_clk = counter + 1ULL; /* one-clock penalty */
		}

		new_event.t_clock = t_clk;
		new_event.cb = cb;
		new_event.value = val;
		new_event.domain = this;

		/* locate the slot where the event should be inserted.
		 * loop should terminate with iterator at end or just before the first item with
		 * a later t_clock than ours */
		i=events.begin();
		while (i != events.end() && (*i).t_clock <= t_clk) i++;
		events.insert(i,new_event); /* NTS: inserts new item BEFORE the element at iterator i */
	}
	void add_event_rel(ClockDomainEventHandler cb,unsigned long long t_clk,unsigned long long val=CLOCKDOM_DONTCARE) {
		add_event(cb,t_clk+counter,val);
	}
	void add_event_pic(PIC_EventHandler cb_pic,unsigned long long t_clk,unsigned long long val=CLOCKDOM_DONTCARE) {
		std::list<ClockDomainEvent>::iterator i;
		ClockDomainEvent new_event;

		if (t_clk < counter) {
			LOG_MSG("Clock domain %s warning: attempt to add event prior to NOW\n",name.c_str());
			t_clk = counter + 1ULL; /* one-clock penalty */
		}

		new_event.t_clock = t_clk;
		new_event.cb_pic = cb_pic;
		new_event.value = val;
		new_event.domain = this;

		/* locate the slot where the event should be inserted.
		 * loop should terminate with iterator at end or just before the first item with
		 * a later t_clock than ours */
		i=events.begin();
		while (i != events.end() && (*i).t_clock <= t_clk) i++;
		events.insert(i,new_event); /* NTS: inserts new item BEFORE the element at iterator i */
	}
	void add_event_rel_pic(PIC_EventHandler cb_pic,unsigned long long t_clk,unsigned long long val=CLOCKDOM_DONTCARE) {
		add_event_pic(cb_pic,t_clk+counter,val);
	}
	void remove_all_events() { /* remove all events. do not call them, just remove them */
		events.clear();
	}
	bool next_event_time(unsigned long long &t_next) {
		bool ret = false;

		if (events.begin() != events.end()) {
			t_next = events.front().t_clock;
			ret = true;
		}
		else {
			t_next = 0;
		}

		return ret;
	}
	void fire_events() {
		std::list<ClockDomainEvent>::iterator i;

		i = events.begin();
		while (i != events.end() && counter >= (*i).t_clock) {
			/* call the callback, erase the event, and reset the iterator to the new first entry to loop again */
			/* NTS: This code must tread carefully and assume that the callback will add another event to the list (or remove some even!).
			 *      As a linked list we're safe as long as the node we're holding onto still exists after the callback
			 *      and we do not hold onto any iterators or pointers to the next or previous entries */
			(*i).in_progress = true;
			if ((*i).cb_pic != NULL) (*i).cb_pic((Bitu)(*i).value);
			if ((*i).cb != NULL) (*i).cb(&(*i));
			(*i).in_progress = false;
			events.erase(i);
			i=events.begin();
		}
	}
public:
	/* NTS: Slave clock rules:
	 *       - Must have the same "freq" value as master
	 *       - Do not set clock time by floating point time (only the toplevel clocks in the tree should do that)
	 *       - Must rebase at the same reference time as the master
	 *       - Must maintain time according to master time divided by master's clock divider */
	unsigned long long		freq,freq_div;	/* frequency of clock as integer ratio */
	unsigned long long		rmaster_mult,rmaster_div; /* this clock * mult / div = master clock */
	unsigned long long		counter;	/* in units of freq */
	std::string			name;
	std::list<ClockDomainEvent>	events;		/* <- NTS: I'm tempted to use std::map<> but the most common use of this
							           event list will be to access the first entry to check if an
								   event is ready to fire (O(1) time) and it will happen far more
								   than the act of inserting events */
};

#endif //DOSBOX_CLOCKDOMAIN_H

