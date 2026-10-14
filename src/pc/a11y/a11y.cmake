# SPDX-License-Identifier: GPL-3.0-or-later
# Accessibility fork: speech, its screen reader library (Prism), the readers
# that turn RmlUi pages and the game's screens into speech, and the cue engine
# with its sound library (miniaudio).
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
        ${CMAKE_CURRENT_LIST_DIR}/lobby_speech.cpp
        ${CMAKE_CURRENT_LIST_DIR}/sss_speech.cpp
        ${CMAKE_CURRENT_LIST_DIR}/sss_names.cpp
        ${CMAKE_CURRENT_LIST_DIR}/sss_reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/sss_targets.cpp
        ${CMAKE_CURRENT_LIST_DIR}/steering.cpp
        ${CMAKE_CURRENT_LIST_DIR}/steerer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/cue_engine.cpp
        ${CMAKE_CURRENT_LIST_DIR}/cue_output.cpp
        ${CMAKE_CURRENT_LIST_DIR}/cue_sounds.cpp
        ${CMAKE_CURRENT_LIST_DIR}/cue_settings.cpp
        ${CMAKE_CURRENT_LIST_DIR}/cue_system.cpp
        ${CMAKE_CURRENT_LIST_DIR}/value_model.cpp
        ${CMAKE_CURRENT_LIST_DIR}/value_file.cpp
        ${CMAKE_CURRENT_LIST_DIR}/position_cue.cpp
        ${CMAKE_CURRENT_LIST_DIR}/gap_warning.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ledge_warning.cpp
        ${CMAKE_CURRENT_LIST_DIR}/match_cues.cpp)
target_include_directories(melee PRIVATE ${CMAKE_CURRENT_LIST_DIR})
# game_access.c reads the decomp's structs, so it lays out their bitfields as
# the game's own files do (melee_game's options in CMakeLists.txt); MinGW's
# default layout moves every field after one. The fighter's headers also bring
# the big-endian disc structs, whose warning the game's files turn off too.
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/game_access.c PROPERTIES COMPILE_OPTIONS
        "$<$<BOOL:${WIN32}>:-mno-ms-bitfields>;$<$<C_COMPILER_ID:GNU>:-Wno-scalar-storage-order>")

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

# miniaudio, the cue engine's sound library (docs/adr/0005-cues-play-through-
# the-forks-own-audio-stream.md): one header, downloaded at configure like
# Prism and compiled once in its own library. It must be 0.11.25 or later:
# 0.11.21 crackled on every pitch change. The directory name carries the
# version, so a bump downloads afresh.
set(A11Y_MINIAUDIO_VERSION 0.11.25)
set(A11Y_MINIAUDIO_SHA256 ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89)
set(_miniaudio_dir "${CMAKE_BINARY_DIR}/miniaudio-${A11Y_MINIAUDIO_VERSION}")
if (NOT EXISTS "${_miniaudio_dir}/miniaudio.h")
    message(STATUS "a11y: downloading miniaudio ${A11Y_MINIAUDIO_VERSION}")
    set(_miniaudio_url
            "https://raw.githubusercontent.com/mackron/miniaudio/${A11Y_MINIAUDIO_VERSION}/miniaudio.h")
    file(DOWNLOAD "${_miniaudio_url}" "${_miniaudio_dir}/miniaudio.h.part"
            EXPECTED_HASH SHA256=${A11Y_MINIAUDIO_SHA256}
            STATUS _miniaudio_status)
    list(GET _miniaudio_status 0 _miniaudio_code)
    if (NOT _miniaudio_code EQUAL 0)
        list(GET _miniaudio_status 1 _miniaudio_error)
        file(REMOVE "${_miniaudio_dir}/miniaudio.h.part")
        message(FATAL_ERROR "a11y: could not download ${_miniaudio_url}: ${_miniaudio_error}. "
                "The first configure needs network access.")
    endif ()
    file(RENAME "${_miniaudio_dir}/miniaudio.h.part" "${_miniaudio_dir}/miniaudio.h")
