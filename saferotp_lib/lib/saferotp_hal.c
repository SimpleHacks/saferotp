#include "saferotp.h"
#include "saferotp_debug_stub.h"
#include "saferotp_hal.h"

// These are the wrapper functions that will use hardware / virtualization as appropriate
// The code is a bit messy here due to the #if/#else blocks, so keeping it separate

bool write_raw_wrapper(uint16_t starting_row, const void* buffer, size_t buffer_size) {
    if (!is_valid_otp_range_raw(starting_row, buffer_size)) {
        PRINT_ERROR("OTP WRITE Error: Invalid (start row / raw byte count): 0x%03x %zu\n", starting_row, buffer_size);
        return false;
    }
#if defined(SAFEROTP_ENABLE_VIRTUALIZATION)
    if (virt_is_initialized()) {
        return virt_write_raw_otp_wrapper(starting_row, buffer, buffer_size);
    }
#endif // defined(SAFEROTP_ENABLE_VIRTUALIZATION)
#if defined(SAFEROTP_ENABLE_HARDWARE_HAL)
    return hw_write_raw_otp_wrapper(starting_row, buffer, buffer_size);
#else    
    return false;
#endif // defined(SAFEROTP_ENABLE_HARDWARE_HAL)
}
bool read_raw_wrapper(uint16_t starting_row, void* buffer, size_t buffer_size) {
    if (!is_valid_otp_range_raw(starting_row, buffer_size)) {
        PRINT_ERROR("OTP WRITE Error: Invalid (start row / raw byte count): 0x%03x %zu\n", starting_row, buffer_size);
        return false;
    }
    if (buffer_size % sizeof(uint32_t) != 0u) {
        PRINT_ERROR("OTP VIRT Error: Attempt to read virtualized OTP data with non-aligned size %d\n", buffer_size);
        return false;
    }

#if defined(SAFEROTP_ENABLE_VIRTUALIZATION)
    if (virt_is_initialized()) {
        return virt_read_raw_otp_wrapper(starting_row, buffer, buffer_size);
    }
#endif // defined(SAFEROTP_ENABLE_VIRTUALIZATION)
#if defined(SAFEROTP_ENABLE_HARDWARE_HAL)
    return hw_read_raw_otp_wrapper(starting_row, buffer, buffer_size);
#else
    return false;
#endif // defined(SAFEROTP_ENABLE_HARDWARE_HAL)
}
