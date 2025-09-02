
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

project(    saferotp_lib   C CXX ASM)

# There are two options that the parent project can override.
option( SAFEROTP_DISABLE_HARDWARE_OTP    "Disable access to hardware-backed OTP"   OFF )
option( SAFEROTP_ENABLE_VIRTUALIZED_OTP  "Enable access to virtualized OTP"        OFF )

set(saferotp_sources
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
add_library( saferotp_lib             INTERFACE)                   # Clients will link to `saferotp_lib`, which resolves to one of:
add_library( saferotp_lib_default     STATIC ${saferotp_sources} ) # DEFAULT: hardware-backed OTP only (no virtualized OTP)
add_library( saferotp_lib_with_virt   STATIC ${saferotp_sources} ) # hardware-backed OTP, with virtualized OTP available
add_library( saferotp_lib_virt_only   STATIC ${saferotp_sources} ) # no hardware-backed OTP ... only virtualized OTP
add_library( saferotp_lib_no_otp      STATIC ${saferotp_sources} ) # neither hardware OTP nor virtualized OTP

# Private items are still defined individually for each variant:
target_link_libraries(      saferotp_lib_default   PRIVATE   pico_stdlib)
target_link_libraries(      saferotp_lib_with_virt PRIVATE   pico_stdlib)
target_link_libraries(      saferotp_lib_virt_only PRIVATE   pico_stdlib) # if only using virtualized OTP, no pico stdlib required?
target_link_libraries(      saferotp_lib_no_otp    PRIVATE   pico_stdlib) # if no OTP support, no pico stdlib required?
target_include_directories( saferotp_lib_default   PRIVATE   lib)
target_include_directories( saferotp_lib_with_virt PRIVATE   lib)
target_include_directories( saferotp_lib_virt_only PRIVATE   lib)
target_include_directories( saferotp_lib_no_otp    PRIVATE   lib)
target_include_directories( saferotp_lib_default   PRIVATE   inc)
target_include_directories( saferotp_lib_with_virt PRIVATE   inc)
target_include_directories( saferotp_lib_virt_only PRIVATE   inc)
target_include_directories( saferotp_lib_no_otp    PRIVATE   inc)

target_include_directories( saferotp_lib           INTERFACE inc)

# MAGIC -- Use generator expression to define compile definitions
#          This occurs on the INTERFACE to ensure the project linking to saferotp_lib
#          gets the same defines set, since the APIs exposed in the headers depend
#          on these defines.
target_compile_definitions(saferotp_lib
  INTERFACE
    $<$<NOT:$<BOOL:${SAFEROTP_DISABLE_HARDWARE_OTP}>>:SAFEROTP_ENABLE_HARDWARE_HAL>
    $<$<BOOL:${SAFEROTP_ENABLE_VIRTUALIZED_OTP}>:SAFEROTP_ENABLE_VIRTUALIZATION>
)

# TODO: How to enable -Wpendantic only for own files, and not pico_stdlib?
#       pico_stdlib has lots of errors when `-Wpedentic` is enabled
# the following line did not work:
# target_include_directories( saferotp_lib SYSTEM PRIVATE ${pico_stdlib_INCLUDE_DIRS})
# But there's supposed to be a way to mark them as system includes?

set(saferotp_compile_options
        -Wall
        # -Werror
        -O0
        # -Wpedantic
        -Wno-unknown-pragmas
        -Wno-inline
        -Wno-unused-function
)

target_compile_options(     saferotp_lib_default   PRIVATE   ${saferotp_compile_options} )
target_compile_options(     saferotp_lib_with_virt PRIVATE   ${saferotp_compile_options} )
target_compile_options(     saferotp_lib_virt_only PRIVATE   ${saferotp_compile_options} )
target_compile_options(     saferotp_lib_no_otp    PRIVATE   ${saferotp_compile_options} )

target_compile_definitions( saferotp_lib_default   PRIVATE   SAFEROTP_ENABLE_HARDWARE_HAL                                )
target_compile_definitions( saferotp_lib_with_virt PRIVATE   SAFEROTP_ENABLE_HARDWARE_HAL SAFEROTP_ENABLE_VIRTUALIZATION )
target_compile_definitions( saferotp_lib_virt_only PRIVATE                                SAFEROTP_ENABLE_VIRTUALIZATION )
target_compile_definitions( saferotp_lib_no_otp    PRIVATE                                                               )

set_property(TARGET         saferotp_lib_default   PROPERTY  POSITION_INDEPENDENT_CODE ON)
set_property(TARGET         saferotp_lib_with_virt PROPERTY  POSITION_INDEPENDENT_CODE ON)
set_property(TARGET         saferotp_lib_virt_only PROPERTY  POSITION_INDEPENDENT_CODE ON)
set_property(TARGET         saferotp_lib_no_otp    PROPERTY  POSITION_INDEPENDENT_CODE ON)

# The magic of how to link to a different actual library, based on the two options:
if     ( SAFEROTP_ENABLE_HARDWARE_HAL  AND  SAFEROTP_ENABLE_VIRTUALIZATION )
        target_link_libraries(saferotp_lib INTERFACE saferotp_lib_with_virt)
elseif ( SAFEROTP_ENABLE_HARDWARE_HAL                                      )
        target_link_libraries(saferotp_lib INTERFACE saferotp_lib_default)
elseif (                                    SAFEROTP_ENABLE_VIRTUALIZATION )
        target_link_libraries(saferotp_lib INTERFACE saferotp_lib_virt_only)
else(                                                                      )
        target_link_libraries(saferotp_lib INTERFACE saferotp_lib_no_otp)
endif()




# TODO: Enable install so that clients can use find_package()

