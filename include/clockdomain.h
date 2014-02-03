
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

class ClockDomain {
public:
	ClockDomain() {
		freq = 0;
		freq_div = 1;
		master = true;
		master_clock = NULL;
	}
	ClockDomain(unsigned long long freq_new) {
		freq = freq_new;
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
	void set_time(double f) {
		f -= base_f;
		if (f < 0.0) {
			fprintf(stderr,"Clock domain %s warning: time went backwards below base\n",name.c_str());
			base_f = f;
			f = 0;
			notify_rebase();
			set_time((unsigned long long)0);
		}
		else {
			set_time((unsigned long long)floor(f*freq));
		}
	}
	void set_time(unsigned long long c) {
		unsigned long long adv;

		if (c >= counter) {
			advance(c - counter);
		}
		else if ((c+(freq/10ULL)) >= counter) { /* FIXME: Minor errors in PIC_FullIndex() will cause time to jump backwards
							          slightly.. which probably explains why truly time-sensitive stuff
								  like the warping FX and raster hacks in "The Good, The Bad, The Ugly"
								  has minor timing errors in DOSBox. */
			static bool jwarn=false;

			if (!jwarn) {
				fprintf(stderr,"Clock domain %s warning: time jumped slightly backwards (time errors from PIC_FullIndex()?)\n",name.c_str());
				jwarn=true;
			}
			/* ignore */
		}
		else {
			fprintf(stderr,"Clock domain %s warning: time went backwards\n",name.c_str());
			counter = c;
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
			fprintf(stderr,"Clock domain %s warning: attempting to add slave clock %s when said clock is already slave to someone else\n",name.c_str(),s->name.c_str());
			return;
		}

		for (size_t i=0;i < slaves.size();i++) {
			if (slaves[i] == s) {
				fprintf(stderr,"Clock domain %s warning: attempted to add slave clock %s again\n",name.c_str(),s->name.c_str());
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
public:
	unsigned long long		freq,freq_div;
	double				base_f;		/* base time if driven by floating point time */
	unsigned long long		counter;	/* in units of freq */
	unsigned long long		counter_whole;	/* in units of freq / freq_div */
	unsigned long long		counter_whole_snapshot;
	std::string			name;
	bool				master;
	ClockDomain*			master_clock;
	std::vector<ClockDomain*>	slaves;
};

