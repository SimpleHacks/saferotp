cmake_minimum_required(VERSION 3.21)

project(rtt_lib LANGUAGES C CXX)

option(RTT_LIB_BIND_SYSCALLS "Provides _write syscall bound to RTT" ${PROJECT_IS_TOP_LEVEL})



set(RTT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rtt_src)

add_library(rtt_lib STATIC
    src/debug_rtt.c
    ${RTT_DIR}/RTT/SEGGER_RTT.c
    ${RTT_DIR}/RTT/SEGGER_RTT_printf.c
)

if (RTT_LIB_BIND_SYSCALLS)
    target_sources(rtt_lib PRIVATE
        ${RTT_DIR}/Syscalls/SEGGER_RTT_Syscalls_GCC.c
        ${RTT_DIR}/Syscalls/SEGGER_RTT_Syscalls_IAR.c
        ${RTT_DIR}/Syscalls/SEGGER_RTT_Syscalls_KEIL.c
        ${RTT_DIR}/Syscalls/SEGGER_RTT_Syscalls_SES.c
    )
    target_link_options(rtt_lib INTERFACE -u_write) # Ensures to ignore std implementation
endif()

target_link_libraries(  rtt_lib pico_stdlib  )

# Enable debug output over RTT
target_compile_definitions( rtt_lib PRIVATE DEBUG_OUTPUT_SEGGER_RTT        )    
target_compile_definitions( rtt_lib PRIVATE DEBUG_INPUT_SEGGER_RTT         )   

# For TinyUSB CDC, might be able to enable debug input/output
# over the specified USB CDC interface.
# (default interface is zero when only one CDC interface exists)
# target_compile_definitions( rtt_lib PRIVATE DEBUG_OUTPUT_TINYUSB_CDC_ITF=0 )           
# target_compile_definitions( rtt_lib PRIVATE DEBUG_INPUT_TINYUSB_CDC_ITF=0  )          

target_include_directories(rtt_lib PUBLIC
    ${RTT_DIR}/RTT
    inc
)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  # GCC emits warnings for RTT code ... just disable these here until modify source to avoid these
  target_compile_options(rtt_lib PRIVATE -Wno-pedantic -Wno-sign-conversion -Wno-cast-align -O0)
endif ()

