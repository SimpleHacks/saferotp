#pragma once

#if defined(BP_VER) && (BP_VER != 5)

    // The BusPirate project uses RTT for debug input/output,
    // requiring only to define a default category for a file,
    // followed by the inclusion of teh `debug_rtt.h` header.
    #define BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY  BP_DEBUG_CAT_OTP
    #include "debug_rtt.h"

    // And provide an implementation for the `WAIT_FOR_KEY()`
    // macro that uses RTT to wait for a keypress.
    #define MY_DEBUG_WAIT_FOR_KEY() SaferOtp_WaitForKey_impl()
    void SaferOtp_WaitForKey_impl(void);

#elif defined(SAFEROTP_LIB_DEBUG_OUTPUT_RTT)

    // A later example will show how to integrate RTT
    // in the project, and how to use it.

    // It is NOT required to use RTT. However, it IS required to
    // define the above macros, even if the definition of the macro
    // is empty (disabling the output).
    #error "SAFEROTP_LIB_DEBUG_OUTPUT_RTT is not yet implemented."

#elif defined(SAFEROTP_LIB_DEBUG_OUTPUT_PRINTF)

    #error "SAFEROTP_LIB_DEBUG_OUTPUT_PRINTF is not yet implemented."

#else

    // Remove all debug output because no supported option
    // was specified.
    // The following defines will cause the statements to
    // be removed by the preprocessor ... meaning that
    // no debug output will be generated.
    #define PRINT_FATAL(...)
    #define PRINT_ERROR(...)
    #define PRINT_WARNING(...)
    #define PRINT_INFO(...)
    #define PRINT_VERBOSE(...)
    #define PRINT_DEBUG(...)
    #define MY_DEBUG_WAIT_FOR_KEY()

#endif


