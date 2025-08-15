add_executable(learn_otp
        # main thunk
        main.cpp
        # list of "phantom" raw OTP values that decode
        # incorrectly when read by bootloader and when
        # read from memory-mapped OTP.
        phantom.c
        # main entry ponit
        learn_otp.cpp
        # Zoned OTP creation and testing
        zoned_otp.h
        zoned_otp.cpp
        # routines for ECC creation and correction
        rp2350_otp_ecc.h
        rp2350_otp_ecc.c
        # routines for automatic white-label of the buspirate 5XL, 6, & 7
        bp_whitelabel.h
        bp_whitelabel.c
)

# # enable usb output, disable uart output
# pico_enable_stdio_usb(  learn_otp 1)
# pico_enable_stdio_uart( learn_otp 0)


# pull in common dependencies
target_link_libraries(  learn_otp pico_stdlib  )
target_link_libraries(  learn_otp rtt_lib      )
target_link_libraries(  learn_otp saferotp_lib )

target_compile_options( learn_otp PRIVATE -Wno-pedantic -Wno-sign-conversion -Wno-cast-align -O0 )


# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs( learn_otp )


