#include "dosbox.h"
#include "debug.h"
#include "logging.h"
#include "cpu.h"
#include "paging.h"

std::list<Breakpoint*> Breakpoint::BPoints;

Breakpoint* Breakpoint::AddBreakpoint(uint16_t seg, uint32_t off, bool once)
{
    Breakpoint* bp = new Breakpoint();
    bp->SetAddress(seg, off);
    bp->once = once;
    BPoints.push_front(bp);
    return bp;
}

void Breakpoint::SetAddress(uint16_t seg, uint32_t off) {
    location = (PhysPt)GetAddress(seg, off);
    type = BP_PHYSICAL;
    segment = seg;
    offset = off;
}

void Breakpoint::SetAddress(PhysPt adr) {
    location = adr;
    type = BP_PHYSICAL;
}

void Breakpoint::SetValue(uint8_t value) {
    ahValue = value;
}

void Breakpoint::SetOther(uint8_t other) {
    alValue = other;
}

bool Breakpoint::IsActive(void) {
    return active;
}

uint16_t Breakpoint::GetValue(void) {
    return ahValue;
}

uint16_t Breakpoint::GetOther(void) {
    return alValue;
}

void Breakpoint::SetInt(uint8_t _intNr, uint16_t ah, uint16_t al) {
    intNr = _intNr;
    ahValue = ah;
    alValue = al;
    type = BP_INTERRUPT;
}

Breakpoint* Breakpoint::AddIntBreakpoint(uint8_t intNum, uint16_t ah, uint16_t al, bool once)
{
    Breakpoint* bp = new Breakpoint();
    bp->SetInt(intNum, ah, al);
    bp->once = once;
    BPoints.push_front(bp);
    return bp;
}

Breakpoint* Breakpoint::AddMemBreakpoint(uint16_t seg, uint32_t off)
{
    Breakpoint* bp = new Breakpoint();
    bp->SetAddress(seg, off);
    bp->once = false;
    bp->type = BP_MEMORY;
    BPoints.push_front(bp);
    return bp;
}

void Breakpoint::ActivateBreakpoints()
{
    // activate all breakpoints
    std::list<Breakpoint*>::iterator i;
    for (i = BPoints.begin(); i != BPoints.end(); ++i)
        (*i)->Activate(true);
}

void Breakpoint::DeactivateBreakpoints()
{
    // deactivate all breakpoints
    std::list<Breakpoint*>::iterator i;
    for (i = BPoints.begin(); i != BPoints.end(); ++i)
        (*i)->Activate(false);
}

void Breakpoint::ActivateBreakpointsExceptAt(PhysPt adr)
{
    // activate all breakpoints, except those at adr
    std::list<Breakpoint*>::iterator i;
    for (i = BPoints.begin(); i != BPoints.end(); ++i) {
        Breakpoint* bp = (*i);
        // Do not activate breakpoints at adr
        if (bp->type == BP_PHYSICAL && bp->location == adr)
            continue;
        bp->Activate(true);
    }
}

