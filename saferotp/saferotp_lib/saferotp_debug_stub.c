#include "saferotp_debug_stub.h"

// This example presumes the use of SEGGER RTT
// for debugging input.


// decide where to single-step through the whitelabel process ...
// controlled via RTT (no USB connection required)
void SaferOtp_WaitForKey_impl(void) {
    // clear any prior keypresses
    int t;
    do {
        t = SEGGER_RTT_GetKey();
    } while (t >= 0);

    // Get a new keypress
    (void)SEGGER_RTT_WaitKey();

    // And clear any remaining kepresses (particularly useful for telnet, which does line-by-line input)
    do {
        t = SEGGER_RTT_GetKey();
    } while (t >= 0);

    return;
}



