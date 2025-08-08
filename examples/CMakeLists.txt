
cmake_minimum_required(VERSION 3.13)

# Gets the current directory name, verifies only allowed characters in the name, and
# sets the variable `EXECUTABLE_FROM_DIRECTORY_NAME` to that cleaned directory name.
# Callers can then use that variable (the directory name) for the executable, etc.
# This helps the examples have fewer changes between their CMakeLists.txt files,
# enhancing the clarity of what the minimum changes need to be between steps.
function(derive_executable_name_from_directory_name)
    # 1. get the last element of the source dir
    get_filename_component(_dir_name
        ${CMAKE_CURRENT_SOURCE_DIR}
        NAME
    )

    # 2. enforce allowed characters only
    if(NOT _dir_name MATCHES "^[A-Za-z0-9_]+$")
        message(FATAL_ERROR
            "Invalid directory name '${_dir_name}'.\n"
            "Allowed characters are upper-/lowercase letters, digits, and underscore only."
        )
    endif()

    # 3. export into the calling scope
    set(EXECUTABLE_FROM_DIRECTORY_NAME "${_dir_name}" PARENT_SCOPE)
endfunction()

# Not much to do here except list subdirectories
add_subdirectory_exclude_platforms( 00_hello_otp             host rp2040 )
add_subdirectory_exclude_platforms( 01_link_saferotp_lib     host rp2040 )

