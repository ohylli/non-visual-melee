# SPDX-License-Identifier: GPL-3.0-or-later
# Accessibility fork: speech, its screen reader library (Prism), and the
# readers that turn RmlUi pages and the game's screens into speech.
# Included from the root CMakeLists.txt with one line, after the melee target
# and the unit_tests target exist. Everything the fork adds to the build lives
# here, so base merges never touch it.

# The one place to bump Prism: release tag and the sha256 of the zip below.
set(A11Y_PRISM_VERSION v0.18.2)
set(A11Y_PRISM_SHA256 31c02e3ef2260b4d3b11fb00132f8eb12bc147b5fb17031b580670b117ba7d23)

# Per-platform Prism wiring. An empty asset means no Prism: the screen reader
# bridge then compiles as a silent stub. Prism publishes
# prism-macos-universal.zip, prism-linux-x64.zip and prism-linux-arm64.zip for
# the same tag, so another platform is a new branch here that sets the asset,
# its hash, the library to link and the runtime file to copy; the fork's C++
# stays as it is (.scratch/macos-port/spec.md has the worked plan).
set(A11Y_PRISM_ASSET "")
if (WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64)$")
    set(A11Y_PRISM_ASSET prism-windows-x64.zip)
    # The dynamic release variant: the static one needs delay loading on the
    # consumer side, which MinGW lacks. GNU ld reads the MSVC import library
    # directly; the exported surface is plain C with opaque pointers.
    set(A11Y_PRISM_LINK_LIB dynamic/release/lib/prism.lib)
    set(A11Y_PRISM_RUNTIME_LIB dynamic/release/bin/prism.dll)
endif ()

target_sources(melee PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/hooks.cpp
        ${CMAKE_CURRENT_LIST_DIR}/speech.cpp
        ${CMAKE_CURRENT_LIST_DIR}/screen_reader_bridge.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rmlui_reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/launcher_speech.cpp
        ${CMAKE_CURRENT_LIST_DIR}/scene_speech.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game_text.cpp
        ${CMAKE_CURRENT_LIST_DIR}/game_access.c
        ${CMAKE_CURRENT_LIST_DIR}/menu_speech.cpp
        ${CMAKE_CURRENT_LIST_DIR}/menu_names.cpp
        ${CMAKE_CURRENT_LIST_DIR}/menu_text.cpp
        ${CMAKE_CURRENT_LIST_DIR}/css_speech.cpp
        ${CMAKE_CURRENT_LIST_DIR}/css_names.cpp
        ${CMAKE_CURRENT_LIST_DIR}/css_targets.cpp
        ${CMAKE_CURRENT_LIST_DIR}/css_reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/steering.cpp)
target_include_directories(melee PRIVATE ${CMAKE_CURRENT_LIST_DIR})
# game_access.c reads the decomp's structs, so it lays out their bitfields as
# the game's own files do (melee_game's options in CMakeLists.txt); MinGW's
# default layout moves every field after one.
if (WIN32)
    set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/game_access.c
            PROPERTIES COMPILE_OPTIONS -mno-ms-bitfields)
endif ()

