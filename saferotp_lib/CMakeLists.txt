
cmake_minimum_required(VERSION 3.21)
set(CMAKE_C_STANDARD   11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(PICO_PLATFORM "rp2350")


# TODO -- Add selectable option to enable hardware HAL
# TODO -- Add selectable option to enable virtualization HAL
# TODO -- Only import the PICO SDK when hardware HAL option is enabled
# Pico SDK requires this occur prior to setting the project()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
pico_sdk_init()

project(                    saferotp_lib   C CXX ASM)

# TODO -- exclude unused HAL files from source list?  (currently excluding via `#if defined()` blocks)
add_library(                saferotp_lib   STATIC
        lib/saferotp_direntry.c
        lib/saferotp_ecc.c
        lib/saferotp_rw.c
        lib/saferotp_debug_stub.c
        lib/saferotp_debug_stub.h
        lib/saferotp_hal.c
        lib/saferotp_hal.h
        lib/saferotp_hal_hardware.c
        lib/saferotp_hal_virtualized.c
)

# TODO -- remove BusPirate specific hacks
if (NOT BP_VER)
        # Nothing
elseif(NOT (${BP_VER} MATCHES "5"))
        message(STATUS "BusPirate version ${BP_VER} detected")
        # BusPirate6 integration hack
        message(STATUS "SaferOTP ... using hack for BusPirate, to include ../../debug_rtt.h")
        target_include_directories( saferotp_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../)
endif()

# Cannot be INTERFACE ... as cannot then find "pico/stdlib.h"
# TODO -- remove pico_stdlib unless enabling hardware HAL option
target_link_libraries(      saferotp_lib PRIVATE   pico_stdlib)

target_include_directories( saferotp_lib PRIVATE   lib)
target_include_directories( saferotp_lib PRIVATE   inc)
target_include_directories( saferotp_lib INTERFACE inc)

# TODO: How to enable -Wpendantic only for own files, and not pico_stdlib?
#       pico_stdlib has lots of errors when `-Wpedentic` is enabled
# the following line did not work:
# target_include_directories( saferotp_lib SYSTEM PRIVATE ${pico_stdlib_INCLUDE_DIRS})
# But there's supposed to be a way to mark them as system includes?

target_compile_options(     saferotp_lib PRIVATE   -Wall)
# target_compile_options(     saferotp_lib PRIVATE   -Werror)
target_compile_options(     saferotp_lib PRIVATE   -O0)
# target_compile_options(     saferotp_lib PRIVATE   -Wpedantic)
target_compile_options(     saferotp_lib PRIVATE   -Wno-unknown-pragmas)
target_compile_options(     saferotp_lib PRIVATE   -Wno-inline)
target_compile_options(     saferotp_lib PRIVATE   -Wno-unused-function)
set_property(TARGET         saferotp_lib PROPERTY  POSITION_INDEPENDENT_CODE ON)

