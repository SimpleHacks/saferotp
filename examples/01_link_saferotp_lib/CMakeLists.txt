
add_executable(01_link_saferotp_lib
        hello_otp.c
        )

# pull in common dependencies
target_link_libraries(01_link_saferotp_lib pico_stdlib)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(01_link_saferotp_lib)

