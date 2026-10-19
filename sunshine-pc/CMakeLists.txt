cmake_minimum_required(VERSION 3.25)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif ()

project(sunshine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The decomp uses C99 compound literals in C++ ((Vec){...}), which MSVC's cl rejects. Game code needs clang-cl.
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Configure with the windows-clang-* preset (clang-cl); got ${CMAKE_CXX_COMPILER_ID}")
endif ()

# Same switches as Dusklight: DVD (nod) for reading the user's disc image, CARD for .gci saves, RmlUi for the launcher UI.
# THP is OFF: the port builds the decomp's own decoder (sms/src/dolphin/thp, host versions under TARGET_PC), which is the
# console's output bit for bit; Aurora's decoder deliberately differs (it fixes the SDK's quarter-IDCT column swap and
# rounds audio ties up).
set(AURORA_ENABLE_DVD ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_CARD ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_THP OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_RMLUI ON CACHE BOOL "" FORCE)
# Developers can point this at a private copy of Aurora to work on multi-file changes without breaking other trees.
set(SUNSHINE_AURORA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/aurora" CACHE PATH "Aurora source directory")
add_subdirectory(${SUNSHINE_AURORA_DIR} extern/aurora EXCLUDE_FROM_ALL)

# Decomp game code, listed by tools/gen-files.py from sms/configure.py.
set(SMS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../sms)
set(SMS_SRC_DIR ${SMS_DIR}/src)
include(files.cmake)
set_source_files_properties(${SMS_CXX_C_SOURCES} PROPERTIES LANGUAGE CXX) # THPPlayer is built -lang=c++

# Port-side SDK layer (OS threads, AI/VI/DSP glue, GX leftovers): compiled with the game flags so it sees the decomp's headers.
file(GLOB SDK_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/sdk/*.cpp)
add_library(sms_game STATIC ${SMS_GAME_SOURCES} ${SDK_SOURCES})
target_link_libraries(sms_game PUBLIC bcrypt)

# Shift-JIS string literals: the decomp sources are UTF-8, the disc data is Shift-JIS, and clang-cl cannot emit a
# Shift-JIS execution charset. tools/sjis_overlay.py writes copies with octal-escaped CP932 literals and a clang VFS
# overlay that makes the compiler read them; it runs (incrementally, well under a second) before every build.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# Aurora's patch series (patches/aurora) must be applied to extern/aurora. Warn only, so configure never rewrites the
# submodule; the apply step is idempotent: python tools/apply-aurora-patches.py
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/apply-aurora-patches.py --check
    RESULT_VARIABLE AURORA_PATCH_RESULT
    OUTPUT_VARIABLE AURORA_PATCH_OUTPUT
    ERROR_VARIABLE AURORA_PATCH_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
if (NOT AURORA_PATCH_RESULT EQUAL 0)
    message(WARNING "extern/aurora: ${AURORA_PATCH_OUTPUT}")
endif ()
set(SJIS_OVERLAY ${CMAKE_BINARY_DIR}/sjis/overlay.yaml)
add_custom_target(sjis_overlay
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/sjis_overlay.py ${SMS_DIR} ${CMAKE_BINARY_DIR}/sjis
    BYPRODUCTS ${SJIS_OVERLAY}
    COMMENT "Shift-JIS string literal overlay")
add_dependencies(sms_game sjis_overlay)
target_compile_options(sms_game PUBLIC "SHELL:-Xclang -ivfsoverlay -Xclang \"${SJIS_OVERLAY}\"")
# Order matters: port shims first, then the decomp's headers, Aurora's last (it only supplies what the decomp lacks).
target_include_directories(sms_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SMS_DIR}/include
    ${SMS_DIR}/include/dolphin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/glyphs
    ${SMS_DIR}/include/PowerPC_EABI_Support
    ${SUNSHINE_AURORA_DIR}/include)
target_compile_definitions(sms_game PUBLIC TARGET_PC=1 VERSION_GMSJ01=1 NDEBUG=1)
# -fno-strict-return: many decomp functions fall off the end of a non-void function (MWCC just returns whatever is in
# the return register). Clang treats that as unreachable and executes padding (int3), e.g. JUTGamePad::read.
target_compile_options(sms_game PUBLIC
    /utf-8 /Zc:__cplusplus /W0 -Wno-everything -Wno-multichar -Wno-trigraphs /clang:-fno-strict-return
    /FI${CMAKE_CURRENT_SOURCE_DIR}/include/pc_compat.h)
target_compile_options(sms_game PRIVATE /MP)

add_executable(sunshine src/main.cpp src/crash.cpp src/logging.cpp src/sampler.cpp src/devstart/devstart.cpp src/render/render_scale.cpp
    src/cheats/cheats_core.cpp src/cheats/cheats_game.cpp src/json/json_lite.cpp
    src/mods/mod_manifest.cpp src/mods/mods_host.cpp src/mods/mods_loader.cpp src/util/read_file.cpp
    src/settings.cpp src/launcher/launcher.cpp src/glyphs/glyphs.cpp src/framerate/framerate.cpp src/widescreen/widescreen.cpp
    src/saves/save_directory.cpp src/saves/save_roundtrip.cpp src/devscript/devscript.cpp src/mouse/mouse.cpp)
target_include_directories(sunshine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_property(SOURCE src/logging.cpp APPEND PROPERTY COMPILE_DEFINITIONS
    SUNSHINE_VERSION="${PROJECT_VERSION}" SUNSHINE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
# Round-trip test of the INI settings reader/writer (tools/settings_test.cpp); no game or Aurora dependency.
add_executable(settings_test tools/settings_test.cpp src/settings.cpp)
target_include_directories(settings_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_custom_command(TARGET sunshine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:sunshine>/assets
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/pipeline_cache_seed.py $<TARGET_FILE_DIR:sunshine>/initial_pipeline_cache.db)
target_link_libraries(sunshine PRIVATE sms_game
    aurora::core aurora::gx aurora::gd aurora::si aurora::vi aurora::pad aurora::mtx aurora::os
    aurora::dvd aurora::card aurora::main ZLIB::ZLIB)
include(${SUNSHINE_AURORA_DIR}/cmake/AuroraCopyRuntimeDLLs.cmake)
aurora_copy_runtime_dlls(sunshine)
# List every unresolved symbol instead of stopping after 100 (Goal-Port B.2 sorts them by cause).
target_link_options(sunshine PRIVATE /errorlimit:0)
# The decomp keeps large buffers on the stack (and ASan adds red zones); the default 1 MiB main stack is too small.
target_link_options(sunshine PRIVATE /STACK:67108864)

# Developer diagnostic, OFF by default: turn unresolved symbols into runtime faults so the boot path can run before
# every decomp source exists. Never use it for a deliverable build: a missing function then crashes when called.
option(SUNSHINE_ALLOW_UNRESOLVED "Link with /FORCE:UNRESOLVED (diagnostics only)" OFF)
if (SUNSHINE_ALLOW_UNRESOLVED)
    target_link_options(sunshine PRIVATE /FORCE:UNRESOLVED)
endif ()

# Developer diagnostic, OFF by default: AddressSanitizer on the game code and the port (not on Aurora/Dawn). Finds heap
# corruption and bad frees in the decompiled game. Needs clang-cl's ASan runtime, which is copied next to the exe.
option(SUNSHINE_ASAN "Build game code and port with AddressSanitizer (diagnostics only)" OFF)
if (SUNSHINE_ASAN)
    get_filename_component(_llvm_bin ${CMAKE_CXX_COMPILER} DIRECTORY)
    file(GLOB _asan_dir "${_llvm_bin}/../lib/clang/*/lib/windows")
    list(GET _asan_dir 0 _asan_dir)
    # Global redzones misreport merged string literals at static init (see platform-notes.md); heap is the target.
    target_compile_options(sms_game PUBLIC /fsanitize=address -mllvm -asan-globals=0)
    # MSVC STL container annotations produce false positives when only some translation units are instrumented.
    target_compile_definitions(sms_game PUBLIC _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
    target_link_libraries(sunshine PRIVATE
        "${_asan_dir}/clang_rt.asan_dynamic-x86_64.lib"
        "-wholearchive:${_asan_dir}/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib")
    target_link_options(sunshine PRIVATE /INCREMENTAL:NO)
    add_custom_command(TARGET sunshine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_asan_dir}/clang_rt.asan_dynamic-x86_64.dll" $<TARGET_FILE_DIR:sunshine>)
endif ()

# Example mods (mods-example/), OFF by default. Each builds <exe dir>/mod-examples/<id>/ (mod.dll + mod.json). That
# folder is NOT on the mods/ discovery path: copy it into <exe dir>/mods/ to enable it.
option(SUNSHINE_BUILD_EXAMPLE_MODS "Build the example mods into <exe dir>/mod-examples" OFF)
if (SUNSHINE_BUILD_EXAMPLE_MODS)
    foreach (_mod hello faulty)
        add_library(sunshine_mod_${_mod} SHARED mods-example/${_mod}/mod.cpp)
        target_include_directories(sunshine_mod_${_mod} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(sunshine_mod_${_mod} PROPERTIES OUTPUT_NAME mod
            RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:sunshine>/mod-examples/${_mod}
            LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:sunshine>/mod-examples/${_mod}
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mod_import/${_mod})
        add_custom_command(TARGET sunshine_mod_${_mod} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/mods-example/${_mod}/mod.json
                $<TARGET_FILE_DIR:sunshine_mod_${_mod}>/mod.json)
    endforeach ()
endif ()

# Installation-free Windows package. `cmake --install <build> --prefix <dir>` or `cpack` (ZIP). The release path is
# tools/package.py, which also writes the licence texts and refuses to ship anything that looks like a game image.
install(TARGETS sunshine RUNTIME DESTINATION .)
if (WIN32)
    # Import closure of sunshine.exe plus the DXC pair Dawn LoadLibrary()s (in no import table).
    # Not aurora_install_runtime_dlls: its install(CODE) writes to the relative destination "." (cwd), not the prefix.
    install(FILES $<TARGET_RUNTIME_DLLS:sunshine> DESTINATION .)
    if (TARGET dawn::webgpu_dawn)
        install(FILES "$<TARGET_FILE_DIR:sunshine>/dxcompiler.dll" "$<TARGET_FILE_DIR:sunshine>/dxil.dll" DESTINATION .)
    endif ()
endif ()
install(DIRECTORY assets/ DESTINATION assets OPTIONAL)
install(FILES "$<TARGET_FILE_DIR:sunshine>/initial_pipeline_cache.db" DESTINATION .)
install(FILES LICENSE THIRD_PARTY_LICENSES.md docs/TESTING.md tools/collect_logs.cmd tools/collect_logs.ps1 tools/play_logged.cmd DESTINATION .)
install(DIRECTORY licenses/ DESTINATION licenses)
set(CPACK_PACKAGE_NAME "sunshine")
set(CPACK_GENERATOR "ZIP")
include(CPack)
