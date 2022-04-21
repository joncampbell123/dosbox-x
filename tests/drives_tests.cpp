/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "../src/dos/drives.h"

#include <gtest/gtest.h>

#include <string>

std::string run_Set_Label(char const * const input, bool cdrom) {
    char output[32] = { 0 };
    Set_Label(input, output, cdrom);
    return std::string(output);
}

namespace {

TEST(WildFileCmp, ExactMatch)
{
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "TEST.EXE"));
    EXPECT_EQ(true, WildFileCmp("TEST", "TEST"));
    EXPECT_EQ(false, WildFileCmp("TEST.EXE", ".EXE"));
    EXPECT_EQ(true, WildFileCmp(".EXE", ".EXE"));
}

TEST(WildFileCmp, WildDotWild)
{
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "*.*"));
    EXPECT_EQ(true, WildFileCmp("TEST", "*.*"));
    EXPECT_EQ(true, WildFileCmp(".EXE", "*.*"));
}

TEST(WildFileCmp, WildcardNoExt)
{
    bool oldlfn = uselfn;
    uselfn = false;
    EXPECT_EQ(false, WildFileCmp("TEST.EXE", "*"));
    EXPECT_EQ(false, WildFileCmp(".EXE", "*"));
    EXPECT_EQ(false, WildFileCmp("TEST.BAK", "T*"));
    uselfn = true;
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "*"));
    EXPECT_EQ(true, WildFileCmp(".EXE", "*"));
    EXPECT_EQ(true, WildFileCmp("TEST.BAK", "T*"));
    uselfn = oldlfn;
    EXPECT_EQ(true, WildFileCmp("TEST", "*"));
    EXPECT_EQ(true, WildFileCmp("TEST", "T*"));
    EXPECT_EQ(false, WildFileCmp("TEST", "Z*"));
}

TEST(WildFileCmp, QuestionMark)
{
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "?EST.EXE"));
    EXPECT_EQ(true, WildFileCmp("TEST", "?EST"));
    EXPECT_EQ(false, WildFileCmp("TEST", "???Z"));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "TEST.???"));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "TEST.?XE"));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "???T.EXE"));
    EXPECT_EQ(true, WildFileCmp("TEST", "???T.???"));
}

TEST(LWildFileCmp, LFNCompare)
{
    bool oldlfn = uselfn;
    uselfn = true;
    EXPECT_EQ(false, LWildFileCmp("TEST", ""));
    EXPECT_EQ(true, LWildFileCmp("TEST.EXE", "*"));
    EXPECT_EQ(true, LWildFileCmp("TEST", "?EST"));
    EXPECT_EQ(false, LWildFileCmp("TEST", "???Z"));
    EXPECT_EQ(true, LWildFileCmp("TEST.EXE", "T*T.*"));
    EXPECT_EQ(true, LWildFileCmp("TEST.EXE", "T*T.?X?"));
    EXPECT_EQ(true, LWildFileCmp("TEST.EXE", "T??T.E*E"));
    EXPECT_EQ(true, LWildFileCmp("Test.exe", "*ST.E*"));
    EXPECT_EQ(true, LWildFileCmp("Test long name", "*NAME"));
    EXPECT_EQ(true, LWildFileCmp("Test long name", "*T*L*M*"));
    EXPECT_EQ(true, LWildFileCmp("Test long name.txt", "T*long*.T??"));
    EXPECT_EQ(true, LWildFileCmp("Test long name.txt", "??st*name.*t"));
    EXPECT_EQ(true, LWildFileCmp("Test long name.txt", "Test?long?????.*t"));
    EXPECT_EQ(true, LWildFileCmp("Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt", "Test*long.???"));
    EXPECT_EQ(true, LWildFileCmp("Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt", "Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt"));
    EXPECT_EQ(false, LWildFileCmp("Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt", "Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt"));
    EXPECT_EQ(false, LWildFileCmp("TEST", "Z*"));
    EXPECT_EQ(false, LWildFileCmp("TEST FILE NAME", "*Y*"));
    EXPECT_EQ(false, LWildFileCmp("TEST FILE NAME", "*F*X*"));
	uselfn = oldlfn;
}

/**
 * Set_Labels tests. These test the conversion of a FAT/CD-ROM volume
 * label to an MS-DOS 8.3 label with a variety of edge cases & oddities.
 */
TEST(Set_Label, Daggerfall)
{
    std::string output = run_Set_Label("Daggerfall", false);
    EXPECT_EQ("DAGGERFALL", output);
}
TEST(Set_Label, DaggerfallCD)
{
    std::string output = run_Set_Label("Daggerfall", true);
    EXPECT_EQ("Daggerfa.ll", output);
}

TEST(Set_Label, LongerThan11)
{
    std::string output = run_Set_Label("a123456789AAA", false);
    EXPECT_EQ("A123456789A", output);
}
TEST(Set_Label, LongerThan11CD)
{
    std::string output = run_Set_Label("a123456789AAA", true);
    EXPECT_EQ("a1234567.89A", output);
}

TEST(Set_Label, ShorterThan8)
{
    std::string output = run_Set_Label("a123456", false);
    EXPECT_EQ("A123456", output);
}
TEST(Set_Label, ShorterThan8CD)
{
    std::string output = run_Set_Label("a123456", true);
    EXPECT_EQ("a123456", output);
}

// Tests that the CD-ROM version adds a trailing dot when
// input is 8 chars plus one dot (9 chars total)
TEST(Set_Label, EqualTo8)
{
    std::string output = run_Set_Label("a1234567", false);
    EXPECT_EQ("A1234567", output);
}
TEST(Set_Label, EqualTo8CD)
{
    std::string output = run_Set_Label("a1234567", true);
    EXPECT_EQ("a1234567.", output);
}

// A test to ensure non-CD-ROM function strips trailing dot
TEST(Set_Label, StripEndingDot)
{
    std::string output = run_Set_Label("a1234567.", false);
    EXPECT_EQ("A1234567", output);
}
TEST(Set_Label, NoStripEndingDotCD)
{
    std::string output = run_Set_Label("a1234567.", true);
    EXPECT_EQ("a1234567.", output);
}

// Just to make sure this function doesn't clean invalid DOS labels
TEST(Set_Label, InvalidCharsEndingDot)
{
    std::string output = run_Set_Label("?*':&@(..", false);
    EXPECT_EQ("?*':&@(.", output);
}
TEST(Set_Label, InvalidCharsEndingDotCD)
{
    std::string output = run_Set_Label("?*':&@(..", true);
    EXPECT_EQ("?*':&@(..", output);
}

} // namespace
