
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
	}
public:
	unsigned long long		t_clock;	/* <- at what clock tick to fire (in freq ticks NOT freq/div) */
	unsigned long long		value;		/* <- this is up to the code assigning the event */
	ClockDomain*			domain;
	ClockDomainEventHandler		cb;
};

class ClockDomain {
public:
	ClockDomain() {
		freq = 0;
		freq_mul = 1;
		freq_div = 1;
		master = true;
		master_clock = NULL;
	}
	ClockDomain(unsigned long long freq_new) {
		freq = freq_new;
		freq_mul = 1;
		freq_div = 1;
		master = true;
		master_clock = NULL;
	}
	/* we allow non-integer frequencies as integer fractions.
	 * example: 33.3333333...MHz as 100,000,000Hz / 3 */
	ClockDomain(unsigned long long freq_new,unsigned long long div) {
		master_clock = NULL;
		freq = freq_new;
		freq_div = div;
		master = true;
		freq_mul = 1;
	}
public:
	void set_name(const char *s) {
		name = s;
	}
	void set_frequency(unsigned long long freq_new,unsigned long long div_new) {
		counter = 0;
		freq = freq_new;
		freq_div = div_new;
	}
	const char *get_name() {
		return name.c_str();
	}
	void set_base(double f) {
		counter = 0;
		base_f = f;
	}
	double clocks_to_time(unsigned long long t) {
		double f = (double)t / freq;
		return f + base_f;
	}
	unsigned long long time_to_clocks(double f) {
		f -= base_f;
		if (f < 0.0) {
			LOG_MSG("Clock domain %s warning: time went backwards below base\n",name.c_str());
			base_f = f;
			f = 0;
			notify_rebase();
			return 0ULL;
		}
		else {
			return (unsigned long long)floor(f*freq);
		}
	}
	void advance(unsigned long long c) { /* where C is units of freq * freq_div */
		unsigned long long wadv = (counter % freq_div) + c;

		counter += c;
		if (wadv >= freq_div) {
			counter_whole += wadv/freq_div;
			notify_advance(wadv/freq_div);
		}
	}
	virtual void notify_advance(unsigned long long wc) { /* override me! */
		for (size_t i=0;i < slaves.size();i++) slaves[i]->advance(wc);
	}
	virtual void notify_rebase() { /* override me! */
		for (size_t i=0;i < slaves.size();i++) slaves[i]->notify_rebase();
	}
	void add_slave(ClockDomain *s) {
		assert(s != NULL);
		if (s->master_clock != NULL) {
			LOG_MSG("Clock domain %s warning: attempting to add slave clock %s when said clock is already slave to someone else\n",name.c_str(),s->name.c_str());
			return;
		}

		for (size_t i=0;i < slaves.size();i++) {
			if (slaves[i] == s) {
				LOG_MSG("Clock domain %s warning: attempted to add slave clock %s again\n",name.c_str(),s->name.c_str());
				return;
			}
		}

		s->master = false;
		s->master_clock = this;
		slaves.push_back(s);
	}
	void snapshot() {
		counter_whole_snapshot = counter_whole;
		for (size_t i=0;i < slaves.size();i++) slaves[i]->snapshot();
	}
	void remove_event(ClockDomainEventHandler cb,unsigned long long t_clk=CLOCKDOM_DONTCARE,unsigned long long val=CLOCKDOM_DONTCARE) {
		std::list<ClockDomainEvent>::iterator i;

		i=events.begin();
		while (i != events.end()) {
			if ((*i).cb == cb &&
				(t_clk == CLOCKDOM_DONTCARE || (*i).t_clock == t_clk) &&
				(val == CLOCKDOM_DONTCARE || (*i).value == val)) {
				break; /* found it */
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
	void remove_all_events() { /* remove all events. do not call them, just remove them */
		events.clear();
	}
	bool next_event_time(unsigned long long &t_next) {
		unsigned long long t_slave;
		bool ret = false;

		if (events.size() != 0) {
			t_next = events.front().t_clock;
			ret = true;
		}
		else {
			t_next = 0;
		}

		/* and if slave clocks have their own, then report that too */
		for (size_t i=0;i < slaves.size();i++) {
			if (slaves[i]->next_event_time(t_slave)) {
				t_slave *= freq_div;
				if (ret == false || t_next > t_slave) {
					t_next = t_slave;
					ret = true;
				}
			}
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
			(*i).cb(&(*i));
			events.erase(i);
			i=events.begin();
		}

		/* then let the slaves fire their events */
		for (size_t i=0;i < slaves.size();i++) slaves[i]->fire_events();
	}
public:
	/* NTS: Slave clock rules:
	 *       - Must have the same "freq" value as master
	 *       - Do not set clock time by floating point time (only the toplevel clocks in the tree should do that)
	 *       - Must rebase at the same reference time as the master
	 *       - Must maintain time according to master time divided by master's clock divider */
	unsigned long long		freq,freq_mul,freq_div;	/* NTS: For slave clocks this code assumes freq value is the same, divides only by freq_div */
	double				base_f;		/* base time if driven by floating point time */
	unsigned long long		counter;	/* in units of freq */
	unsigned long long		counter_whole;	/* in units of freq / freq_div */
	unsigned long long		counter_whole_snapshot;
	std::string			name;
	bool				master;
	ClockDomain*			master_clock;
	std::vector<ClockDomain*>	slaves;
	std::list<ClockDomainEvent>	events;		/* <- NTS: I'm tempted to use std::map<> but the most common use of this
							           event list will be to access the first entry to check if an
								   event is ready to fire (O(1) time) and it will happen far more
								   than the act of inserting events */
};

