
#include <catch2/catch.hpp>
#include <mmpfpp/external/plugin_registrar.h>

#include <cstring>
#include <string>

namespace {

// Captured state from the fake manager-side callbacks
struct capture_t
{
    mmpf_build_info_t info{};
    std::string       object_id;
    mmpf_register_params reg{};
    int info_calls   = 0;
    int object_calls = 0;
};

MemeInteger_t fake_register_info(mmpf_manage_ptr, const mmpf_build_info_t* _info, mmint_t)
{
    // intentionally copy raw pointers; strings are static literals
    return 0;
}

capture_t g_cap;

MemeInteger_t fake_register_info_capture(mmpf_manage_ptr, const mmpf_build_info_t* _info, mmint_t _stsize)
{
    ++g_cap.info_calls;
    REQUIRE(_stsize >= (mmint_t)sizeof(mmpf_build_info_t));
    g_cap.info = *_info;
    return 0;
}

MemeInteger_t fake_register_object_capture(mmpf_manage_ptr, const uint8_t* _id, mmint_t _len,
                                           const mmpf_register_params* _params, mmint_t _stsize)
{
    ++g_cap.object_calls;
    REQUIRE(_stsize >= (mmint_t)sizeof(mmpf_register_params));
    g_cap.object_id.assign((const char*)_id, _len);
    g_cap.reg = *_params;
    return 0;
}

// Interface helper required by plugin_registrar::register_object
struct helper_noop
{
    static void* create_func(mmpf_obj_params*, mmint_t) { return nullptr; }
    static MemeInteger_t destroy_func(void*) { return 0; }
};

mmpf_init_params_t make_params(mmpf_init_params_t& p)
{
    static mmpf_manage_services_t services{};
    services.manage = reinterpret_cast<mmpf_manage_ptr>(0x1); // non-null sentinel
    services.version = MMPF_VER_NUMBER;
    p.manage_services = &services;
    p.register_object = fake_register_object_capture;
    p.register_plugin_info = fake_register_info_capture;
    return p;
}

} // namespace

TEST_CASE("plugin_registrar - init with null params returns MGEC__INVAL", "[registrar]")
{
    mmpfpp::external::plugin_registrar reg(nullptr, sizeof(mmpf_init_params));
    REQUIRE(reg.init() == MGEC__INVAL);
}

TEST_CASE("plugin_registrar - init with too-small struct size returns MGEC__INVAL", "[registrar]")
{
    mmpf_init_params_t p{};
    make_params(p);
    mmpfpp::external::plugin_registrar reg(&p, sizeof(mmpf_init_params) - 1);
    REQUIRE(reg.init() == MGEC__INVAL);
}

TEST_CASE("plugin_registrar - init without register_plugin_info returns MGEC__CANCELED", "[registrar]")
{
    mmpf_init_params_t p{};
    make_params(p);
    p.register_plugin_info = nullptr;
    mmpfpp::external::plugin_registrar reg(&p, sizeof(mmpf_init_params));
    REQUIRE(reg.init() == MGEC__CANCELED);
}

TEST_CASE("plugin_registrar - init success forwards build info", "[registrar]")
{
    g_cap = capture_t{};
    mmpf_init_params_t p{};
    make_params(p);
    mmpfpp::external::plugin_registrar reg(&p, sizeof(mmpf_init_params));

    REQUIRE(reg.init() == 0);
    REQUIRE(g_cap.info_calls == 1);
    REQUIRE(g_cap.info.version == MMPF_VER_NUMBER);
    REQUIRE(g_cap.info.ifacelang == mmpf_ifacelang_C);
    REQUIRE(g_cap.info.build_date_slen == (mmint_t)(sizeof(__DATE__) - 1));
    REQUIRE(g_cap.info.build_time_slen == (mmint_t)(sizeof(__TIME__) - 1));
}

TEST_CASE("plugin_registrar - register_object with null params returns MGEC__INVAL", "[registrar]")
{
    mmpfpp::external::plugin_registrar reg(nullptr, sizeof(mmpf_init_params));
    REQUIRE(reg.register_object<helper_noop>("obj", (const MemeByte_t*)"type", 4) == MGEC__INVAL);
}

TEST_CASE("plugin_registrar - register_object without callback returns MGEC__CANCELED", "[registrar]")
{
    mmpf_init_params_t p{};
    make_params(p);
    p.register_object = nullptr;
    mmpfpp::external::plugin_registrar reg(&p, sizeof(mmpf_init_params));
    REQUIRE(reg.register_object<helper_noop>("obj", (const MemeByte_t*)"type", 4) == MGEC__CANCELED);
}

TEST_CASE("plugin_registrar - register_object success forwards name, app_type and funcs", "[registrar]")
{
    g_cap = capture_t{};
    mmpf_init_params_t p{};
    make_params(p);
    mmpfpp::external::plugin_registrar reg(&p, sizeof(mmpf_init_params));

    static const char kType[] = "my.app.type";
    REQUIRE(reg.register_object<helper_noop>("my.object",
        (const MemeByte_t*)kType, sizeof(kType) - 1) == 0);

    REQUIRE(g_cap.object_calls == 1);
    REQUIRE(g_cap.object_id == "my.object");
    REQUIRE(g_cap.reg.app_type_slen == (mmint_t)(sizeof(kType) - 1));
    REQUIRE(std::memcmp(g_cap.reg.app_type, kType, sizeof(kType) - 1) == 0);
    REQUIRE(g_cap.reg.create_func == helper_noop::create_func);
    REQUIRE(g_cap.reg.destroy_func == helper_noop::destroy_func);
}
