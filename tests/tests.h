#if C_DEBUG

#if !defined(_WIN32_WINDOWS)

// The following includes necessary gTest/gMock source files for unit tests.
// Launch DOSBox-X with -tests option in debug builds to run unit tests.

#include "gmock/gmock.cc"
#include "gmock/gmock-cardinalities.cc"
#include "gmock/gmock-internal-utils.cc"
#include "gmock/gmock-matchers.cc"
#include "gmock/gmock-spec-builders.cc"
#include "gtest/gtest.cc"
#include "gtest/gtest-death-test.cc"
#include "gtest/gtest-filepath.cc"
#include "gtest/gtest-matchers.cc"
#include "gtest/gtest-port.cc"
#include "gtest/gtest-printers.cc"
#include "gtest/gtest-test-part.cc"
#include "gtest/gtest-typed-test.cc"

// The following are source files containing unit tests.

#include "dos_files_tests.cpp"
#include "drives_tests.cpp"
#include "shell_cmds_tests.cpp"
#include "shell_redirection_tests.cpp"

#else
//google test code causes problem on win9x, remove them and add empty impelmentations for linkage.

#include <assert.h>
#include "gtest/gtest.h"
namespace testing {
void InitGoogleTest(int* argc, char** argv) {}

UnitTest* UnitTest::GetInstance() {static UnitTest ut; return &ut;}
UnitTest::UnitTest(){}
UnitTest::~UnitTest(){}
int UnitTest::Run() {return 0;}

namespace internal {
    Mutex::Mutex(){}
    Mutex::~Mutex(){}
}
}

#endif

#endif // DEBUG