if (A11Y_PRISM_ASSET)
    # Prebuilt, not built from source: see docs/adr/0001-prebuilt-prism-dll.md.
    # The directory name carries the tag, so a bump downloads afresh, and the
    # stamp file marks a finished extraction, so a later configure works
    # offline and an interrupted one retries.
    set(_prism_dir "${CMAKE_BINARY_DIR}/prism-${A11Y_PRISM_VERSION}")
    set(_prism_stamp "${_prism_dir}/.extracted")
    if (NOT EXISTS "${_prism_stamp}")
        set(_prism_url
                "https://github.com/ethindp/prism/releases/download/${A11Y_PRISM_VERSION}/${A11Y_PRISM_ASSET}")
        set(_prism_zip "${CMAKE_BINARY_DIR}/${A11Y_PRISM_ASSET}")
        message(STATUS "a11y: downloading Prism ${A11Y_PRISM_VERSION} (${A11Y_PRISM_ASSET})")
        file(DOWNLOAD "${_prism_url}" "${_prism_zip}"
                EXPECTED_HASH SHA256=${A11Y_PRISM_SHA256}
                STATUS _prism_status)
        list(GET _prism_status 0 _prism_code)
        if (NOT _prism_code EQUAL 0)
            list(GET _prism_status 1 _prism_error)
            file(REMOVE "${_prism_zip}")
            message(FATAL_ERROR "a11y: could not download ${_prism_url}: ${_prism_error}. "
                    "The first configure needs network access.")
        endif ()
        file(REMOVE_RECURSE "${_prism_dir}")
        # The zip also carries debug builds and static libraries; only the
        # header, the dynamic release build and the licences are used.
        file(ARCHIVE_EXTRACT INPUT "${_prism_zip}" DESTINATION "${_prism_dir}"
                PATTERNS include/* dynamic/release/* LICENSES/* NOTICE)
        file(REMOVE "${_prism_zip}")
        file(TOUCH "${_prism_stamp}")
    endif ()

    target_include_directories(melee PRIVATE "${_prism_dir}/include")
    target_link_libraries(melee PRIVATE "${_prism_dir}/${A11Y_PRISM_LINK_LIB}")
    # This macro, not the platform, selects the real bridge body.
    target_compile_definitions(melee PRIVATE A11Y_HAVE_PRISM)
    # Linked at build time rather than loaded at run time: a missing prism.dll
    # stops melee.exe with the Windows "not found" dialog, which a screen
    # reader reads, instead of starting silently.
    add_custom_command(TARGET melee POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_prism_dir}/${A11Y_PRISM_RUNTIME_LIB}" "$<TARGET_FILE_DIR:melee>")
endif ()

if (TARGET unit_tests)
    # Speech against a fake bridge; the test defines its own pc_log_line.
    add_executable(speech_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/speech.cpp)
    target_include_directories(speech_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(speech_test PRIVATE -UNDEBUG)
    add_test(NAME speech COMMAND speech_test)
    set_tests_properties(speech PROPERTIES LABELS melee)
    add_dependencies(unit_tests speech_test)

    # Launcher speech against the real resources/launcher.rml, in an RmlUi
    # context that draws nothing; also a fake bridge and its own pc_log_line.
    add_executable(launcher_speech_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_launcher_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/launcher_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/rmlui_reader.cpp
            ${CMAKE_CURRENT_LIST_DIR}/speech.cpp)
    target_include_directories(launcher_speech_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(launcher_speech_test PRIVATE rmlui)
    target_compile_options(launcher_speech_test PRIVATE -UNDEBUG)
    add_test(NAME launcher_speech COMMAND launcher_speech_test ${PROJECT_SOURCE_DIR}/resources)
    set_tests_properties(launcher_speech PROPERTIES LABELS melee)
    add_dependencies(unit_tests launcher_speech_test)

    # The scene table against a fake bridge; its own pc_log_line.
    add_executable(scene_speech_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_scene_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/scene_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/speech.cpp)
    target_include_directories(scene_speech_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(scene_speech_test PRIVATE -UNDEBUG)
    add_test(NAME scene_speech COMMAND scene_speech_test)
    set_tests_properties(scene_speech PROPERTIES LABELS melee)
    add_dependencies(unit_tests scene_speech_test)

    # The menu reader against hand-written snapshots of the main menu tree;
    # a fake bridge and its own pc_log_line.
    add_executable(menu_speech_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_menu_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/menu_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/menu_names.cpp
            ${CMAKE_CURRENT_LIST_DIR}/game_text.cpp
            ${CMAKE_CURRENT_LIST_DIR}/speech.cpp)
    target_include_directories(menu_speech_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(menu_speech_test PRIVATE -UNDEBUG)
    add_test(NAME menu_speech COMMAND menu_speech_test)
    set_tests_properties(menu_speech PROPERTIES LABELS melee)
    add_dependencies(unit_tests menu_speech_test)

    # Character select speech against hand-written snapshots of the screen;
    # a fake bridge and its own pc_log_line.
    add_executable(css_speech_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_css_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/css_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/css_names.cpp
            ${CMAKE_CURRENT_LIST_DIR}/css_targets.cpp
            ${CMAKE_CURRENT_LIST_DIR}/speech.cpp)
    target_include_directories(css_speech_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(css_speech_test PRIVATE -UNDEBUG)
    add_test(NAME css_speech COMMAND css_speech_test)
    set_tests_properties(css_speech PROPERTIES LABELS melee)
    add_dependencies(unit_tests css_speech_test)

    # The glide against a simulated hand that moves as the game's does.
    add_executable(steering_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_steering.cpp
            ${CMAKE_CURRENT_LIST_DIR}/steering.cpp
            ${CMAKE_CURRENT_LIST_DIR}/css_targets.cpp)
    target_include_directories(steering_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(steering_test PRIVATE -UNDEBUG)
    add_test(NAME steering COMMAND steering_test)
    set_tests_properties(steering PROPERTIES LABELS melee)
    add_dependencies(unit_tests steering_test)

    # Character select's targets against hand-written snapshots.
    add_executable(css_targets_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_css_targets.cpp
            ${CMAKE_CURRENT_LIST_DIR}/css_targets.cpp)
    target_include_directories(css_targets_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(css_targets_test PRIVATE -UNDEBUG)
    add_test(NAME css_targets COMMAND css_targets_test)
    set_tests_properties(css_targets PROPERTIES LABELS melee)
    add_dependencies(unit_tests css_targets_test)

    # The game text decoder against hand-written byte code.
    add_executable(game_text_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_game_text.cpp
            ${CMAKE_CURRENT_LIST_DIR}/game_text.cpp)
    target_include_directories(game_text_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(game_text_test PRIVATE -UNDEBUG)
    add_test(NAME game_text COMMAND game_text_test)
    set_tests_properties(game_text PROPERTIES LABELS melee)
    add_dependencies(unit_tests game_text_test)
endif ()
