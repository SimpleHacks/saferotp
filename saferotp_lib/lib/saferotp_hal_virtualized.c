#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>               // for memset()
#include "saferotp.h"             // after move to CMakeLists.txt option enabling, maybe remove this header?
#include "saferotp_ecc.h"
#include "saferotp_debug_stub.h"
#include "saferotp_hal.h"

#if defined(SAFEROTP_ENABLE_VIRTUALIZATION)

// This file contains functions that are internal implementation details.
// In particular, this provides the implementation for the virtualized OTP APIs.
// 


typedef struct _BP_VIRTUALIZED_OTP_BUFFER {
    SAFEROTP_RAW_READ_RESULT rows[NUM_OTP_ROWS]; // 0x1000 == 4096 rows, requiring 4 bytes each == 16k statically allocated buffer (!!)
} BP_VIRTUALIZED_OTP_BUFFER;
static BP_VIRTUALIZED_OTP_BUFFER g_virtual_otp = { 0 };
static bool g_virtual_otp_initialized = false;


bool virt_initialize() {
    // Initialize the virtualized OTP pages
    if (g_virtual_otp_initialized) {
        return false;
    }
    memset(&g_virtual_otp, 0, sizeof(g_virtual_otp));
    g_virtual_otp_initialized = true;
    return true;
}
void virt_log_dump_rows_encoded_to_fail_reads(uint16_t starting_row, uint16_t row_count) {
    if (row_count > NUM_OTP_ROWS - starting_row) {
        row_count = NUM_OTP_ROWS - starting_row;
    }
    uint16_t end_row = starting_row + row_count;
    for (uint16_t row = starting_row; row < end_row; ++row) {
        if (g_virtual_otp.rows[row].is_error) {
            PRINT_WARNING("OTP VIRT Warning: -->  Row 0x%03x (%02x:%02x) failed to read\n",
                row,
                (row / NUM_OTP_PAGE_ROWS), (row % NUM_OTP_PAGE_ROWS)
            );
        }
    }
}
bool virt_override_restore(uint16_t starting_row, const void* buffer, size_t buffer_size) {
    // callers can then save/restore OTP state, such as from storage / file system
    if (!is_valid_otp_range_raw(starting_row, buffer_size)) {
        PRINT_ERROR("OTP VIRT Error: Invalid (start row / raw byte count): 0x%03x %zu\n", starting_row, buffer_size);
        return false;
    }
    // NOTE: This simply replaces the values, even if doing so would not otherwise have been a valid write.
    //       Allows resetting pages to zero (bits from 1 -> 0), bypasses permissions, etc.
    memcpy(&g_virtual_otp.rows[starting_row], buffer, buffer_size);
    return true;
}
bool virt_override_save(uint16_t starting_row, void* buffer, size_t buffer_size) {
    // callers can then save/restore OTP state, such as from storage / file system
    if (!is_valid_otp_range_raw(starting_row, buffer_size)) {
        PRINT_ERROR("OTP VIRT Error: Invalid (start row / raw byte count): 0x%03x %zu\n", starting_row, buffer_size);
        return false;
    }
    memcpy(buffer, &g_virtual_otp.rows[starting_row], buffer_size);
    return true;
}
bool virt_is_initialized() {
    return g_virtual_otp_initialized;
}


