#include "catch2/catch.hpp"

TEST_CASE("Example test case", "[cooltag]") {
    REQUIRE ( 1 == 1 );
}

TEST_CASE("test that doesn't run", "[cooltag][!mayfail]") {
    REQUIRE ( 0 == 1 );
}
