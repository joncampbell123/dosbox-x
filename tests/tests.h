#if C_DEBUG

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

#endif // DEBUG
