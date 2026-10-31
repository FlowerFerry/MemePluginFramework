
#include <catch2/catch.hpp>
#include <meme/string_fwd.h>   // ref_counter.h uses MemeInteger_t without including it
#include <mmpfpp/internal/ref_counter.h>

#include <mutex>

using mmpfpp::internal::ref_counter;

TEST_CASE("ref_counter - default construction count=0 compare=0", "[ref_counter]")
{
    ref_counter<> rc;
    REQUIRE(rc.count_ == 0);
    REQUIRE(rc.compare_value_ == 0);
}

TEST_CASE("ref_counter - value constructor sets count", "[ref_counter]")
{
    ref_counter<> rc{5};
    REQUIRE(rc.count_ == 5);
    REQUIRE(rc.compare_value_ == 0);
}

TEST_CASE("ref_counter - increment via mutex reference", "[ref_counter]")
{
    std::mutex mtx;
    ref_counter<std::mutex> rc{0};
    rc.increment(mtx);
    rc.increment(mtx);
    REQUIRE(rc.count_ == 2);
}

TEST_CASE("ref_counter - increment via unique_lock", "[ref_counter]")
{
    std::mutex mtx;
    ref_counter<std::mutex> rc{0};
    std::unique_lock<std::mutex> locker(mtx);
    rc.increment(locker);
    REQUIRE(rc.count_ == 1);
    // locker must be re-locked by scope_unique_locker after the call
    REQUIRE(locker.owns_lock());
}

TEST_CASE("ref_counter - decrement via mutex reference", "[ref_counter]")
{
    std::mutex mtx;
    ref_counter<std::mutex> rc{3};
    rc.decrement(mtx);
    REQUIRE(rc.count_ == 2);
}

TEST_CASE("ref_counter - decrement fires callback when crossing compare_value", "[ref_counter]")
{
    std::mutex mtx;
    ref_counter<std::mutex> rc{1};

    int fired = 0;
    rc.set_callback([&](const ref_counter<std::mutex>& c)
    {
        ++fired;
        REQUIRE(c.count_ == 0);
    });

    rc.decrement(mtx);
    REQUIRE(fired == 1);
    REQUIRE(rc.count_ == 0);

    // Already at compare_value: further decrement must not fire again
    // (condition requires count > compare_value on the pre-decrement value)
    rc.decrement(mtx);
    REQUIRE(fired == 1);
}

TEST_CASE("ref_counter - decrement with compare_value != 0 fires at threshold", "[ref_counter]")
{
    std::mutex mtx;
    ref_counter<std::mutex> rc{3};
    rc.compare_value_ = 2;

    int fired = 0;
    rc.set_callback([&](const ref_counter<std::mutex>&) { ++fired; });

    rc.decrement(mtx);          // 3 -> 2, crosses threshold
    REQUIRE(fired == 1);
    rc.decrement(mtx);          // 2 -> 1, pre-value not above threshold
    REQUIRE(fired == 1);
}

TEST_CASE("ref_counter - no callback set, decrement at zero is safe", "[ref_counter]")
{
    std::mutex mtx;
    ref_counter<std::mutex> rc{0};
    rc.decrement(mtx);
    REQUIRE(rc.count_ == -1);
}

TEST_CASE("ref_counter - is_meet true when count <= compare_value", "[ref_counter]")
{
    std::mutex mtx;
    ref_counter<std::mutex> rc{0};
    std::unique_lock<std::mutex> locker(mtx);
    REQUIRE(rc.is_meet(locker));
    // caller-owned lock stays owned after the call
    REQUIRE(locker.owns_lock());
}

TEST_CASE("ref_counter - is_meet false when count > compare_value", "[ref_counter]")
{
    std::mutex mtx;
    ref_counter<std::mutex> rc{2};
    std::unique_lock<std::mutex> locker(mtx);
    REQUIRE_FALSE(rc.is_meet(locker));
    REQUIRE(locker.owns_lock());
}

TEST_CASE("ref_counter - set_callback with empty function keeps previous callback", "[ref_counter]")
{
    std::mutex mtx;
    ref_counter<std::mutex> rc{1};

    int fired = 0;
    rc.set_callback([&](const ref_counter<std::mutex>&) { ++fired; });
    rc.set_callback(nullptr);   // empty std::function: ignored, old cb kept

    rc.decrement(mtx);
    REQUIRE(fired == 1);
}