bool virt_write_raw_otp_wrapper(uint16_t starting_row, const void* buffer, size_t buffer_size) {
    if (!g_virtual_otp_initialized) {
        PRINT_ERROR("OTP VIRT Error: Attempt to write virtualized OTP data without initialization\n");
        return false;
    }
    if (NULL == buffer) {
        PRINT_FATAL("Function call to virt_write_raw_otp_wrapper() with NULL buffer\n");
        return false;
    }

    // belt and suspenders ... even if caller did this
    if (!is_valid_otp_range_raw(starting_row, buffer_size)) {
        PRINT_ERROR("OTP VIRT WRITE Error: Invalid (start row / raw byte count): 0x%03x %zu\n", starting_row, buffer_size);
        return false;
    }
    // TODO: Check BOOTLOCK7 to determine if bootrom will require ownership of BOOTLOCK2 (OTP)
    size_t row_count = buffer_size / sizeof(uint32_t);
    // process each row in order (per RP2350 datasheet ... )
    for (size_t i = 0; i < row_count; ++i) {
        // TODO: Any permissions checks, when implemented....

        // verify the existing value was readable ... else refuse to modify it.
        SAFEROTP_RAW_READ_RESULT *current = &g_virtual_otp.rows[starting_row + i];
        if (current->is_error) {
            PRINT_ERROR("OTP VIRT WRITE Error: Attempt to write virtualized OTP row 0x%03x, which previously failed to read (start row %03x, buffer size %zx)\n", starting_row+i, starting_row, buffer_size);
            return false;
        }
        // OTP bits can only transition from zero to one (0 --> 1).
        // Verify none of the bits would transition from (1 --> 0).
        const SAFEROTP_RAW_READ_RESULT *new_value = (const SAFEROTP_RAW_READ_RESULT *)(  &(((const uint32_t*)buffer)[i]) );
        if ((current->as_uint32 | new_value->as_uint32) != new_value->as_uint32) {
            PRINT_ERROR("OTP VIRT WRITE Error: Attempt to write virtualized OTP row 0x%03x from %06x -> %06x, which would flip bits from 0 --> 1 (start row %03x, buffer size %zx)\n",
                starting_row+i,
                current->as_uint32, new_value->as_uint32,
                starting_row, buffer_size
            );
            return false;
        }
        // Update the individual row's data
        current->as_uint32 = new_value->as_uint32;
    }
    return true;
}
bool virt_read_raw_otp_wrapper(uint16_t starting_row, void* buffer, size_t buffer_size) {
    if (NULL == buffer) {
        PRINT_FATAL("Function call to virt_read_raw_otp_wrapper() with NULL buffer\n");
        return false;
    }
    memset(buffer, 0, buffer_size);

    if (!g_virtual_otp_initialized) {
        PRINT_ERROR("OTP VIRT Error: Attempt to write virtualized OTP data without initialization\n");
        return false;
    }
    // belt and suspenders ... even if caller did this
    if (!is_valid_otp_range_raw(starting_row, buffer_size)) {
        PRINT_ERROR("OTP VIRT READ Error: Invalid (start row / raw byte count): 0x%03x %zu\n", starting_row, buffer_size);
        return false;
    }
    // TODO: Check BOOTLOCK7 to determine if bootrom will require ownership of BOOTLOCK2 (OTP)
    size_t row_count = buffer_size / sizeof(uint32_t);
    // process each row in order (per RP2350 datasheet ... )
    for (size_t i = 0; i < row_count; ++i) {
        // TODO: Any permissions checks, when implemented....

        // verify the existing value was readable ... else return an error
        SAFEROTP_RAW_READ_RESULT *current = &g_virtual_otp.rows[starting_row + i];
        if (current->is_error) {
            PRINT_ERROR("OTP VIRT READ Error: Attempt to write virtualized OTP row 0x%03x, which previously failed to read (start row %03x, buffer size %zx)\n", starting_row+i, starting_row, buffer_size);
            return false; // report the error
        }
        // Else return the value from the virtualized buffer
        uint32_t * to_write = &(((uint32_t*)buffer)[i]);
        *to_write = current->as_uint32;
    }
    return true;
}

#endif // defined(SAFEROTP_ENABLE_VIRTUALIZATION)

