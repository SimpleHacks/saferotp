#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "saferotp.h"             // after move to CMakeLists.txt option enabling, maybe remove this header?
#include "saferotp_debug_stub.h"
#include "saferotp_hal.h"


#if defined(SAFEROTP_ENABLE_HARDWARE_HAL)

// Set this global variable anywhere in the code
// to immediately wait for keypress prior to writing
// to the OTP fuses.  This will catch ***ALL*** writes
// that use this library.
static volatile bool g_WaitForKey_otp_rw = false;
#define WAIT_FOR_KEY()                 \
    do {                               \
        if (g_WaitForKey_otp_rw) {     \
            MY_DEBUG_WAIT_FOR_KEY();   \
        }                              \
    } while (0)

typedef struct _MAP_BOOTROM_RESULT_TO_STRING {
    int bootrom_code;
    const char * string; // NULL entry ends the array
} MAP_BOOTROM_RESULT_TO_STRING;
const MAP_BOOTROM_RESULT_TO_STRING bootrom_result_map[] = {
    { .bootrom_code = BOOTROM_OK,                             .string = "BOOTROM_OK"                             },
    { .bootrom_code = BOOTROM_ERROR_NOT_PERMITTED,            .string = "BOOTROM_ERROR_NOT_PERMITTED"            },
    { .bootrom_code = BOOTROM_ERROR_INVALID_ARG,              .string = "BOOTROM_ERROR_INVALID_ARG"              },
    { .bootrom_code = BOOTROM_ERROR_INVALID_ADDRESS,          .string = "BOOTROM_ERROR_INVALID_ADDRESS"          },
    { .bootrom_code = BOOTROM_ERROR_BAD_ALIGNMENT,            .string = "BOOTROM_ERROR_BAD_ALIGNMENT"            },
    { .bootrom_code = BOOTROM_ERROR_INVALID_STATE,            .string = "BOOTROM_ERROR_INVALID_STATE"            },
    { .bootrom_code = BOOTROM_ERROR_BUFFER_TOO_SMALL,         .string = "BOOTROM_ERROR_BUFFER_TOO_SMALL"         },
    { .bootrom_code = BOOTROM_ERROR_PRECONDITION_NOT_MET,     .string = "BOOTROM_ERROR_PRECONDITION_NOT_MET"     },
    { .bootrom_code = BOOTROM_ERROR_MODIFIED_DATA,            .string = "BOOTROM_ERROR_MODIFIED_DATA"            },
    { .bootrom_code = BOOTROM_ERROR_INVALID_DATA,             .string = "BOOTROM_ERROR_INVALID_DATA"             },
    { .bootrom_code = BOOTROM_ERROR_NOT_FOUND,                .string = "BOOTROM_ERROR_NOT_FOUND"                },
    { .bootrom_code = BOOTROM_ERROR_UNSUPPORTED_MODIFICATION, .string = "BOOTROM_ERROR_UNSUPPORTED_MODIFICATION" },
    { .bootrom_code = BOOTROM_ERROR_LOCK_REQUIRED,            .string = "BOOTROM_ERROR_LOCK_REQUIRED"            },
    { .bootrom_code = 0,                                      .string = NULL                                     },
};
const char* map_bootrom_result_to_string(int bootrom_code) {
    const MAP_BOOTROM_RESULT_TO_STRING * entry = &bootrom_result_map[0];
    while (NULL != entry->string) {
        if (bootrom_code == entry->bootrom_code) {
            return entry->string;
        }
    }
    return NULL;
}

// returns TRUE on successful write, FALSE on failures
bool hw_write_raw_otp_wrapper(uint16_t starting_row, const void* buffer, size_t buffer_size) {
    // NOTE: rom_func_otp_access() ensures necessary bootrom locks are acquired.
    //       Memory-mapped regions are *NOT* protected from simultaneous access, and
    //       the documentation explicitly warns that the (opaque) Synopsys OTP IP block
    //       requires serializing all access to the OTP.
    otp_cmd_t cmd;
    cmd.flags = starting_row;
    cmd.flags |= OTP_CMD_WRITE_BITS;
    PRINT_DEBUG("OTP WRITE Debug: about to write OTP starting at row %03x %d bytes (0x%x rows\n", starting_row, buffer_size, (buffer_size/sizeof(uint32_t)));
    WAIT_FOR_KEY();
    int r = rom_func_otp_access((uint8_t*)buffer, buffer_size, cmd);
    if (r != BOOTROM_OK) {
        const char* s = map_bootrom_result_to_string(r);
        if (NULL == s) {
            s = "";
        }
        PRINT_ERROR(
            "OTP WRITE Error: Failed to write raw OTP values starting at row %03x (%d bytes / 0x%x rows), error %d (0x%x) %s\n",
            starting_row,
            buffer_size, (buffer_size/sizeof(uint32_t)),
            r, r, s
        );
    }
    return (BOOTROM_OK == r);
}
bool hw_read_raw_otp_wrapper(uint16_t starting_row, void* buffer, size_t buffer_size) {
    // TODO: Check BOOTLOCK7 to determine if bootrom will require ownership of BOOTLOCK2 (OTP)
    //       This would return error BOOTROM_ERROR_LOCK_REQUIRED (-19) if this ever occurs.
    otp_cmd_t cmd;
    cmd.flags = starting_row;
    int r = rom_func_otp_access((uint8_t*)buffer, buffer_size, cmd);
    PRINT_DEBUG("OTP READ Debug: about to write OTP starting at row %03x %d bytes (0x%x rows\n", starting_row, buffer_size, (buffer_size/sizeof(uint32_t)));
    if (r != BOOTROM_OK) {
        const char* s = map_bootrom_result_to_string(r);
        if (NULL == s) {
            s = "";
        }
        PRINT_ERROR(
            "OTP READ Error: Failed to write raw OTP values starting at row %03x (%d bytes / 0x%x rows), error %d (0x%x) %s\n",
            starting_row,
            buffer_size, (buffer_size/sizeof(uint32_t)),
            r, r, s
        );
    }
    return (BOOTROM_OK == r);
}


#endif // defined(SAFEROTP_ENABLE_HARDWARE_HAL)
