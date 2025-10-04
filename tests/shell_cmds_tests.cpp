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

#include "shell.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "dosbox_test_fixture.h"

const char *GetCmdName(int i);

namespace {

using namespace testing;

class DOS_Shell_CMDSTest : public DOSBoxTestFixture {};

class MockDOS_Shell : public DOS_Shell {
public:
	/**
	 * NOTE: If we need to call the actual object, we use this. By
	 * default, the mocked functions return whatever we tell it to
	 * (if given a .WillOnce(Return(...)), or a default value
	 * (false).
	 *
	 * MockDOS_Shell()
	 * {
	 * 	// delegate call to the real object.
	 * 	ON_CALL(*this, execute_shell_cmd)
	 * 	        .WillByDefault([this](char *name, char *arguments) {
	 * 		        return real_.execute_shell_cmd(name, arguments);
	 * 	        });
	 * }
	 */
	MOCK_METHOD(bool, execute_shell_cmd, (char *name, char *arguments), (override));
	MOCK_METHOD(void, WriteOut, (const char *format, const char *arguments), (override));
	MOCK_METHOD(int, WriteOut_NoParsing, (const char * format, bool dbcs), (override));

private:
	DOS_Shell real_; // Keeps an instance of the real in the mock.
};

void assert_DoCommand(std::string input, std::string expected_name, std::string expected_args)
{
	MockDOS_Shell shell;
	char *input_c_str = const_cast<char *>(input.c_str());
	EXPECT_CALL(shell, execute_shell_cmd(StrEq(expected_name), StrEq(expected_args))).Times(1).WillOnce(Return(true));
	EXPECT_NO_THROW({ shell.DoCommand(input_c_str); });
}

// Tests chars that separate command name from arguments
TEST_F(DOS_Shell_CMDSTest, DoCommand_Separating_Chars)
{
	// These should all cause the parser to stop
	std::vector<char> end_chars{
	        32,
	        '/',
	        '\t',
	        '=',
	};
	for (auto end_chr : end_chars) {
		MockDOS_Shell shell;
		std::string name = "PATH";
		std::string args = "";
		std::string input = name;
		input += end_chr;
		input += "ARG";
		args += end_chr;
		args += "ARG";
		assert_DoCommand(input, name, args);
	}
}

TEST_F(DOS_Shell_CMDSTest, DoCommand_All_Cmds_Do_Valid_Execute)
{
	MockDOS_Shell shell;
	uint32_t cmd_index=0;
	while (GetCmdName(cmd_index)) {
		std::string input = GetCmdName(cmd_index);
		assert_DoCommand(input, input, "");
		cmd_index++;
	}
}

TEST_F(DOS_Shell_CMDSTest, DoCommand_Trim_Space)
{
	assert_DoCommand(" PATH ", "PATH", "");
}

TEST_F(DOS_Shell_CMDSTest, DoCommand_Splits_Cmd_and_Args)
{
	// NOTE: It does not strip the arguments!
	assert_DoCommand("DIR *.*", "DIR", " *.*");
}

TEST_F(DOS_Shell_CMDSTest, DoCommand_Doesnt_Split_Colon)
{
	// ensure we don't split on colon ...
	assert_DoCommand("C:", "C:", "");
	// ... but it does split on slash
	assert_DoCommand("C:\\", "C:", "\\");
}

TEST_F(DOS_Shell_CMDSTest, DoCommand_Nospace_Dot_Handling)
{
	assert_DoCommand("DIR.EXE", "DIR", ".EXE");
	assert_DoCommand("CD..", "CD", "..");
	assert_DoCommand("CD....", "CD", "....");
}

TEST_F(DOS_Shell_CMDSTest, DoCommand_Nospace_Slash_Handling)
{
	assert_DoCommand("CD\\DIRECTORY", "CD", "\\DIRECTORY");
	assert_DoCommand("CD\\", "CD", "\\");
}

TEST_F(DOS_Shell_CMDSTest, CMD_ECHO_off_on)
{
	MockDOS_Shell shell;
	EXPECT_TRUE(shell.echo); // should be the default
	EXPECT_CALL(shell, WriteOut_NoParsing(_, true)).Times(0);
	EXPECT_NO_THROW({ shell.CMD_ECHO(const_cast<char *>("OFF")); });
	EXPECT_FALSE(shell.echo);
	EXPECT_NO_THROW({ shell.CMD_ECHO(const_cast<char *>("ON")); });
	EXPECT_TRUE(shell.echo);
}

TEST_F(DOS_Shell_CMDSTest, CMD_ECHO_space_handling)
{
	MockDOS_Shell shell;

	EXPECT_TRUE(shell.echo);
	EXPECT_CALL(shell, WriteOut_NoParsing(StrEq("OFF "), true)).Times(1);
	// this DOES NOT trigger ECHO OFF (trailing space causes it to not)
	EXPECT_NO_THROW({ shell.CMD_ECHO(const_cast<char *>(" OFF ")); });
	EXPECT_TRUE(shell.echo);

	EXPECT_CALL(shell, WriteOut_NoParsing(StrEq("FF "), true)).Times(1);
	// this DOES NOT trigger ECHO OFF (initial 'O' gets stripped)
	EXPECT_NO_THROW({ shell.CMD_ECHO(const_cast<char *>("OFF ")); });
	EXPECT_TRUE(shell.echo);

	// no trailing space, echo off should work
	EXPECT_CALL(shell, WriteOut_NoParsing(_, true)).Times(0);
	EXPECT_NO_THROW({ shell.CMD_ECHO(const_cast<char *>(" OFF")); });
	// check that OFF worked properly, despite spaces
	EXPECT_FALSE(shell.echo);

	// NOTE: the expected string here is missing the leading char of the
	// input to ECHO. the first char is stripped as it's assumed it will be
	// a space, period or slash.
	EXPECT_CALL(shell, WriteOut_NoParsing(StrEq("    HI "), true)).Times(1);
	EXPECT_NO_THROW({ shell.CMD_ECHO(const_cast<char *>(".    HI ")); });
}

} // namespace
