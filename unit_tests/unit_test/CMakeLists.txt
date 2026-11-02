cmake_minimum_required(VERSION 3.14)

project(mmpf_unittest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Bionic has no separate libpthread/librt, but meme_string links the literal
# name `rt` on every non-Windows platform. An INTERFACE target of that name
# makes the link line resolve to nothing.
if (ANDROID)
    add_library(rt INTERFACE)
endif()

# --- meme_string (C library + memepp headers) ---
set(MEMESTRING_UNITTEST_ENABLE  OFF CACHE BOOL "" FORCE)
set(MEMESTRING_TEST_ENABLE      OFF CACHE BOOL "" FORCE)
set(MEMESTRING_BENCHMARK_ENABLE OFF CACHE BOOL "" FORCE)

# A sibling working copy is used when present so both repos can be edited
# together; CI has no such copy and falls back to the pinned download.
set(MMPF_MEMESTRING_LOCAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/MemeString
    CACHE PATH "Local meme_string working copy, ignored when absent")
if (EXISTS ${MMPF_MEMESTRING_LOCAL_DIR}/CMakeLists.txt)
    set(MMPF_MEMESTRING_SOURCE_DIR ${MMPF_MEMESTRING_LOCAL_DIR})
    add_subdirectory(${MMPF_MEMESTRING_LOCAL_DIR} ${CMAKE_BINARY_DIR}/meme_string_build)
else()
    # Pinned to the develop revision the suite was verified against.
    set(MMPF_MEMESTRING_FETCH_DIR ${FETCHCONTENT_BASE_DIR}/mmpf-meme_string-src)
    FetchContent_Declare(
        MemeString
        GIT_REPOSITORY https://github.com/FlowerFerry/MemeString.git
        GIT_TAG        653256082b8a29b33609f03936a9c1a0202eeddc
        GIT_SUBMODULES ""
        GIT_PROGRESS TRUE
        SOURCE_DIR ${MMPF_MEMESTRING_FETCH_DIR}
    )
    FetchContent_MakeAvailable(MemeString)
    set(MMPF_MEMESTRING_SOURCE_DIR ${MMPF_MEMESTRING_FETCH_DIR})
endif()

# --- ww898/utf-cpp header-only library (error_message.h dependency) ---
FetchContent_Declare(
    utf-cpp
    GIT_REPOSITORY https://github.com/ww898/utf-cpp.git
    GIT_TAG        v2.2.4
)
FetchContent_GetProperties(utf-cpp)
if (NOT utf-cpp_POPULATED)
    FetchContent_Populate(utf-cpp)
endif()

# --- Catch2 v2.13.9 single header ---
# Vendored copy first, download only when it is absent.
set(MMPF_CATCH2_SINGLE_INCLUDE_LOCAL
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/catch2-v2.13.9-single_include
    CACHE PATH "Local Catch2 single_include directory, ignored when absent")
if (EXISTS ${MMPF_CATCH2_SINGLE_INCLUDE_LOCAL}/catch2/catch.hpp)
    set(MMPF_CATCH2_SINGLE_INCLUDE ${MMPF_CATCH2_SINGLE_INCLUDE_LOCAL})
else()
    set(MMPF_CATCH2_FETCH_DIR ${FETCHCONTENT_BASE_DIR}/mmpf-catch2-single_include)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v2.13.9
        GIT_SUBMODULES ""
        SOURCE_DIR ${MMPF_CATCH2_FETCH_DIR}
    )
    FetchContent_Populate(Catch2)
    set(MMPF_CATCH2_SINGLE_INCLUDE ${MMPF_CATCH2_FETCH_DIR}/single_include)
endif()

set(SRC_FILES
    main.cpp
    test_version.cpp
    test_ref_counter.cpp
    test_object_adapter.cpp
    test_plugin_registrar.cpp
    test_manager.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/MemeString/include
    ${utf-cpp_SOURCE_DIR}/include
    ${MMPF_CATCH2_SINGLE_INCLUDE}
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${MEMESTRING_PROJECT_NAME})

if (ANDROID)
    target_link_libraries(${PROJECT_NAME} PRIVATE log)
endif()

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /bigobj)
endif()

if (WIN32)
    # copy the meme_string DLL next to the test exe
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${MEMESTRING_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()

enable_testing()
add_test(NAME mmpf_unittest COMMAND ${PROJECT_NAME})
