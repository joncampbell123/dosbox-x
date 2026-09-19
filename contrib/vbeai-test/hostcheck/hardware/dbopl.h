/* Stand-in for src/hardware/dbopl.h.
 *
 * Records every register write so the tests can assert on what the voice
 * allocator actually programmed, and keeps a register image for readback.
 * Mirrors the real DBOPL::Handler signatures; if those change upstream this
 * stops compiling, which is the intended signal. */
#ifndef H_DBOPL_STUB_H
#define H_DBOPL_STUB_H

#include "dosbox.h"
#include "mixer.h"
#include <vector>
#include <utility>

extern std::vector<std::pair<uint32_t,uint8_t> > OplWrites;
extern uint8_t OplRegs[512];

namespace DBOPL {

struct Handler {
    bool opl3mode;
    explicit Handler(bool opl3Mode) : opl3mode(opl3Mode) {}
    void WriteReg(uint32_t addr, uint8_t val);
    void Generate(MixerChannel* chan, Bitu samples) { (void)chan; (void)samples; }
    void Init(Bitu rate) { (void)rate; }
};

}
#endif
