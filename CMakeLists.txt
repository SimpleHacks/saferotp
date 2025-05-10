
cmake_minimum_required(VERSION 3.21)
set(CMAKE_C_STANDARD   11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(PICO_PLATFORM "rp2350")

project(                    saferotp_lib   C CXX ASM)

# Pico SDK requires this occur prior to setting the project()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
pico_sdk_init()

add_library(                saferotp_lib   STATIC
        saferotp_lib/saferotp_direntry.c
        saferotp_lib/saferotp_ecc.c
        saferotp_lib/saferotp_rw.c
        saferotp_lib/saferotp_debug_stub.c
)

# HACK -- Manually add the include directory for debug_rtt.h
#         To support saferotp_lib/saferotp_debug_stub.h
message(STATUS "SaferOTP ... using hack to include debug_rtt.h")
target_include_directories( saferotp_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../)

# Cannot be INTERFACE ... as cannot then find "pico/stdlib.h"
target_link_libraries(      saferotp_lib PRIVATE pico_stdlib)

target_include_directories( saferotp_lib PRIVATE   saferotp_lib)
target_include_directories( saferotp_lib PRIVATE   saferotp_inc)
target_include_directories( saferotp_lib INTERFACE saferotp_inc)

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

