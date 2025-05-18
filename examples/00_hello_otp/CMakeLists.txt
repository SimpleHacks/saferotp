
add_executable(00_hello_otp
        hello_otp.c
        )

# pull in common dependencies
target_link_libraries(00_hello_otp pico_stdlib)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(00_hello_otp)
