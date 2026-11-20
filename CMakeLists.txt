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

add_library(nature_audio
    src/audio/soundscape.cpp
)

target_compile_features(nature_audio PUBLIC cxx_std_20)
target_include_directories(nature_audio PUBLIC include)
target_link_libraries(nature_audio PUBLIC nature_core)

if(MSVC)
    target_compile_options(nature_audio PRIVATE /W4 /fp:strict)
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_audio PRIVATE /WX)
    endif()
else()
    target_compile_options(nature_audio PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_audio PRIVATE -Werror)
    endif()
endif()

add_executable(naturecore_sim tools/naturecore_sim.cpp)
target_link_libraries(naturecore_sim PRIVATE nature_core)

add_executable(nature_preview_cli tools/nature_preview.cpp)
target_link_libraries(nature_preview_cli PRIVATE nature_preview)

add_executable(soundscape_render tools/soundscape_render.cpp)
target_link_libraries(soundscape_render PRIVATE nature_audio)

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

    add_library(nature_macos_audio
        src/platform/macos/audio_engine.mm
    )

    target_compile_features(nature_macos_audio PUBLIC cxx_std_20)
    target_include_directories(nature_macos_audio PUBLIC src/platform/macos)
    target_compile_options(nature_macos_audio PRIVATE
        -fobjc-arc
        -Wall
        -Wextra
        -Wpedantic
    )
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(nature_macos_audio PRIVATE -Werror)
    endif()
    target_link_libraries(nature_macos_audio PUBLIC
        nature_audio
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework AudioToolbox"
    )

    add_executable(natureview_metal
        src/platform/macos/metal_app.mm
        tools/macos/metal_main.cpp
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
        nature_preview
        nature_macos_audio
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
    )

    add_executable(natureview_audio_demo
        tools/macos/audio_live_demo.cpp
    )

    target_compile_features(natureview_audio_demo PRIVATE cxx_std_20)
    target_include_directories(natureview_audio_demo PRIVATE src/platform/macos)
    target_compile_options(natureview_audio_demo PRIVATE
        -fobjc-arc
        -Wall
        -Wextra
        -Wpedantic
    )
    if(NATUREVIEW_WARNINGS_AS_ERRORS)
        target_compile_options(natureview_audio_demo PRIVATE -Werror)
    endif()
    target_link_libraries(natureview_audio_demo PRIVATE nature_macos_audio)
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

    add_executable(audio_tests tests/audio_tests.cpp)
    target_link_libraries(audio_tests PRIVATE nature_audio)
    add_test(NAME audio_tests COMMAND audio_tests)

    if(WIN32)
        add_test(NAME dx12_probe COMMAND natureview_dx12_probe)
    endif()

    if(APPLE)
        add_test(
            NAME metal_scene_smoke
            COMMAND natureview_metal --mute --smoke --frames 3 --width 480 --height 270
        )
        set_tests_properties(metal_scene_smoke PROPERTIES TIMEOUT 30)

        add_test(
            NAME metal_scene_capture
            COMMAND natureview_metal
                --frames 1
                --seconds 30
                --width 320
                --height 180
                --mute
                --capture ${CMAKE_CURRENT_BINARY_DIR}/metal-scene.ppm
        )
        set_tests_properties(metal_scene_capture PROPERTIES
            FIXTURES_SETUP metal_scene_image
            TIMEOUT 30
        )

        add_executable(metal_capture_tests tests/metal_capture_tests.cpp)
        add_test(
            NAME metal_capture_tests
            COMMAND metal_capture_tests
                ${CMAKE_CURRENT_BINARY_DIR}/metal-scene.ppm
        )
        set_tests_properties(metal_capture_tests PROPERTIES
            FIXTURES_REQUIRED metal_scene_image
        )

        add_test(
            NAME metal_night_capture
            COMMAND natureview_metal
                --seed 22
                --frames 1
                --seconds 60
                --width 320
                --height 180
                --mute
                --capture ${CMAKE_CURRENT_BINARY_DIR}/metal-night.ppm
        )
        set_tests_properties(metal_night_capture PROPERTIES
            FIXTURES_SETUP metal_night_image
            TIMEOUT 30
        )

        add_test(
            NAME metal_night_capture_tests
            COMMAND metal_capture_tests
                ${CMAKE_CURRENT_BINARY_DIR}/metal-night.ppm
        )
        set_tests_properties(metal_night_capture_tests PROPERTIES
            FIXTURES_REQUIRED metal_night_image
        )
    endif()
endif()
