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

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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
	char * in = 0, * out = 0, * toc = 0;

	strcpy(line, "echo hello!");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "echo hello!");
	EXPECT_EQ(in, nullptr);
	EXPECT_EQ(out, nullptr);
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = 0; out = 0; toc = 0;
	strcpy(line, "echo test>test.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "echo test");
	EXPECT_EQ(in, nullptr);
	EXPECT_STREQ(out, "test.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = 0; out = 0; toc = 0;
	strcpy(line, "sort<test.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "sort");
	EXPECT_STREQ(in, "test.txt");
	EXPECT_EQ(out, nullptr);
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = 0; out = 0; toc = 0;
	strcpy(line, "less<in.txt>out.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "less");
	EXPECT_STREQ(in, "in.txt");
	EXPECT_STREQ(out, "out.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = 0; out = 0; toc = 0;
	strcpy(line, "more<file.txt|sort");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "more");
	EXPECT_STREQ(in, "file.txt");
	EXPECT_EQ(out, nullptr);
	EXPECT_STREQ(toc, "sort");
	EXPECT_EQ(append, false);

	in = 0; out = 0; toc = 0;
	strcpy(line, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa<in.txt>>out.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	EXPECT_STREQ(in, "in.txt");
	EXPECT_STREQ(out, "out.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, true);

	append = false;
	in = 0; out = 0; toc = 0;
	strcpy(line, "");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "");
	EXPECT_EQ(in, nullptr);
	EXPECT_EQ(out, nullptr);
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = 0; out = 0; toc = 0;
	strcpy(line, " echo  test < in.txt > out.txt ");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, " echo  test   ");
	EXPECT_STREQ(in, "in.txt");
	EXPECT_STREQ(out, "out.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);

	in = 0; out = 0; toc = 0;
	strcpy(line, "dir || more");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "dir ");
	EXPECT_EQ(in, nullptr);
	EXPECT_EQ(out,nullptr);
	EXPECT_STREQ(toc, "| more");
	EXPECT_EQ(append, false);

	in = 0; out = 0; toc = 0;
	strcpy(line, "dir *.bat << in.txt >> out.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "dir *.bat  in.txt ");
	EXPECT_STREQ(in, "<");
	EXPECT_STREQ(out, "out.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, true);

	in = 0; out = 0; toc = 0;
	strcpy(line, "echo test>out1.txt>>out2.txt");
	shell.GetRedirection(line, &in, &out, &toc, &append);
	EXPECT_STREQ(line, "echo test");
	EXPECT_EQ(in, nullptr);
	EXPECT_STREQ(out, "out1.txt>>out2.txt");
	EXPECT_EQ(toc, nullptr);
	EXPECT_EQ(append, false);
}

} // namespace
