#pragma once

// macros used elsewhere
#define PRINT_FATAL(...)
#define PRINT_ERROR(...)
#define PRINT_WARNING(...)
#define PRINT_INFO(...)
#define PRINT_VERBOSE(...)
#define PRINT_DEBUG(...)
#define MY_DEBUG_WAIT_FOR_KEY()

#if 0 // non-library debug code ... e.g., using Segger RTT for input / output
//#include "debug_rtt.h"
//
// During development, it's REALLY useful to force the code to single-step through this process.
// To support this, this file has code that uses waits for the RTT terminal to accept input.
//
// To use this is a TWO STEP process:
// 1. In the file to wait, define a unique variable:
//    static volatile bool g_WaitForKey_uniquifier = false;
// 2. Define WAIT_FOR_KEY() to something similar to the following:
//     do {
//        if (g_WaitForKey_uniquifier) {
//            MY_DEBUG_WAIT_FOR_KEY();
//        }
//     } while (0);
// 3. Adjust `g_WaitForKey_uniquifier` to wait / not wait
//    in corresponding code locations.
//
// Because OTP fuses can only transition from 0 -> 1, this capability is critical to
// minimizing the number of RP2350 chips with invalid data during development.

#endif

