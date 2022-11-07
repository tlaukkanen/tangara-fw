#include "unity.h"

TEST_CASE("Example test case", "[cooltag]") {
    TEST_ASSERT_EQUAL(0, 0);
}

TEST_CASE("test that doesn't run", "[cooltag][ignore]") {
    TEST_ASSERT_EQUAL(0, 1);
}