bool Breakpoint::CheckBreakpoint(uint16_t seg, uint32_t off)
// Checks if breakpoint is valid and should stop execution
{
    // Quick exit if there are no breakpoints
    if (BPoints.empty()) return false;

    // Search matching breakpoint
    for (auto i = BPoints.begin(); i != BPoints.end(); ++i) {
        Breakpoint* bp = (*i);

        if ((bp->type == BP_PHYSICAL) && bp->IsActive() &&
            (bp->location == GetAddress(seg, off))) {
            // Found
            if (bp->once) {
                // delete it, if it should only be used once
                (BPoints.erase)(i);
                bp->Activate(false);
                delete bp;
            }
            else {
                // Also look for once-only breakpoints at this address
                bp = FindPhysBreakpoint(seg, off, true);
                if (bp) {
                    BPoints.remove(bp);
                    bp->Activate(false);
                    delete bp;
                }
            }
            return true;
        }
#if C_HEAVY_DEBUG
        // Memory breakpoint support
        else if (bp->IsActive()) {
            if ((bp->type == BP_MEMORY) || (bp->type == BP_MEMORY_PROT) || (bp->type == BP_MEMORY_LINEAR)) {
                // Watch Protected Mode Memoryonly in pmode
                if (bp->type == BP_MEMORY_PROT) {
                    // Check if pmode is active
                    if (!cpu.pmode) return false;
                    // Check if descriptor is valid
                    Descriptor desc;
                    if (!cpu.gdt.GetDescriptor(bp->segment, desc)) return false;
                    if (desc.GetLimit() == 0) return false;
                }

                Bitu address;
                if (bp->type == BP_MEMORY_LINEAR) address = bp->offset;
                else address = (Bitu)GetAddress(bp->segment, bp->offset);
                uint8_t value = 0;
                if (mem_readb_checked((PhysPt)address, &value)) return false;
                if (bp->GetValue() != value) {
                    // Yup, memory value changed
                    DEBUG_ShowMsg("DEBUG: Memory breakpoint %s: %04X:%04X - %02X -> %02X\n", (bp->type == BP_MEMORY_PROT) ? "(Prot)" : "", bp->segment, bp->offset, bp->GetValue(), value);
                    bp->SetValue(value);
                    return true;
                }
            }
        }
#endif
    }
    return false;
}

bool Breakpoint::CheckIntBreakpoint(PhysPt adr, uint8_t intNr, uint16_t ahValue, uint16_t alValue)
// Checks if interrupt breakpoint is valid and should stop execution
{
    if (BPoints.empty()) return false;

    // unused
    (void)adr;

    // Search matching breakpoint
    std::list<Breakpoint*>::iterator i;
    for (i = BPoints.begin(); i != BPoints.end(); ++i) {
        Breakpoint* bp = (*i);
        if ((bp->type == BP_INTERRUPT) && bp->IsActive() && (bp->intNr == intNr)) {
            if (((bp->GetValue() == BPINT_ALL) || (bp->GetValue() == ahValue)) && ((bp->GetOther() == BPINT_ALL) || (bp->GetOther() == alValue))) {
                // Ignore it once ?
                // Found
                if (bp->once) {
                    // delete it, if it should only be used once
                    (BPoints.erase)(i);
                    bp->Activate(false);
                    delete bp;
                }
                return true;
            }
        }
    }
    return false;
}

void Breakpoint::DeleteAll()
{
    std::list<Breakpoint*>::iterator i;
    for (i = BPoints.begin(); i != BPoints.end(); ++i) {
        Breakpoint* bp = (*i);
        bp->Activate(false);
        delete bp;
    }
    (BPoints.clear)();
}


bool Breakpoint::DeleteByIndex(uint16_t index)
{
    // Search matching breakpoint
    int nr = 0;
    std::list<Breakpoint*>::iterator i;
    Breakpoint* bp;
    for (i = BPoints.begin(); i != BPoints.end(); ++i) {
        if (nr == index) {
            bp = (*i);
            (BPoints.erase)(i);
            bp->Activate(false);
            delete bp;
            return true;
        }
        nr++;
    }
    return false;
}

Breakpoint* Breakpoint::FindPhysBreakpoint(uint16_t seg, uint32_t off, bool once)
{
    if (BPoints.empty()) return 0;
#if !C_HEAVY_DEBUG
    PhysPt adr = GetAddress(seg, off);
#endif
    // Search for matching breakpoint
    std::list<Breakpoint*>::iterator i;
    Breakpoint* bp;
    for (i = BPoints.begin(); i != BPoints.end(); ++i) {
        bp = (*i);
#if C_HEAVY_DEBUG
        // Heavy debugging breakpoints are triggered by matching seg:off
        bool atLocation = bp->segment == seg && bp->offset == off;
#else
        // Normal debugging breakpoints are triggered at an address
        bool atLocation = bp->location == adr;
#endif

        if (bp->type == BP_PHYSICAL && atLocation && bp->once == once)
            return bp;
    }

    return 0;
}

