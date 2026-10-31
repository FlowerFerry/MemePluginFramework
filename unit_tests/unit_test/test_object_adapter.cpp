
#include <catch2/catch.hpp>
#include <mmpfpp/internal/object_adapter.h>

#include <functional>

namespace {

struct dummy_src
{
    int value = 42;
};

struct dummy_adapter : public mmpfpp::internal::iplugin
{
    MMPF_ADAPTERCLASS_MEMBER(dummy_adapter, dummy_src)
};

struct dummy_factory : public mmpfpp::internal::object_adapter<dummy_adapter, dummy_src>
{
    virtual memepp::string app_type() const override { return "dummy.type"; }

    static dummy_factory& instance()
    {
        static dummy_factory f;
        return f;
    }
};

} // namespace

TEST_CASE("object_adapter - create_adapt constructs adapter over source object", "[object_adapter]")
{
    dummy_factory factory;
    dummy_src src;

    bool destroy_called = false;
    auto destroy_fn = [&](void* _p) -> MemeInteger_t
    {
        destroy_called = true;
        REQUIRE(_p == &src);
        return 0;
    };

    auto* adapted = factory.create_adapt(&src, destroy_fn);
    REQUIRE(adapted != nullptr);

    auto* typed = dynamic_cast<dummy_adapter*>(adapted);
    REQUIRE(typed != nullptr);

    // Destroying the adapter must invoke the destroy callback with the source pointer
    factory.destroy_adapt(adapted);
    REQUIRE(destroy_called);
}

TEST_CASE("object_adapter - destroy_adapt(nullptr) is a no-op returning 0", "[object_adapter]")
{
    dummy_factory factory;
    REQUIRE(factory.destroy_adapt(nullptr) == 0);
}

TEST_CASE("object_adapter - app_type round trip", "[object_adapter]")
{
    dummy_factory factory;
    REQUIRE(factory.app_type() == memepp::string("dummy.type"));
}

TEST_CASE("object_adapter - adapter without destroy function does not crash", "[object_adapter]")
{
    dummy_factory factory;
    dummy_src src;

    auto* adapted = factory.create_adapt(&src, std::function<mmpf_destroy_func_t>());
    REQUIRE(adapted != nullptr);
    factory.destroy_adapt(adapted);   // func_ empty: destructor skips call
}
