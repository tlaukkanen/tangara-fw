#include "catch2/catch.hpp"

TEST_CASE("Example test case", "[cooltag]") {
    REQUIRE ( 1 == 1 );
}

TEST_CASE("test that doesn't run", "[cooltag][ignore]") {
    REQUIRE ( 0 == 1 );
}
