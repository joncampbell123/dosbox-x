#if C_DEBUG

// The following includes necessary GTest/GMock source files for unit tests.
// Launch DOSBox-X with -tests option in debug builds to run unit tests.

#include "../../tests/gmock/gmock.cc"
#include "../../tests/gmock/gmock-cardinalities.cc"
#include "../../tests/gmock/gmock-internal-utils.cc"
#include "../../tests/gmock/gmock-matchers.cc"
#include "../../tests/gmock/gmock-spec-builders.cc"
#include "../../tests/gtest/gtest.cc"
#include "../../tests/gtest/gtest-death-test.cc"
#include "../../tests/gtest/gtest-filepath.cc"
#include "../../tests/gtest/gtest-matchers.cc"
#include "../../tests/gtest/gtest-port.cc"
#include "../../tests/gtest/gtest-printers.cc"
#include "../../tests/gtest/gtest-test-part.cc"
#include "../../tests/gtest/gtest-typed-test.cc"

// The following are source files containing unit tests.

#include "../../tests/drives_tests.cpp"
#include "../../tests/shell_cmds_tests.cpp"

#endif // DEBUG
