### NOTE: This is based on the CMakeLists.txt from the
###       Raspberry Pi Pico SDK v2.1.0

# indentation used to allow collapsing sections in VSCode


### OTP is only available on the RP2350 platform so just hard-code it
    set(PICO_PLATFORM "rp2350")

### Substantially from pico-examples/CMakeLists.txt
    cmake_minimum_required(VERSION 3.12)

    # Pull in SDK (must be before project)
    include(pico_sdk_import.cmake)
    
    # Commented out as not required for this project
    # include(pico_extras_import_optional.cmake)

    # Define a top-level project because it appears to be required?
    project(saferotp_umbrella C CXX ASM)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)

    if (PICO_SDK_VERSION_STRING VERSION_LESS "2.1.0")
        message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.1.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
    endif()

    # If you want debug output from USB (pass -DPICO_STDIO_USB=1) this ensures you don't lose any debug output while USB is set up
    if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
        set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
    endif()

    # Initialize the SDK
    pico_sdk_init()

    function(add_subdirectory_exclude_platforms NAME)
        if (ARGN)
            if (PICO_PLATFORM IN_LIST ARGN)
                message("Skipping ${NAME} example which is unsupported on this platform")
                return()
            endif()
            foreach(PATTERN IN LISTS ARGN)
                string(REGEX MATCH "${PATTERN}" MATCHED "${PICO_PLATFORM}")
                if (MATCHED)
                    message("Skipping ${NAME} example which is unsupported on this platform")
                    return()
                endif()
            endforeach()
        endif()
        add_subdirectory(${NAME})
    endfunction()

### Very little needs to be done here
    add_subdirectory( saferotp_lib )
    add_subdirectory( examples     )

# Raspberry Pico SDK disables a bunch of warnings ... Too bad!
    # add_compile_options(-Wall
    #         -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
    #         -Wno-unused-function # we have some for the docs that aren't called
    #         )
    # if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    #     add_compile_options(-Wno-maybe-uninitialized)
    # endif()