#if defined(SAFEROTP_ENABLE_HARDWARE_HAL) && defined(SAFEROTP_ENABLE_VIRTUALIZATION)
bool virt_fill_rows_from_hardware(uint16_t starting_row, uint16_t row_count) {
    if (starting_row > NUM_OTP_ROWS) {
        PRINT_ERROR("OTP row 0x%03x is too large (max %03x)", starting_row, NUM_OTP_ROWS-1);
        return false;
    }
    if (row_count > NUM_OTP_ROWS - starting_row) {
        row_count = NUM_OTP_ROWS - starting_row;
    }
    uint16_t end_row = starting_row + row_count;
    size_t error_count = 0u;
    for (uint16_t row = starting_row; row < end_row; ++row) {
        if (!hw_read_raw_otp_wrapper(row, &g_virtual_otp.rows[row], sizeof(SAFEROTP_RAW_READ_RESULT))) {
            // can easily scan for errors later by just checking if any of the high bits were set
            g_virtual_otp.rows[row].as_uint32 = 0xFFFFFFFFu; // ensure the stored value is an error
            error_count++;
        }
    }
    return error_count == 0u;
}
bool virt_fill_page_from_hardware(uint16_t page) {
    if (page >= NUM_OTP_PAGES) {
        PRINT_ERROR("OTP page %2d (%02x) is invalid (max %2d (%02x))\n", page, page, NUM_OTP_PAGES-1u, NUM_OTP_PAGES-1u);
        return false;
    }
    // read the requested page of OTP into the virtualized buffer
    return virt_fill_rows_from_hardware(page * NUM_OTP_PAGE_ROWS, NUM_OTP_PAGE_ROWS);
}
#endif // defined(SAFEROTP_ENABLE_HARDWARE_HAL) && defined(SAFEROTP_ENABLE_VIRTUALIZATION)

// //////////////////////////////////////////////////////////////////////
// Above this point are the internal implementation details.
// Below this point are the public APIs

#if defined(SAFEROTP_ENABLE_VIRTUALIZATION)
bool saferotp_virtualization_init() {
    if (virt_is_initialized()) {
        PRINT_ERROR("OTP VIRT Error: init() should only be called once\n");
        return false; 
    }
    return virt_initialize();
}
bool saferotp_virtualization_restore(uint16_t starting_row, const void* buffer, size_t buffer_size) {
    if (!virt_is_initialized()) {
        PRINT_ERROR("OTP VIRT Error: init() must be called before use of virtualization\n");
        return false; 
    }
    return virt_override_restore(starting_row, buffer, buffer_size);
}
bool saferotp_virtualization_save(uint16_t starting_row, void* buffer, size_t buffer_size) {
    if (!virt_is_initialized()) {
        PRINT_ERROR("OTP VIRT Error: init() must be called before use of virtualization\n");
        return false; 
    }
    return virt_override_save(starting_row, buffer, buffer_size);
}
#endif // defined(SAFEROTP_ENABLE_VIRTUALIZATION)


#if defined(SAFEROTP_ENABLE_HARDWARE_HAL) && defined(SAFEROTP_ENABLE_VIRTUALIZATION)
bool saferotp_virtualization_restore_all_pages_from_hardware() {
    if (!virt_is_initialized()) {
        PRINT_ERROR("OTP VIRT Error: init() must be called before use of virtualization\n");
        return false; 
    }
    bool result = true;
    for (uint16_t i = 0; i < NUM_OTP_PAGES; ++i) {
        result = result && virt_fill_page_from_hardware(i);
    }
    if (!result) {
        virt_log_dump_rows_encoded_to_fail_reads(0, NUM_OTP_ROWS);
    }
    return result;
}
bool saferotp_virtualization_restore_page_from_hardware(uint16_t page) {
    if (!virt_is_initialized()) {
        PRINT_ERROR("OTP VIRT Error: init() must be called before use of virtualization\n");
        return false; 
    }
    bool result = virt_fill_page_from_hardware(page);
    if (!result) {
        virt_log_dump_rows_encoded_to_fail_reads(page, NUM_OTP_PAGE_ROWS);
    }
    return result;
}
#endif // defined(SAFEROTP_ENABLE_HARDWARE_HAL) && defined(SAFEROTP_ENABLE_VIRTUALIZATION)
