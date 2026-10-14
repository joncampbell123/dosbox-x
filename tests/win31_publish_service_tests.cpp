/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Matthew Hiltner
 */

#include "cpu.h"
#include "mem.h"
#include "paging.h"
#include "regs.h"
#include "vga.h"
#include "win31_publish.h"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <sstream>

void POD_Save_VGA_Draw(std::ostream &stream);
void POD_Load_VGA_Draw(std::istream &stream);

namespace {

class Win31PublishService : public ::testing::Test {
protected:
    static constexpr uint32_t Ram = 0x400000;
    static constexpr uint32_t Descriptor = Ram + 0xff0;
    static constexpr uint32_t Back = Ram + 0x4000;
    static constexpr uint32_t Cursor = Ram + 0x5000;
    static constexpr size_t RamBytes = 0xa000;
    VGA_Type saved_vga;
    bool saved_pmode;
    bool saved_paging;
    decltype(cpu.cpl) saved_cpl;
    uint32_t saved_flags;
    uint8_t display[32] = {};
    std::vector<uint8_t> saved_ram;

    void SetUp() override
    {
        ASSERT_GT(MEM_TotalPages(), (Ram + RamBytes) / 4096);
        saved_vga = vga;
        saved_pmode = cpu.pmode;
        saved_cpl = cpu.cpl;
        saved_flags = reg_flags;
        saved_paging = paging.enabled;
        saved_ram.assign(MemBase + Ram, MemBase + Ram + RamBytes);
        memset(MemBase + Ram, 0, RamBytes);
        cpu.pmode = true;
        cpu.cpl = 0;
        reg_flags &= ~FLAG_VM;
        paging.enabled = false;
        vga.mode = M_LIN8;
        vga.mem.linear = display;
        vga.mem.memsize = sizeof(display);
        vga.draw.linear_base = display;
        vga.draw.resizing = false;
        vga.config.display_start = 0;
        vga.draw.panning = 0;
        vga.draw.bytes_skip = 0;
        vga.draw.width = 8;
        vga.draw.height = 4;
        vga.draw.address_add = 8;
        const uint32_t fields[] = {40, 1, Back, 0, Cursor, 32, 8, 8, 4, 8};
        for (unsigned i = 0; i < 10; ++i)
            set_field(i, fields[i]);
        memset(MemBase + Back, 0x55, sizeof(display));
        VGA_Win31PublishCancel();
    }

    void TearDown() override
    {
        if (saved_ram.empty())
            return;
        VGA_Win31PublishCancel();
        memcpy(MemBase + Ram, saved_ram.data(), RamBytes);
        vga = saved_vga;
        cpu.pmode = saved_pmode;
        cpu.cpl = saved_cpl;
        reg_flags = saved_flags;
        paging.enabled = saved_paging;
    }

