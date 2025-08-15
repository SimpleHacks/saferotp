
cmake_minimum_required(VERSION 3.13)

if (NOT DEFINED PICO_PLATFORM)
    set(PICO_PLATFORM "rp2350")
endif()

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)
# # Can also include the "extras" not yet ready for inclusion in the SDK
# include(pico_extras_import.cmake)

# Give a unique name to the root project
project(my_root_pico_project)

# Initialize the SDK
pico_sdk_init()

# Provide an add_subdirectory() that excludes certain platforms (e.g., host, rp2040, rp2350)
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


# What subdirectories have projects / code?
add_subdirectory(libs)
add_subdirectory_exclude_platforms(learn_otp          host rp2040)
