cmake_minimum_required(VERSION 3.13)

# Define EXECUTABLE_FROM_DIRECTORY_NAME based on the current directory name.
derive_executable_name_from_directory_name()

add_executable(
        ${EXECUTABLE_FROM_DIRECTORY_NAME}
        main.c
        hexdump.h
        hexdump.c
        )

# pull in common dependencies
target_link_libraries(
        ${EXECUTABLE_FROM_DIRECTORY_NAME}
        pico_stdlib
        saferotp_lib
        )

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(
        ${EXECUTABLE_FROM_DIRECTORY_NAME}
        )
