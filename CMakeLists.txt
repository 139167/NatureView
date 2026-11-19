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
    src/render_scene.cpp
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

if(WIN32)
    add_library(nature_dx12
        src/platform/windows/dx12_device.cpp
    )

    target_compile_features(nature_dx12 PUBLIC cxx_std_20)
    target_include_directories(nature_dx12 PRIVATE src/platform/windows)
    target_compile_definitions(nature_dx12 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(nature_dx12 PRIVATE /W4 /fp:strict)
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_dx12 PRIVATE /WX)
    endif()
    target_link_libraries(nature_dx12 PUBLIC d3d12 dxgi)

    add_executable(natureview_dx12_probe tools/windows/dx12_probe.cpp)
    target_include_directories(natureview_dx12_probe PRIVATE src/platform/windows)
    target_link_libraries(natureview_dx12_probe PRIVATE nature_dx12)
endif()

if(APPLE)
    enable_language(OBJCXX)

    add_executable(natureview_metal
        src/platform/macos/metal_clear_app.mm
        tools/macos/metal_clear_main.cpp
    )

    target_compile_features(natureview_metal PRIVATE cxx_std_20)
    target_include_directories(natureview_metal PRIVATE src/platform/macos)
    target_compile_options(natureview_metal PRIVATE
        -fobjc-arc
        -Wall
        -Wextra
        -Wpedantic
    )
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(natureview_metal PRIVATE -Werror)
    endif()
    target_link_libraries(natureview_metal PRIVATE
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
    )
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(naturecore_tests tests/core_tests.cpp)
    target_link_libraries(naturecore_tests PRIVATE nature_core)
    add_test(NAME naturecore_tests COMMAND naturecore_tests)

    add_executable(preview_tests tests/preview_tests.cpp)
    target_link_libraries(preview_tests PRIVATE nature_preview)
    add_test(NAME preview_tests COMMAND preview_tests)

    add_executable(render_scene_tests tests/render_scene_tests.cpp)
    target_link_libraries(render_scene_tests PRIVATE nature_preview)
    add_test(NAME render_scene_tests COMMAND render_scene_tests)

    if(WIN32)
        add_test(NAME dx12_probe COMMAND natureview_dx12_probe)
    endif()

    if(APPLE)
        add_test(
            NAME metal_clear_smoke
            COMMAND natureview_metal --smoke --frames 3 --width 480 --height 270
        )
        set_tests_properties(metal_clear_smoke PROPERTIES TIMEOUT 30)
    endif()
endif()