Breakpoint* Breakpoint::FindOtherActiveBreakpoint(PhysPt adr, Breakpoint* skip)
{
    std::list<Breakpoint*>::iterator i;
    for (i = BPoints.begin(); i != BPoints.end(); ++i) {
        Breakpoint* bp = (*i);
        if (bp != skip && bp->type == BP_PHYSICAL && bp->location == adr && bp->IsActive())
            return bp;
    }
    return 0;
}

// is there a permanent breakpoint at address ?
bool Breakpoint::IsBreakpoint(uint16_t seg, uint32_t off)
{
    return FindPhysBreakpoint(seg, off, false) != 0;
}

bool Breakpoint::DeleteBreakpoint(uint16_t seg, uint32_t off)
{
    Breakpoint* bp = FindPhysBreakpoint(seg, off, false);
    if (bp) {
        BPoints.remove(bp);
        delete bp;
        return true;
    }

    return false;
}

void Breakpoint::ShowList(void)
{
    // iterate list
    int nr = 0;
    std::list<Breakpoint*>::iterator i;
    for (i = BPoints.begin(); i != BPoints.end(); ++i) {
        Breakpoint* bp = (*i);
        if (bp->type == BP_PHYSICAL) {
            DEBUG_ShowMsg("%02X. BP %04X:%04X\n", nr, bp->segment, bp->offset);
        }
        else if (bp->type == BP_INTERRUPT) {
            if (bp->GetValue() == BPINT_ALL) DEBUG_ShowMsg("%02X. BPINT %02X\n", nr, bp->intNr);
            else if (bp->GetOther() == BPINT_ALL) DEBUG_ShowMsg("%02X. BPINT %02X AH=%02X\n", nr, bp->intNr, bp->GetValue());
            else DEBUG_ShowMsg("%02X. BPINT %02X AH=%02X AL=%02X\n", nr, bp->intNr, bp->GetValue(), bp->GetOther());
        }
        else if (bp->type == BP_MEMORY) {
            DEBUG_ShowMsg("%02X. BPMEM %04X:%04X (%02X)\n", nr, bp->segment, bp->offset, bp->GetValue());
        }
        else if (bp->type == BP_MEMORY_PROT) {
            DEBUG_ShowMsg("%02X. BPPM %04X:%08X (%02X)\n", nr, bp->segment, bp->offset, bp->GetValue());
        }
        else if (bp->type == BP_MEMORY_LINEAR) {
            DEBUG_ShowMsg("%02X. BPLM %08X (%02X)\n", nr, bp->offset, bp->GetValue());
        }
        nr++;
    }
}

Breakpoint::Breakpoint(void) : type(BP_UNKNOWN), location(0),
#if !C_HEAVY_DEBUG
    oldData(0xCC),
#endif
    segment(0), offset(0), intNr(0), ahValue(0), alValue(0), active(false), once(false) {
}

void Breakpoint::Activate(bool _active)
{
#if !C_HEAVY_DEBUG
    if (type == BKPNT_PHYSICAL) {
        if (_active) {
            // Set 0xCC and save old value
            uint8_t data = mem_readb(location);
            if (data != 0xCC) {
                oldData = data;
                mem_writeb(location, 0xCC);
            }
            else if (!active) {
                // Another activate breakpoint is already here.
                // Find it, and copy its oldData value
                CBreakpoint* bp = FindOtherActiveBreakpoint(location, this);

                if (!bp || bp->oldData == 0xCC) {
                    // This might also happen if there is a real 0xCC instruction here
                    DEBUG_ShowMsg("DEBUG: Internal error while activating breakpoint.\n");
                    oldData = 0xCC;
                }
                else
                    oldData = bp->oldData;
            };
        }
        else {
            if (mem_readb(location) == 0xCC) {
                if (oldData == 0xCC)
                    DEBUG_ShowMsg("DEBUG: Internal error while deactivating breakpoint.\n");

                // Check if we are the last active breakpoint at this location
                bool otherActive = (FindOtherActiveBreakpoint(location, this) != 0);

                // If so, remove 0xCC and set old value
                if (!otherActive)
                    mem_writeb(location, oldData);
            };
        }
    }
#endif
    active = _active;
}

