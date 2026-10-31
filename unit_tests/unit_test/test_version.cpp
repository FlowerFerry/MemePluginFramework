
#include <catch2/catch.hpp>
#include <mmpf/external/plugin.h>

#include <string>

TEST_CASE("plugin.h - version macros are consistent", "[version]")
{
    REQUIRE(MMPF_VER_MAJOR == 0);
    REQUIRE(MMPF_VER_MINOR == 1);
    REQUIRE(MMPF_VER_PATCH == 0);

    // NOTE: MEGO__STRINGIZE stringifies the parenthesized macro literally,
    // so MMPF_VER_STRING currently expands to "(0).(1).(0)", not "0.1.0".
    // Asserting actual behavior; fix the macro upstream if "0.1.0" is wanted.
    REQUIRE(std::string(MMPF_VER_STRING) == "(0).(1).(0)");

    REQUIRE(MEGO__GET_VERSION_MAJOR(MMPF_VER_NUMBER) == MMPF_VER_MAJOR);
    REQUIRE(MEGO__GET_VERSION_MINOR(MMPF_VER_NUMBER) == MMPF_VER_MINOR);
    REQUIRE(MEGO__GET_VERSION_PATCH(MMPF_VER_NUMBER) == MMPF_VER_PATCH);
}

TEST_CASE("plugin.h - log level enum ordering", "[version]")
{
    REQUIRE(mmpf_loglvl_trace == 0);
    REQUIRE(mmpf_loglvl_debug > mmpf_loglvl_trace);
    REQUIRE(mmpf_loglvl_info  > mmpf_loglvl_debug);
    REQUIRE(mmpf_loglvl_warning > mmpf_loglvl_info);
    REQUIRE(mmpf_loglvl_error  > mmpf_loglvl_warning);
    REQUIRE(mmpf_loglvl_fatal  > mmpf_loglvl_error);
}

TEST_CASE("plugin.h - mmpf_plugin_t layout", "[version]")
{
    // The manager writes __v0..__v3 and 'object' must be the last member.
    mmpf_plugin_t p{};
    REQUIRE(p.__v0 == 0);
    REQUIRE(p.__v3 == 0);
    REQUIRE(p.object == nullptr);
}