endif ()
add_library(a11y_miniaudio STATIC ${CMAKE_CURRENT_LIST_DIR}/miniaudio_impl.c)
# A system include, so its warnings stay out of the files that include it.
target_include_directories(a11y_miniaudio SYSTEM PUBLIC "${_miniaudio_dir}")
# Public: these change the layout of miniaudio's structs, so every file that
# includes the header gets them. SDL plays the mix, so no device code; the
# cues never write audio files.
target_compile_definitions(a11y_miniaudio PUBLIC MA_NO_DEVICE_IO MA_NO_ENCODING)
target_compile_options(a11y_miniaudio PRIVATE -w)
target_link_libraries(melee PRIVATE a11y_miniaudio)

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
            ${CMAKE_CURRENT_LIST_DIR}/sss_names.cpp
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

    # Stage select speech against hand-written snapshots of the screen; a fake
    # bridge and its own pc_log_line.
    add_executable(sss_speech_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_sss_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sss_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sss_names.cpp
            ${CMAKE_CURRENT_LIST_DIR}/speech.cpp)
    target_include_directories(sss_speech_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(sss_speech_test PRIVATE -UNDEBUG)
    add_test(NAME sss_speech COMMAND sss_speech_test)
    set_tests_properties(sss_speech PROPERTIES LABELS melee)
    add_dependencies(unit_tests sss_speech_test)

    # Lobby speech against hand-written snapshots of the lobby's view; a fake
    # bridge and its own pc_log_line.
    add_executable(lobby_speech_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_lobby_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/lobby_speech.cpp
            ${CMAKE_CURRENT_LIST_DIR}/speech.cpp)
    target_include_directories(lobby_speech_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(lobby_speech_test PRIVATE -UNDEBUG)
    add_test(NAME lobby_speech COMMAND lobby_speech_test)
    set_tests_properties(lobby_speech PROPERTIES LABELS melee)
    add_dependencies(unit_tests lobby_speech_test)

    # The glide against simulated cursors that move as the game's do.
    add_executable(steering_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_steering.cpp
            ${CMAKE_CURRENT_LIST_DIR}/steering.cpp
            ${CMAKE_CURRENT_LIST_DIR}/css_targets.cpp)
    target_include_directories(steering_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(steering_test PRIVATE -UNDEBUG)
    add_test(NAME steering COMMAND steering_test)
    set_tests_properties(steering PROPERTIES LABELS melee)
    add_dependencies(unit_tests steering_test)

    # The shared steps over rows of plain targets.
    add_executable(steps_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_steps.cpp)
    target_include_directories(steps_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(steps_test PRIVATE -UNDEBUG)
    add_test(NAME steps COMMAND steps_test)
    set_tests_properties(steps PROPERTIES LABELS melee)
    add_dependencies(unit_tests steps_test)

    # Stage select's targets against hand-written snapshots.
    add_executable(sss_targets_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_sss_targets.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sss_targets.cpp)
    target_include_directories(sss_targets_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(sss_targets_test PRIVATE -UNDEBUG)
    add_test(NAME sss_targets COMMAND sss_targets_test)
    set_tests_properties(sss_targets PROPERTIES LABELS melee)
    add_dependencies(unit_tests sss_targets_test)

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

    # The cue engine rendered into buffers, with no device; its own
    # pc_log_line. It also writes a dump of a known pulse train, which the
    # dump analysis script must then read back correctly.
    add_executable(cue_engine_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_cue_engine.cpp
            ${CMAKE_CURRENT_LIST_DIR}/cue_engine.cpp
            ${CMAKE_CURRENT_LIST_DIR}/cue_sounds.cpp)
    target_include_directories(cue_engine_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(cue_engine_test PRIVATE a11y_miniaudio)
    target_compile_options(cue_engine_test PRIVATE -UNDEBUG)
    add_test(NAME cue_engine COMMAND cue_engine_test ${CMAKE_BINARY_DIR}/cue_pulse_train.f32)
    set_tests_properties(cue_engine PROPERTIES LABELS melee FIXTURES_SETUP cue_dump)
    add_dependencies(unit_tests cue_engine_test)
    if (MELEE_PYTHON)
        add_test(NAME cue_dump_analysis COMMAND ${MELEE_PYTHON}
                ${PROJECT_SOURCE_DIR}/tools/a11y/test_cue_dump.py ${CMAKE_BINARY_DIR}/cue_pulse_train.f32)
        set_tests_properties(cue_dump_analysis PROPERTIES LABELS melee FIXTURES_REQUIRED cue_dump)
    endif ()

    # The value model and its file against files in a scratch folder, and
    # the cue settings feeding the cue engine rendered into a buffer; a fake
    # bridge and its own pc_log_line.
    add_executable(cue_settings_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_cue_settings.cpp
            ${CMAKE_CURRENT_LIST_DIR}/value_model.cpp
            ${CMAKE_CURRENT_LIST_DIR}/value_file.cpp
            ${CMAKE_CURRENT_LIST_DIR}/cue_settings.cpp
            ${CMAKE_CURRENT_LIST_DIR}/cue_engine.cpp
            ${CMAKE_CURRENT_LIST_DIR}/speech.cpp)
    target_include_directories(cue_settings_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(cue_settings_test PRIVATE -UNDEBUG)
    add_test(NAME cue_settings COMMAND cue_settings_test ${CMAKE_BINARY_DIR}/cue_settings_test)
    set_tests_properties(cue_settings PROPERTIES LABELS melee)
    add_dependencies(unit_tests cue_settings_test)

    # The position cues' driver against hand-written match snapshots.
    add_executable(position_cue_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_position_cue.cpp
            ${CMAKE_CURRENT_LIST_DIR}/position_cue.cpp)
    target_include_directories(position_cue_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(position_cue_test PRIVATE -UNDEBUG)
    add_test(NAME position_cue COMMAND position_cue_test)
    set_tests_properties(position_cue PROPERTIES LABELS melee)
    add_dependencies(unit_tests position_cue_test)

    # The gap warning's driver against hand-written match snapshots.
    add_executable(gap_warning_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_gap_warning.cpp
            ${CMAKE_CURRENT_LIST_DIR}/gap_warning.cpp)
    target_include_directories(gap_warning_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(gap_warning_test PRIVATE -UNDEBUG)
    add_test(NAME gap_warning COMMAND gap_warning_test)
    set_tests_properties(gap_warning PROPERTIES LABELS melee)
    add_dependencies(unit_tests gap_warning_test)

    # The ledge warning's driver against hand-written match snapshots.
    add_executable(ledge_warning_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_ledge_warning.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ledge_warning.cpp)
    target_include_directories(ledge_warning_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(ledge_warning_test PRIVATE -UNDEBUG)
    add_test(NAME ledge_warning COMMAND ledge_warning_test)
    set_tests_properties(ledge_warning PROPERTIES LABELS melee)
    add_dependencies(unit_tests ledge_warning_test)

    # The match cues against hand-written match snapshots, their voices
    # rendered into buffers with no device; its own pc_log_line.
    add_executable(match_cues_test EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/test_match_cues.cpp
            ${CMAKE_CURRENT_LIST_DIR}/match_cues.cpp
            ${CMAKE_CURRENT_LIST_DIR}/position_cue.cpp
            ${CMAKE_CURRENT_LIST_DIR}/gap_warning.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ledge_warning.cpp
            ${CMAKE_CURRENT_LIST_DIR}/cue_settings.cpp
            ${CMAKE_CURRENT_LIST_DIR}/value_model.cpp
            ${CMAKE_CURRENT_LIST_DIR}/cue_engine.cpp
            ${CMAKE_CURRENT_LIST_DIR}/cue_sounds.cpp)
    target_include_directories(match_cues_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(match_cues_test PRIVATE a11y_miniaudio)
    target_compile_options(match_cues_test PRIVATE -UNDEBUG)
    add_test(NAME match_cues COMMAND match_cues_test)
    set_tests_properties(match_cues PROPERTIES LABELS melee)
    add_dependencies(unit_tests match_cues_test)
endif ()
