/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "shell.h"

#include <cstdio>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cross.h"
#include "../src/dos/drives.h"
#include "dosbox_test_fixture.h"

namespace {

using namespace testing;

class DOS_Shell_REDIRTest : public DOSBoxTestFixture {};

class MockDOS_ShellRedir : public DOS_Shell {
public:
	/**
	 * NOTE: If we need to call the actual object, we use this. By
	 * default, the mocked functions return whatever we tell it to
	 * (if given a .WillOnce(Return(...)), or a default value
	 * (false).
	 */

private:
	DOS_Shell real_; // Keeps an instance of the real in the mock.
};

TEST_F(DOS_Shell_REDIRTest, CMD_Redirection)
{
	MockDOS_Shell shell;
	bool append = false;
	char line[CROSS_LEN];
	char * in = nullptr, * out = nullptr, * toc = nullptr;

	strcpy(line, "echo hello!");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "echo hello!");
	EXPECT_EQ(in, nullptr);
	EXPECT_EQ(out, nullptr);
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, "echo test>test.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "echo test");
	EXPECT_EQ(in, nullptr);
	EXPECT_STREQ(out, "test.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, "sort<test.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "sort");
	EXPECT_STREQ(in, "test.txt");
	EXPECT_EQ(out, nullptr);
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, "less<in.txt>out.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "less");
	EXPECT_STREQ(in, "in.txt");
	EXPECT_STREQ(out, "out.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, "more<file.txt|sort");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "more");
	EXPECT_STREQ(in, "file.txt");
	EXPECT_EQ(out, nullptr);
	EXPECT_STREQ(toc, "sort");
	EXPECT_EQ(append, false);

	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa<in.txt>>out.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	EXPECT_STREQ(in, "in.txt");
	EXPECT_STREQ(out, "out.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, true);

	append = false;
	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, "");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "");
	EXPECT_EQ(in, nullptr);
	EXPECT_EQ(out, nullptr);
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, " echo  test < in.txt > out.txt ");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, " echo  test   ");
	EXPECT_STREQ(in, "in.txt");
	EXPECT_STREQ(out, "out.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, "dir || more");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "dir ");
	EXPECT_EQ(in, nullptr);
	EXPECT_EQ(out,nullptr);
	EXPECT_STREQ(toc, "| more");
	EXPECT_EQ(append, false);

	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, "dir *.bat << in.txt >> out.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "dir *.bat  in.txt ");
	EXPECT_STREQ(in, "<");
	EXPECT_STREQ(out, "out.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, true);

	in = nullptr; out = nullptr; toc = nullptr;
	strcpy(line, "echo test>out1.txt>>out2.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "echo test");
	EXPECT_EQ(in, nullptr);
	EXPECT_STREQ(out, "out1.txt>>out2.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);
}

// Output redirection without a pipe writes the file. The pipe temporary file
// name is only set when there is a pipe, so it must not be looked at here
TEST_F(DOS_Shell_REDIRTest, ParseLine_Output_Redirection_Without_Pipe)
{
#if defined(WIN32)
	const char *tmp = getenv("TEMP");
	const std::string dir = std::string(tmp ? tmp : ".") + "\\dosbox-x-test";
	Cross::CreateDir(dir);
#else
	char tmpl[] = "/tmp/dosbox-x-test-XXXXXX";
	ASSERT_NE(mkdtemp(tmpl), nullptr);
	const std::string dir = tmpl;
#endif
	uint8_t drive = 2;
	while(drive < DOS_DRIVES - 1 && Drives[drive]) drive++;
	ASSERT_LT(drive, DOS_DRIVES - 1);
	std::vector<std::string> options;
	Drives[drive] = new localDrive((dir + CROSS_FILESPLIT).c_str(), 512, 32, 0, 0, 0xF8, options);

	DOS_Shell shell;
	char line[CROSS_LEN];
	snprintf(line, sizeof(line), "echo hello>%c:\\OUT.TXT", 'A' + drive);
	shell.ParseLine(line);

	DriveManager::UnmountDrive(drive);
	Drives[drive] = nullptr;
	const std::string file = dir + CROSS_FILESPLIT + "OUT.TXT";
	FILE *f = fopen(file.c_str(), "rb");
	ASSERT_NE(f, nullptr);
	char buf[16] = {};
	EXPECT_EQ(fread(buf, 1, sizeof(buf) - 1, f), 7u);
	fclose(f);
	EXPECT_STREQ(buf, "hello\r\n");
	remove(file.c_str());
	rmdir(dir.c_str());
}

} // namespace
