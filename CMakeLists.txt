cmake_minimum_required(VERSION 3.24)

project(
    NatureView
    VERSION 0.1.0
    DESCRIPTION "Real-time natural landscape runtime"
    LANGUAGES CXX
)

option(NATUREVIEW_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

add_library(nature_core
    src/camera.cpp
    src/environment.cpp
    src/noise.cpp
    src/simulation.cpp
    src/world.cpp
)

target_compile_features(nature_core PUBLIC cxx_std_20)
target_include_directories(nature_core PUBLIC include)

if(MSVC)
    target_compile_options(nature_core PRIVATE /W4 /fp:strict)
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_core PRIVATE /WX)
    endif()
else()
    target_compile_options(nature_core PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_core PRIVATE -Werror)
    endif()
endif()

add_library(nature_preview
    src/preview_renderer.cpp
)

target_compile_features(nature_preview PUBLIC cxx_std_20)
target_include_directories(nature_preview PUBLIC include)
target_link_libraries(nature_preview PUBLIC nature_core)

if(MSVC)
    target_compile_options(nature_preview PRIVATE /W4)
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_preview PRIVATE /WX)
    endif()
else()
    target_compile_options(nature_preview PRIVATE -Wall -Wextra -Wpedantic)
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_preview PRIVATE -Werror)
    endif()
endif()

add_executable(naturecore_sim tools/naturecore_sim.cpp)
target_link_libraries(naturecore_sim PRIVATE nature_core)

add_executable(nature_preview_cli tools/nature_preview.cpp)
target_link_libraries(nature_preview_cli PRIVATE nature_preview)

include(CTest)

if(BUILD_TESTING)
    add_executable(naturecore_tests tests/core_tests.cpp)
    target_link_libraries(naturecore_tests PRIVATE nature_core)
    add_test(NAME naturecore_tests COMMAND naturecore_tests)

    add_executable(preview_tests tests/preview_tests.cpp)
    target_link_libraries(preview_tests PRIVATE nature_preview)
    add_test(NAME preview_tests COMMAND preview_tests)
endif()
