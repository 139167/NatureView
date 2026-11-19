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
    target_compile_options(nature_core PRIVATE /W4)
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_core PRIVATE /WX)
    endif()
else()
    target_compile_options(nature_core PRIVATE -Wall -Wextra -Wpedantic)
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_core PRIVATE -Werror)
    endif()
endif()

add_executable(naturecore_sim tools/naturecore_sim.cpp)
target_link_libraries(naturecore_sim PRIVATE nature_core)

include(CTest)

if(BUILD_TESTING)
    add_executable(naturecore_tests tests/core_tests.cpp)
    target_link_libraries(naturecore_tests PRIVATE nature_core)
    add_test(NAME naturecore_tests COMMAND naturecore_tests)
endif()
