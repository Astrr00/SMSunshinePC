cmake_minimum_required(VERSION 3.25)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif ()

project(sunshine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The decomp uses C99 compound literals in C++ ((Vec){...}), which MSVC's cl rejects. Game code needs clang-cl.
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Configure with the windows-clang-* preset (clang-cl); got ${CMAKE_CXX_COMPILER_ID}")
endif ()

# Same switches as Dusklight: DVD (nod) for reading the user's disc image, CARD for .gci saves,
# THP for the FMV decoder, RmlUi for the launcher UI.
set(AURORA_ENABLE_DVD ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_CARD ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_THP ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_RMLUI ON CACHE BOOL "" FORCE)
add_subdirectory(extern/aurora EXCLUDE_FROM_ALL)

# Decomp game code, listed by tools/gen-files.py from sms/configure.py.
set(SMS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../sms)
set(SMS_SRC_DIR ${SMS_DIR}/src)
include(files.cmake)
set_source_files_properties(${SMS_CXX_C_SOURCES} PROPERTIES LANGUAGE CXX) # THPPlayer is built -lang=c++

# Port-side SDK layer (OS threads, AI/VI/DSP glue, GX leftovers): compiled with the game flags so it sees the decomp's headers.
file(GLOB SDK_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/sdk/*.cpp)
add_library(sms_game STATIC ${SMS_GAME_SOURCES} ${SDK_SOURCES})
# Order matters: port shims first, then the decomp's headers, Aurora's last (it only supplies what the decomp lacks).
target_include_directories(sms_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SMS_DIR}/include
    ${SMS_DIR}/include/dolphin
    ${SMS_DIR}/include/PowerPC_EABI_Support
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/aurora/include)
target_compile_definitions(sms_game PUBLIC TARGET_PC=1 VERSION_GMSJ01=1 NDEBUG=1)
target_compile_options(sms_game PUBLIC
    /utf-8 /Zc:__cplusplus /W0 -Wno-everything -Wno-multichar -Wno-trigraphs
    /FI${CMAKE_CURRENT_SOURCE_DIR}/include/pc_compat.h)
target_compile_options(sms_game PRIVATE /MP)

add_executable(sunshine src/main.cpp)
target_link_libraries(sunshine PRIVATE sms_game
    aurora::core aurora::gx aurora::gd aurora::si aurora::vi aurora::pad aurora::mtx aurora::os
    aurora::dvd aurora::thp aurora::card aurora::main)
include(extern/aurora/cmake/AuroraCopyRuntimeDLLs.cmake)
aurora_copy_runtime_dlls(sunshine)
# List every unresolved symbol instead of stopping after 100 (Goal-Port B.2 sorts them by cause).
target_link_options(sunshine PRIVATE /errorlimit:0)
