#pragma once

// This header file must define the following macros,
// which are used by the SaferOTP library:
//
// void PRINT_FATAL(...);
// void PRINT_ERROR(...);
// void PRINT_WARNING(...);
// void PRINT_INFO(...);
// void PRINT_VERBOSE(...);
// void PRINT_DEBUG(...);
// void MY_DEBUG_WAIT_FOR_KEY(void);


// The BusPirate project uses RTT for debug input/output.
// It uses the following header to define these PRINT_* macros.
// e.g., #define PRINT_FATAL(...)   BP_DEBUG_PRINT(BP_DEBUG_LEVEL_FATAL,   BP_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
#define BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY  BP_DEBUG_CAT_OTP


// If you get an error here about not finding the header file,
// review the notes on the few changes needed to integrate this
// library into your project.
// It is NOT required to use RTT. However, it IS required to
// define the above macros, even if the definition of the macro
// is empty (disabling the output).
#include "debug_rtt.h"

// And provide an implementation for the `WAIT_FOR_KEY()`
// macro that uses RTT to wait for a keypress.
// It is NOT required to use RTT. However, it IS required to
// define the above macros, even if the definition of the macro
// is empty (disabling the wait for keypress).
#define MY_DEBUG_WAIT_FOR_KEY() SaferOtp_WaitForKey_impl()
void SaferOtp_WaitForKey_impl(void);