    void set_field(unsigned field, uint32_t value)
    {
        uint8_t *p = MemBase + Descriptor + field * 4;
        for (unsigned i = 0; i < 4; ++i)
            p[i] = uint8_t(value >> (i * 8));
    }
};

constexpr uint32_t Win31PublishService::Ram;
constexpr uint32_t Win31PublishService::Descriptor;
constexpr uint32_t Win31PublishService::Back;
constexpr uint32_t Win31PublishService::Cursor;
constexpr size_t Win31PublishService::RamBytes;

TEST_F(Win31PublishService, AcceptsPageCrossingDescriptorWithoutImmediateVramWrite)
{
    ASSERT_EQ(0u, VGA_Win31PublishSubmit(Descriptor));
    memset(MemBase + Back, 0x77, sizeof(display));
    for (auto pixel : display)
        EXPECT_EQ(0, pixel);
    VGA_Win31PublishCommit();
    for (auto pixel : display)
        EXPECT_EQ(0x55, pixel); // Submission owns the bytes as of its call.
    memset(display, 0x22, sizeof(display));
    VGA_Win31PublishCommit(); // Consumed snapshots cannot publish twice.
    EXPECT_EQ(0x22, display[0]);
}

TEST_F(Win31PublishService, ReplacesPendingSnapshotWithNewestSubmission)
{
    ASSERT_EQ(0u, VGA_Win31PublishSubmit(Descriptor));
    memset(MemBase + Back, 0x77, sizeof(display));
    ASSERT_EQ(0u, VGA_Win31PublishSubmit(Descriptor));
    VGA_Win31PublishCommit();
    for (auto pixel : display)
        EXPECT_EQ(0x77, pixel);
}

TEST_F(Win31PublishService, FailedSubmissionAndExplicitCancellationDiscardPendingPixels)
{
    ASSERT_EQ(0u, VGA_Win31PublishSubmit(Descriptor));
    set_field(1, 2);
    EXPECT_EQ(2u, VGA_Win31PublishSubmit(Descriptor));
    VGA_Win31PublishCommit();
    EXPECT_EQ(0, display[0]);
    set_field(1, 1);
    ASSERT_EQ(0u, VGA_Win31PublishSubmit(Descriptor));
    VGA_Win31PublishCommit();
    memset(MemBase + Back, 0x77, sizeof(display));
    ASSERT_EQ(0u, VGA_Win31PublishSubmit(Descriptor));
    VGA_Win31PublishCancel();
    for (auto pixel : display)
        EXPECT_EQ(0x55, pixel); // Cancellation preserves the committed frame.
    VGA_Win31PublishCommit();
    for (auto pixel : display)
        EXPECT_EQ(0x55, pixel); // The cancelled replacement cannot publish later.
}

TEST_F(Win31PublishService, GeometryChangeAndResizeDiscardPendingPixels)
{
    ASSERT_EQ(0u, VGA_Win31PublishSubmit(Descriptor));
    vga.draw.width = 7;
    VGA_Win31PublishCommit();
    EXPECT_EQ(0, display[0]);
    vga.draw.width = 8;
    VGA_Win31PublishCommit(); // Returning to the old geometry cannot revive it.
    EXPECT_EQ(0, display[0]);
    ASSERT_EQ(0u, VGA_Win31PublishSubmit(Descriptor));
    // An already pending resize avoids adding a PIC event to this test's state.
    vga.draw.resizing = true;
    VGA_StartResize(0);
    vga.draw.resizing = false;
    VGA_Win31PublishCommit();
    EXPECT_EQ(0, display[0]);
}

TEST_F(Win31PublishService, SaveStateLoadDiscardsHostPixelsFromPreviousGuestState)
{
    std::stringstream state;
    POD_Save_VGA_Draw(state);
    ASSERT_EQ(0u, VGA_Win31PublishSubmit(Descriptor));
    POD_Load_VGA_Draw(state);
    VGA_Win31PublishCommit();
    for (auto pixel : display)
        EXPECT_EQ(0, pixel);
}

TEST_F(Win31PublishService, RejectsUnsupportedContextAndMode)
{
    cpu.pmode = false;
    EXPECT_EQ(1u, VGA_Win31PublishSubmit(Descriptor));
    cpu.pmode = true;
    cpu.cpl = 3;
    EXPECT_EQ(1u, VGA_Win31PublishSubmit(Descriptor));
    cpu.cpl = 0;
    reg_flags |= FLAG_VM;
    EXPECT_EQ(1u, VGA_Win31PublishSubmit(Descriptor));
    reg_flags &= ~FLAG_VM;
    vga.draw.resizing = true;
    EXPECT_EQ(1u, VGA_Win31PublishSubmit(Descriptor));
    vga.draw.resizing = false;
    vga.config.display_start = 1;
    EXPECT_EQ(1u, VGA_Win31PublishSubmit(Descriptor));
    vga.config.display_start = 0;
    vga.mode = M_LIN16;
    EXPECT_EQ(1u, VGA_Win31PublishSubmit(Descriptor));
}

TEST_F(Win31PublishService, RejectsBadDescriptorMemoryAndCursor)
{
    set_field(1, 2);
    EXPECT_EQ(2u, VGA_Win31PublishSubmit(Descriptor));
    set_field(1, 1);
    EXPECT_EQ(3u, VGA_Win31PublishSubmit(0xfffff000u));
    EXPECT_EQ(3u, VGA_Win31PublishSubmit(0xf0000u)); // BIOS ROM is not RAM.
    set_field(2, 0xfffff000u);
    EXPECT_EQ(3u, VGA_Win31PublishSubmit(Descriptor));
    set_field(2, Back);
    MemBase[Cursor + 4] = 33;
    MemBase[Cursor + 6] = 1;
    MemBase[Cursor + Win31Publish::CursorBytes - 2] = 4;
    EXPECT_EQ(4u, VGA_Win31PublishSubmit(Descriptor));
}

} // namespace
