#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "saferotp.h"

// Redirects to virtualized OTP if enabled and initialized,
// else writes to hardware OTP (if enabled),
// else will return false to all calls.
bool write_raw_wrapper(uint16_t starting_row, const void* buffer, size_t buffer_size);
// Redirects to virtualized OTP if enabled and initialized,
// else reads from hardware OTP (if enabled),
// else will return false to all calls.
bool read_raw_wrapper(uint16_t starting_row, void* buffer, size_t buffer_size);

bool is_valid_otp_range_raw(uint16_t starting_row, size_t raw_byte_count);
bool is_valid_otp_row_range(uint16_t starting_row, size_t row_count);

#if defined(SAFEROTP_ENABLE_HARDWARE_HAL)

#include "pico/bootrom.h" // defines rom_func_otp_access(), NUM_OTP_ROWS, NUM_OTP_PAGES
bool hw_write_raw_otp_wrapper(uint16_t starting_row, const void* buffer, size_t buffer_size);
bool hw_read_raw_otp_wrapper(uint16_t starting_row, void* buffer, size_t buffer_size);

#else

// Define these here anytime the hardware layer is not enabled
#define NUM_OTP_PAGES (64u)
#define NUM_OTP_ROWS  (64u*64u)

__attribute__((deprecated("Hardware HAL not enabled")))
inline bool hw_write_raw_otp_wrapper(uint16_t, const void*, size_t) {
    return false;
}

__attribute__((deprecated("Hardware HAL not enabled")))
inline bool hw_read_raw_otp_wrapper(uint16_t, void*, size_t) {
    return false;
}

#endif

#if defined(SAFEROTP_ENABLE_VIRTUALIZATION)

bool virt_initialize();
void virt_log_dump_rows_encoded_to_fail_reads(uint16_t starting_row, uint16_t row_count);
bool virt_override_restore(uint16_t starting_row, const void* buffer, size_t buffer_size);
bool virt_override_save(uint16_t starting_row, void* buffer, size_t buffer_size);
bool virt_is_initialized();
bool virt_write_raw_otp_wrapper(uint16_t starting_row, const void* buffer, size_t buffer_size);
bool virt_read_raw_otp_wrapper(uint16_t starting_row, void* buffer, size_t buffer_size);

#else

__attribute__((deprecated("Virtualization HAL not enabled")))
inline bool virt_initialize() { return false; }

__attribute__((deprecated("Virtualization HAL not enabled")))
inline bool virt_override_restore(uint16_t starting_row, const void* buffer, size_t buffer_size) { return false; }

__attribute__((deprecated("Virtualization HAL not enabled")))
inline bool virt_override_save(uint16_t starting_row, void* buffer, size_t buffer_size) { return false; }

__attribute__((deprecated("Virtualization HAL not enabled")))
inline bool virt_is_initialized() { return false; }

__attribute__((deprecated("Virtualization HAL not enabled")))
inline bool virt_write_raw_otp_wrapper(uint16_t starting_row, const void* buffer, size_t buffer_size) { return false; }

__attribute__((deprecated("Virtualization HAL not enabled")))
inline bool virt_read_raw_otp_wrapper(uint16_t starting_row, void* buffer, size_t buffer_size) { return false; }

#endif // defined(SAFEROTP_ENABLE_VIRTUALIZATION)


#if defined(SAFEROTP_ENABLE_HARDWARE_HAL) && defined(SAFEROTP_ENABLE_VIRTUALIZATION)

bool virt_fill_rows_from_hardware(uint16_t starting_row, uint16_t row_count);
bool virt_fill_page_from_hardware(uint16_t page);

#else

__attribute__((deprecated("Virtualization HAL or Hardware HAL not enabled")))
inline bool virt_fill_rows_from_hardware(uint16_t starting_row, uint16_t row_count) { return false; }
__attribute__((deprecated("Virtualization HAL or Hardware HAL not enabled")))
inline bool virt_fill_page_from_hardware(uint16_t page) { return false; }

#endif // defined(SAFEROTP_ENABLE_HARDWARE_HAL) && defined(SAFEROTP_ENABLE_VIRTUALIZATION)
