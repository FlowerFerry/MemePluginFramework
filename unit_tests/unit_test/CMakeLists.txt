cmake_minimum_required(VERSION 3.14)

project(mmpf_unittest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- meme_string (C library + memepp headers) from repo deps ---
set(MEMESTRING_UNITTEST_ENABLE OFF CACHE BOOL "" FORCE)
set(MEMESTRING_TEST_ENABLE     OFF CACHE BOOL "" FORCE)
set(MEMESTRING_BENCHMARK_ENABLE OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../deps/MemeString
                 ${CMAKE_BINARY_DIR}/meme_string_build)

# --- ww898/utf-cpp header-only library (error_message.h dependency) ---
include(FetchContent)
FetchContent_Declare(
    utf-cpp
    GIT_REPOSITORY https://github.com/ww898/utf-cpp.git
    GIT_TAG        v2.2.4
)
FetchContent_GetProperties(utf-cpp)
if (NOT utf-cpp_POPULATED)
    FetchContent_Populate(utf-cpp)
endif()

# --- Catch2 v2.13.9 single header (vendored copy, no network) ---
set(MMPF_CATCH2_SINGLE_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/catch2-v2.13.9-single_include
    CACHE PATH "Catch2 single_include directory")

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
