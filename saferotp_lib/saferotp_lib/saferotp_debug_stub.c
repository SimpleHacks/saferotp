#include "saferotp_debug_stub.h"

// Keep edits in lockstep with `saferotp_debug_stub.h`
#if defined(BP_VER) && (BP_VER != 5)

    // Hack for integration with BusPirate, the first
    // project to use this library.  Early adoption has
    // its privileges....

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

#elif defined(SAFEROTP_LIB_DEBUG_OUTPUT_RTT)
    #error "SAFEROTP_LIB_DEBUG_OUTPUT_RTT is not yet implemented."
#elif defined(SAFEROTP_LIB_DEBUG_OUTPUT_PRINTF)
    #error "SAFEROTP_LIB_DEBUG_OUTPUT_PRINTF is not yet implemented."
#else
    // All debug macros were defined to nothing, and thus
    // nothing is required for this compilation unit.
#endif


