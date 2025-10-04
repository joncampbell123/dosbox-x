#ifndef DOSBOX_TEST_FIXTURE_H
#define DOSBOX_TEST_FIXTURE_H

#include <iterator>
#include <string>

#include <gtest/gtest.h>

#define SDL_MAIN_HANDLED

#include "control.h"

class DOSBoxTestFixture : public ::testing::Test {
public:
	DOSBoxTestFixture() {}
	void SetUp() override {}
	void TearDown() override {}
};

#endif
