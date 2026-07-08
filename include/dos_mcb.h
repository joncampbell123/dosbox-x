#pragma once

#include <array>
#include <cstdint>

#include "dos_inc.h"

class DOS_MCB : public MemStruct
{
public:
	enum class MCBType : uint8_t
	{
		ValidBlock = 'M',
		LastBlock  = 'Z'
	};

	DOS_MCB(uint16_t seg) { SetPt(seg); }

	std::string GetFileName() const
    {
        std::array<char, 9> namebuf = {};
        MEM_BlockRead(pt+offsetof(sMCB,filename), namebuf.data(), 8);
        return {namebuf.data()};
    }
	uint16_t GetPSPSeg() const { return sGet(sMCB, psp_segment);}
    uint16_t GetSeg() const { return pt>>4; }
	uint16_t GetSize() const { return sGet(sMCB, size);}
	MCBType GetType() const { return MCBType(sGet(sMCB, type));}
    bool contains(uint16_t seg) const {
        const uint32_t start = GetSeg();
        const uint32_t end = start + GetSize();
        return (seg > start) && (seg <= end);
    }
    bool contains(uint16_t seg, uint16_t off) const {
        const PhysPt physaddr = PhysMake(seg, off);
        const PhysPt start = pt + 16u;
        const PhysPt end = pt + (PhysPt)((uint32_t(GetSize()) + 1u) << 4);
        return (physaddr >= start) && (physaddr < end);
    }
    bool isLastMCB() const { return (GetType()==MCBType::LastBlock); }
	bool isValid() const {
        return (GetType()==MCBType::ValidBlock)||(GetType()==MCBType::LastBlock);
    }
	DOS_MCB nextMCB() const {
	    const uint32_t nextseg = uint32_t(GetSeg()) + GetSize() + 1u;
	    return DOS_MCB(nextseg <= 0xFFFFu ? static_cast<uint16_t>(nextseg) : 0);
	}
    void SetFileName(const std::string& filename)
    {
        MEM_BlockWrite(
            pt+offsetof(sMCB,filename), filename.c_str(), std::min((size_t)8, filename.size()+1)
        );
    }
	void SetPSPSeg(uint16_t pspseg) { sSave(sMCB, psp_segment, pspseg);}
	void SetSize(uint16_t size) { sSave(sMCB, size, size);}
	void SetType(MCBType type) { sSave(sMCB, type, (uint8_t)type);}

    bool operator ==(const DOS_MCB& other) const { return pt == other.pt; }

private:
	#ifdef _MSC_VER
	#pragma pack (1)
	#endif
	struct sMCB {
		uint8_t type;
		uint16_t psp_segment;
		uint16_t size;
		uint8_t unused[3];
		uint8_t filename[8];
	} GCC_ATTRIBUTE(packed);
	#ifdef _MSC_VER
	#pragma pack ()
	#endif

public:
    class Iterator {
    public:
        explicit Iterator(uint16_t seg) : seg(seg) {}
        Iterator& operator++()
        {
            if (seg != 0) {
                DOS_MCB cur(seg);
                if (cur.isValid() && !cur.isLastMCB())
                    seg = static_cast<uint16_t>(cur.GetSeg() + cur.GetSize() + 1);
                else
                    seg = 0;
            }
            return *this;
        }
        DOS_MCB operator*() const { return DOS_MCB(seg); }
        bool operator==(const Iterator& o) const { return seg == o.seg; }
        bool operator!=(const Iterator& o) const { return !(*this == o); }
    private:
        uint16_t seg; // 0 means end/invalid
    };

    Iterator begin() const { return Iterator(GetSeg()); }
    Iterator end()   const { return Iterator(0); }

    // class Iterator
    // {
    // public:
    //     Iterator(const DOS_MCB& startMCB) : currentMCB(startMCB) {}
    //     Iterator& operator++()
    //     {
    //         if (currentMCB.isValid() && !currentMCB.isLastMCB())
    //             currentMCB = currentMCB.nextMCB();
    //         else
    //             currentMCB = DOS_MCB(0); // Invalidate iterator
    //         return *this;
    //     }
    //     const DOS_MCB& operator*() const
    //     {
    //         return currentMCB;
    //     }
    //     bool operator==(const Iterator& other) const
    //     {
    //         return currentMCB == other.currentMCB;
    //     }
    // private:
    //     DOS_MCB currentMCB;
    // };

    // Iterator begin() { return Iterator(*this); }
    // const Iterator cbegin() { return Iterator(*this); }
    // Iterator end() { return Iterator(DOS_MCB(0)); }
    // const Iterator cend() { return Iterator(DOS_MCB(0)); }
};