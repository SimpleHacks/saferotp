
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "pico/stdlib.h" // required for get_core_num()
#include "saferotp.h"
#include "saferotp_debug_stub.h"

static volatile bool g_WaitForKey_otpdir = false;
#define WAIT_FOR_KEY()                 \
    do {                               \
        if (g_WaitForKey_otpdir) {    \
            MY_DEBUG_WAIT_FOR_KEY();   \
        }                              \
    } while (0)

#warning "directory entry functionality is still in prototyping phase" 

// ======================================================================
// the actual structure for the OTP DIRENTRY should remain opaque to callers.
// this allows us to fix any architectural oversights later in a backwards-compatible
// manner, without changing most client code.
typedef struct _X_DIRENTRY {
    union {
        uint8_t  as_uint8_t[8];
        uint16_t as_uint16[4];
 
        struct {
            SAFEROTP_OTPDIR_ENTRY_TYPE entry_type;
            union {
                // For SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE
                // All fields must be zero.
                // If the entry_type value is also zero, then by definition the CRC16 value is also zero.
                struct {
                    uint16_t     must_be_zero[2];
                } none; // SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE
                // For SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RAW
                // Start row is the first row storing the data.
                // The row count must be non-zero.
                // For buffer allocation purposes, the total data size is 32-bits for every row.
                // (See raw data ... 24 bits returned as a 32-bit word).
                struct {
                    uint16_t     start_row;   // first row of the data
                    uint16_t     row_count;   // count of rows used for the data, each row considered to be 32-bits in size (e.g., Raw 24 bits)
                } raw_data;
                // For SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_BYTE3X
                // Start row is the first row storing the data.
                // The row count must be non-zero.
                // For buffer allocation purposes, the total data size is equal to the row count (one byte per row).
                struct {
                    uint16_t     start_row;   // first row of the data
                    uint16_t     row_count;   // number of rows used for the data, each row storing a single byte of data
                } byte3x_data;
                // For SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT3
                // Start row is the first row storing the data.
                // The row count must be a non-zero multiple of 3.
                // For buffer allocation purposes, the total data size is 32-bit for every three rows.
                // (See raw data ... 24 bits in a 32-bit word).
                struct {
                    uint16_t     start_row;   // first row of the data
                    uint16_t     row_count;   // MUST be a multiple of 3 ... as each row must be duplicated three times; every 3 rows considered to store 32-bits in size (e.g., Raw 24 bits)
                } rbit3_data;
                // For SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT8
                // Start row is the first row storing the data.
                // The row count must be a non-zero multiple of 8.
                // For buffer allocation purposes, the total data size is 32-bits for every eight rows.
                // (See raw data ... 24 bits in a 32-bit word).
                struct {
                    uint16_t     start_row;   // first row of the data
                    uint16_t     row_count;   // MUST be a multiple of 8 ... as each row must be duplicated eight times; every 8 rows considered to store 32-bits in size (e.g., Raw 24 bits)
                } rbit8_data;
                // For SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC and SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC_ASCII_STRING
                // Start row is the first row storing the data.
                // The byte count must be non-zero.
                // For buffer allocation purposes, the total data size is stored directly in the OTP directory entry.
                // The row count is the byte count divided by 2 (rounded up).
                struct {
                    uint16_t     start_row;  // first row of the data
                    uint16_t     byte_count; // count of valid bytes in the data (including trailing NULL for strings)
                } ecc_data;
                // For SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_EMBEDED_IN_DIRENTRY
                // Data is stored embedded directly in the directory entry.
                // For buffer allocation purposes, there is always a uint32_t of data.
                // CAUTION: When implementing, must manually construct / decontruct the uint32_t value,
                // due to alignment requirements, cannot declare this as a uint32_t directly in the structure
                // (without pack pragmas or the like).
                struct {
                    uint8_t      data[4];    // Up to 32-bits of data stored directly in the directory entry
                } embedded_data;
            };
            uint16_t             crc16;
        }; // common entry_type and CRC16, with union for various encodings into the directory entry
    };
} X_DIRENTRY;
static_assert(sizeof(X_DIRENTRY) == (4 * sizeof(uint16_t)));
#define xROWS_PER_DIRENTRY 4u


#if RPI_PLATFORM == RP2350
    #define xCORE_COUNT             (2u)
    #define xFIRST_USER_CONTENT_ROW ((uint16_t)0x0C0u)
    #define xUSER_CONTENT_ROW_COUNT ((uint16_t)(0xF40u - xFIRST_USER_CONTENT_ROW))
#else
    #error "unsupported platform ... may have different count of cores"
#endif

// Define a hard-coded location at which to start searching for directory entries.
#define xSTART_ROW ((uint16_t)(xFIRST_USER_CONTENT_ROW + xUSER_CONTENT_ROW_COUNT - xROWS_PER_DIRENTRY))


// define iterator state so it's invalid (needs reset) when zero-initialized.
typedef struct _X_ITERATOR_STATE {
    uint16_t        current_otp_row_start;                // zero is not a valid OTP row for an OTP_DIRENTRY item
    X_DIRENTRY current_entry;                  // all-zero data is indicative of end-of-directory
    bool            entry_validated;                      // zero means entry not yet validated (entry type, crc, start row, data length)
    bool            should_try_next_row_if_not_validated; // set to true when read fails with ECC error
} X_ITERATOR_STATE;

// One "current" OTP_DIRENTRY location is stored for each core.
// This avoids the need for cores to synchronize their read-only operations
// on the OTP directory.
static X_ITERATOR_STATE x_current_directory_entry[xCORE_COUNT] = {};

#pragma region    // Basic CRC16
// It is critical that this CRC return a value of 0x0000u
// when the data provided to it is all-zero, to account for
// an unwritten area being considered an indication of the
// end-of-directory (appendable).
static uint16_t crc16_table_a001[] = {
	0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
	0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
	0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
	0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
	0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
	0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
	0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
	0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
	0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
	0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
	0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
	0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
	0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
	0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
	0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
	0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
	0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
	0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
	0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
	0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
	0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
	0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
	0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
	0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
	0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
	0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
	0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
	0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
	0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
	0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
	0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
	0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
};
static inline uint16_t crc16_calculate(const void* buffer, size_t buffer_len) {
    const uint8_t * data = (const uint8_t*)buffer;
    uint16_t crc = 0x0000u;
    for (size_t i = 0; i < buffer_len; ++i) {
        uint8_t idx = crc ^ *data;
        ++data;
        crc = (crc >> 8) ^ crc16_table_a001[idx];
    }
    return crc;
}
#pragma endregion // Basic CRC16


#pragma region    // OTP Directory Data Encoding Type Validation functions
static bool x_otpdir_is_valid_user_content_row_range(uint16_t start_row, uint16_t row_count) {
    if (start_row < xFIRST_USER_CONTENT_ROW) {
        PRINT_WARNING("Validation of OTPDIR entry as RAW failed: start row (%03x) is less than first user content row %03x",
            start_row, xFIRST_USER_CONTENT_ROW
        );
        return false;
    }
    uint16_t allowed_row_count = xUSER_CONTENT_ROW_COUNT - (start_row - xFIRST_USER_CONTENT_ROW);
    if (row_count > allowed_row_count) {
        PRINT_WARNING("Validation of OTPDIR entry as RAW failed: row count (%03x) exceeds %03x maximum rows for starting at row %03x (user content starts at %03x, %03x rows)",
            row_count, allowed_row_count, start_row, xFIRST_USER_CONTENT_ROW, xUSER_CONTENT_ROW_COUNT
        );
        return false;
    }
    return true;
}

// SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE
static bool x_otpdir_entry_appears_valid_none(const X_DIRENTRY* entry) {
    if (entry->entry_type.encoding_type != SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE) {
        PRINT_ERROR("Validating entry type as NONE, but type of the entry is 0x%02x", entry->entry_type.encoding_type);
        return false;
    }
    if ((entry->none.must_be_zero[0] != 0u) || (entry->none.must_be_zero[1] != 0u)) {
        PRINT_ERROR("Validating entry type as NONE, but data is not all zero");
        return false;
    }
    return true;
}
// SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RAW
static bool x_otpdir_entry_appears_valid_raw(const X_DIRENTRY* entry) {
    if (entry->entry_type.encoding_type != SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RAW) {
        PRINT_ERROR("Validating entry type as RAW, but type of the entry is 0x%02x", entry->entry_type.encoding_type);
        return false;
    }
    if (entry->raw_data.row_count == 0u) {
        PRINT_ERROR("Validating entry type as RAW, but row count is zero");
        return false;
    }
    if (!x_otpdir_is_valid_user_content_row_range(entry->raw_data.start_row, entry->raw_data.row_count)) {
        return false;
    }
    return true;
}
// SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_BYTE3X
static bool x_otpdir_entry_appears_valid_byte3x(const X_DIRENTRY* entry) {
    if (entry->entry_type.encoding_type != SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_BYTE3X) {
        PRINT_ERROR("Validating entry type as BYTE3X, but type of the entry is 0x%02x", entry->entry_type.encoding_type);
        return false;
    }
    if (entry->byte3x_data.row_count == 0u) {
        PRINT_ERROR("Validating entry type as BYTE3X, but row count is zero");
        return false;
    }
    if (!x_otpdir_is_valid_user_content_row_range(entry->byte3x_data.start_row, entry->byte3x_data.row_count)) {
        return false;
    }
    return true;
}
// BP_OTPDIR_DATA_ENCODING_TYPE_RBIT3
static bool x_otpdir_entry_appears_valid_rbit3(const X_DIRENTRY* entry) {
    if (entry->entry_type.encoding_type != SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT3) {
        PRINT_ERROR("Validating entry type as RBIT3, but type of the entry is 0x%02x", entry->entry_type.encoding_type);
        return false;
    }
    if (entry->rbit3_data.row_count == 0u) {
        PRINT_ERROR("Validating entry type as RBIT3, but row count is zero");
        return false;
    }
    if (!x_otpdir_is_valid_user_content_row_range(entry->rbit3_data.start_row, entry->rbit3_data.row_count)) {
        return false;
    }
    if ((entry->rbit3_data.row_count % 3u) != 0u) {
        PRINT_ERROR("Validating entry type as RBIT3, but row count (%03x) is not a multiple of 3", entry->rbit3_data.row_count);
        return false;
    }
    return true;
}
// SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT8
static bool x_otpdir_entry_appears_valid_rbit8(const X_DIRENTRY* entry) {
    if (entry->entry_type.encoding_type != SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT8) {
        PRINT_ERROR("Validating entry type as RBIT8, but type of the entry is 0x%02x", entry->entry_type.encoding_type);
        return false;
    }
    if (entry->rbit8_data.row_count == 0u) {
        PRINT_ERROR("Validating entry type as RAW, but row count is zero");
        return false;
    }
    if (!x_otpdir_is_valid_user_content_row_range(entry->rbit8_data.start_row, entry->rbit8_data.row_count)) {
        return false;
    }
    if ((entry->rbit8_data.row_count % 8u) != 0u) {
        PRINT_ERROR("Validating entry type as RBIT8, but row count (%03x) is not a multiple of 8", entry->rbit8_data.row_count);
        return false;
    }
    return true;
}
// SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC and SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC_ASCII_STRING
static bool x_otpdir_entry_appears_valid_ecc(const X_DIRENTRY* entry) {
    if (entry->entry_type.encoding_type != SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC) {
        PRINT_ERROR("Validating entry type as ECC, but type of the entry is 0x%02x", entry->entry_type.encoding_type);
        return false;
    }
    if (entry->ecc_data.byte_count == 0u) {
        PRINT_ERROR("Validating entry type as ECC, but byte count is zero");
        return false;
    }
    uint16_t row_count = entry->ecc_data.byte_count / 2u + (entry->ecc_data.byte_count % 2u) ? 1 : 0;
    if (!x_otpdir_is_valid_user_content_row_range(entry->ecc_data.start_row, row_count)) {
        return false;
    }
    return true;
}
// SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_EMBEDED_IN_DIRENTRY
static bool x_otpdir_entry_appears_valid_embedded(const X_DIRENTRY* entry) {
    if (entry->entry_type.encoding_type != SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_EMBEDED_IN_DIRENTRY) {
        PRINT_ERROR("Validating entry type as EMBEDED_IN_DIRENTRY, but type of the entry is 0x%02x", entry->entry_type.encoding_type);
        return false;
    }
    // No further validation solely from encoding type is possible.
    return true;
}
#pragma endregion // OTP Directory Data Encoding Type Validation functions



// This function will read the entry at the given OTP row.
// If the entry is not readable in RAW form, then it will
// skip the current entry and read the next one.
static void x_otp_read_and_validate_direntry(uint16_t direntry_otp_row, X_ITERATOR_STATE* out_state) {
    bool failure = false;
    bool should_try_next_row_if_not_validated = false;
    if (direntry_otp_row < xFIRST_USER_CONTENT_ROW) {
        failure = true;
    } else if (direntry_otp_row >= (xFIRST_USER_CONTENT_ROW + xUSER_CONTENT_ROW_COUNT)) {
        failure = true;
    }

    X_DIRENTRY entry;
    memset(&entry, 0, sizeof(X_DIRENTRY));
 
    // read the entry
    if (!failure) {
        if (!saferotp_read_ecc_data(direntry_otp_row, &entry, sizeof(X_DIRENTRY))) {
            // TODO: Want to skip entries that are not readable.
            //       Maybe read as RAW to distinguish unreadable vs. not encoded with ECC data?
            //       For now, just skip the entry ... eventually will hit invalid data or out-of-range row.
            failure = true;
            should_try_next_row_if_not_validated = true;
        }
    }

    // validate the "must be zero" is actually zero
    if (!failure) {
        if (entry.entry_type.must_be_zero != 0u) {
            failure = true;
        }
    }

    // Validate the CRC16
    if (!failure) {
        uint16_t crc16 = crc16_calculate(&entry, sizeof(X_DIRENTRY) - 2);
        if (crc16 != entry.crc16) {
            failure = true;
        }
    }

    // Perform entry-type-specific validation
    if (!failure) {
        if (entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE) {
            if (!x_otpdir_entry_appears_valid_none(&entry)) {
                failure = true;
            }
            // else, it's a valid entry that encodes no data (including type SAFEROTP_OTPDIR_ENTRY_TYPE_END)
        } else if (entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_INVALID) {
            failure = true;
        } else if (entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RAW) {
            if (!x_otpdir_entry_appears_valid_raw(&entry)) {
                failure = true;
            }
        } else if (entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_BYTE3X) {
            if (!x_otpdir_entry_appears_valid_byte3x(&entry)) {
                failure = true;
            }
        } else if (entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT3) {
            if (!x_otpdir_entry_appears_valid_rbit3(&entry)) {
                failure = true;
            }
        } else if (entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT8) {
            if (!x_otpdir_entry_appears_valid_rbit8(&entry)) {
                failure = true;
            }
        } else if (entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC) {
            if (!x_otpdir_entry_appears_valid_ecc(&entry)) {
                failure = true;
            }
        } else if (entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC_ASCII_STRING) {
            if (!x_otpdir_entry_appears_valid_ecc(&entry)) {
                failure = true;
            }
        } else if (entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_EMBEDED_IN_DIRENTRY) {
            if (!x_otpdir_entry_appears_valid_embedded(&entry)) {
                failure = true;
            }
        } else {
            PRINT_WARNING("Unknown OTPDIR entry encoding type: 0x%02x, full data %04x %04x %04x %04x",
                entry.entry_type.encoding_type,
                entry.as_uint16[0], entry.as_uint16[1], entry.as_uint16[2], entry.as_uint16[3]
            );
            // unknown entry type
            failure = true;
        }
    }

    // OK, either failure, or the entry itself seems reasonable.  Update the state accordingly.
    // NOTE: SUCCESS will be returned when there is an entry of `BP_OTDIR_ENTRY_TYPE_END`.
    if (failure) {
        memset(out_state, 0, sizeof(X_ITERATOR_STATE));
        out_state->current_entry.entry_type.as_uint16_t = SAFEROTP_OTPDIR_ENTRY_TYPE_INVALID.as_uint16_t;
        out_state->should_try_next_row_if_not_validated = should_try_next_row_if_not_validated;
    } else {
        memcpy(&out_state->current_entry, &entry, sizeof(X_DIRENTRY));
        out_state->current_otp_row_start = direntry_otp_row;
        out_state->entry_validated = true;
    }
    return;
}

// This function will attempt to validate the entry at the given OTP starting row.
// This function will automatically advance to the next entry if the current entry
// is not valid, but 
static bool x_otp_direntry_find_next_entry(X_ITERATOR_STATE* state, uint16_t starting_row) {
    memset(state, 0, sizeof(X_ITERATOR_STATE));
    uint16_t row = starting_row;
    do {
        x_otp_read_and_validate_direntry(row, state);
        row -= xROWS_PER_DIRENTRY; // in case we loop to the next one...
    } while (!state->entry_validated && state->should_try_next_row_if_not_validated);

    if (!state->entry_validated) {
        return false;
    } else {
        return true;
    }
}
// Ref: FindFirstFile()
static bool x_otp_direntry_reset_directory_iterator(void) {
    X_ITERATOR_STATE* state = &x_current_directory_entry[ get_core_num() ];
    uint16_t starting_row = xSTART_ROW;
    return x_otp_direntry_find_next_entry(state, starting_row);
}
// Ref: FindNextFile()
static bool x_otp_direntry_move_to_next_entry(void) {
    X_ITERATOR_STATE* state = &x_current_directory_entry[ get_core_num() ];
    if (!state->entry_validated) {
        return false; // do nothing ...
    }
    if (state->current_entry.entry_type.as_uint16_t == SAFEROTP_OTPDIR_ENTRY_TYPE_END.as_uint16_t) {
        return false; // do nothing ...
    }
    uint16_t starting_row = state->current_otp_row_start - xROWS_PER_DIRENTRY;
    return x_otp_direntry_find_next_entry(state, starting_row);
}


// Operations that occur against the "current" directory entry
// 1. Get the current type
// 2. Get size of buffer required to read the corresponding data
// 3. Read the corresponding data into a caller-supplied buffer
static SAFEROTP_OTPDIR_ENTRY_TYPE x_otp_direntry_get_current_type(void) {
    X_ITERATOR_STATE* state = &x_current_directory_entry[ get_core_num() ];
    if (!state->entry_validated) {
        return SAFEROTP_OTPDIR_ENTRY_TYPE_END;
    }
    return state->current_entry.entry_type;
}

static size_t x_otp_direntry_get_current_buffer_size_required(void) {

    X_ITERATOR_STATE* state = &x_current_directory_entry[ get_core_num() ];
    size_t result = 0u;

    if (!state->entry_validated) {
        result = 0u;
    } else if (state->current_entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE) {
        result = 0u;
    } else if (state->current_entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RAW) {
        result = state->current_entry.raw_data.row_count * sizeof(uint32_t); // 32-bits per row, of which 24 bits contain the data
    } else if (state->current_entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_BYTE3X) {
        result = state->current_entry.byte3x_data.row_count;
    } else if (state->current_entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT3) {
        result = (state->current_entry.rbit3_data.row_count / 3u) * sizeof(uint32_t); // 32-bits per 3 rows, of which 24 bits contain the data
    } else if (state->current_entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT8) {
        result = (state->current_entry.rbit8_data.row_count / 8u) * sizeof(uint32_t); // 32-bits per 8 rows, of which 24 bits contain the data
    } else if (state->current_entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC) {
        result = state->current_entry.ecc_data.byte_count;
    } else if (state->current_entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC_ASCII_STRING) {
        result = state->current_entry.ecc_data.byte_count;
    } else if (state->current_entry.entry_type.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_EMBEDED_IN_DIRENTRY) {
        result = sizeof(uint32_t);
    } else {
        // ERROR! Unknown entry type!
        PRINT_FATAL("Unknown OTPDIR entry encoding type 0x%02x was marked as validated?  OTP Row %03x  full data %04x %04x %04x %04x",
            state->current_entry.entry_type.encoding_type,
            state->current_otp_row_start,
            state->current_entry.as_uint16[0], state->current_entry.as_uint16[1], state->current_entry.as_uint16[2], state->current_entry.as_uint16[3]
        );
        result = 0u;
    }
    return result;
}

static size_t x_otp_direntry_get_current_entry_data(void* buffer, size_t buffer_size) {
    X_ITERATOR_STATE* state = &x_current_directory_entry[ get_core_num() ];

    if (buffer_size == 0u) {
        PRINT_ERROR("Requested zero bytes of data for the current OTPDIR entry ... this is an error in the calling code");
        return 0u;
    }
    memset(buffer, 0, buffer_size);
    size_t required_size = x_otp_direntry_get_current_buffer_size_required();
    if (required_size == 0u) {
        PRINT_WARNING(
            "Current directory entry has zero bytes of data ... caller should not attempt to read data\n"
        );
        return 0u;
    }
    if (required_size > buffer_size) {
        PRINT_ERROR(
            "Requested buffer size 0x%04x (%d) is too small for the current OTPDIR entry; Need at least 0x%04x (%d) byte buffer",
            buffer_size, buffer_size,
            required_size, required_size
        );
        return 0u;
    }

    switch (state->current_entry.entry_type.encoding_type) {
        case SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE: {

            return 0u;
        }
        case SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RAW: {
            if (!saferotp_read_raw_data(state->current_entry.raw_data.start_row, buffer, required_size)) {
                return 0u;
            }
            return required_size;
        }
        case SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_BYTE3X: {
            uint16_t start_row = state->current_entry.byte3x_data.start_row;
            size_t number_of_reads_required = required_size;
            uint8_t* p = buffer; // for pointer arithmetic
            for (size_t i = 0; i < number_of_reads_required; ++i) {
                if (!saferotp_read_single_row_redundant_byte3x(start_row+i, p+i)) {
                    return 0u;
                }
            }
            return required_size;
        }
        case SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT3: {
            uint16_t start_row = state->current_entry.rbit3_data.start_row;
            size_t number_of_reads_required = required_size / sizeof(uint32_t);
            uint32_t* p = (uint32_t*)buffer; // for pointer arithmetic
            for (size_t i = 0; i < number_of_reads_required; ++i) {
                if (!saferotp_read_redundant_rows_RBIT3(start_row+(i*3), p+i)) {
                    return 0u;
                }
            }
            return required_size;
        }
        case SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT8: {
            uint16_t start_row = state->current_entry.rbit8_data.start_row;
            size_t number_of_reads_required = required_size / sizeof(uint32_t);
            uint32_t* p = (uint32_t*)buffer; // for pointer arithmetic
            for (size_t i = 0; i < number_of_reads_required; ++i) {
                if (!saferotp_read_redundant_rows_RBIT8(start_row+(i*8), p+i)) {
                    return 0u;
                }
            }
            return required_size;
        }
        case SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC: {
            uint16_t start_row = state->current_entry.ecc_data.start_row;
            if (!saferotp_read_ecc_data(start_row, buffer, required_size)) {
                return 0u;
            }
            return required_size;
        }
        case SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC_ASCII_STRING: {
            uint16_t start_row = state->current_entry.ecc_data.start_row;
            if (!saferotp_read_ecc_data(start_row, buffer, required_size)) {
                return 0u;
            }
            uint8_t* p = buffer; // for pointer arithmetic
            if (p[required_size-1] != 0u) {
                PRINT_WARNING("ECC ASCII STRING data is not NULL-terminated");
                return 0u;
            }
            for (size_t i = 0; i < required_size-1; ++i) {
                if ((p[i] < 0x20u) || (p[i] > 0x7Eu)) {
                    PRINT_WARNING("ECC ASCII STRING data contains non-printable character 0x%02x at offset %d", p[i], i);
                    return 0u;
                }
            }
            return required_size;
        }
        case SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_EMBEDED_IN_DIRENTRY: {
            memcpy(buffer, &state->current_entry.embedded_data.data, sizeof(uint32_t));
            return sizeof(uint32_t);
        }
        // do NOT place a default case here ... want the compiler warning for unhandled enum values
    }
    PRINT_ERROR("Unknown OTPDIR entry encoding type: 0x%02x", state->current_entry.entry_type.encoding_type);
    return 0u;
}


/// All code above this point are the static helper functions / implementation details.
/// Only the below are the public API functions.

bool saferotp_otpdir_find_first_entry(void) {
    return x_otp_direntry_reset_directory_iterator();
}
bool saferotp_otpdir_find_next_entry(void) {
    return x_otp_direntry_move_to_next_entry();
}
bool saferotp_otpdir_find_first_entry_of_type(SAFEROTP_OTPDIR_ENTRY_TYPE entryType) {
    if (!saferotp_otpdir_find_first_entry()) {
        return false;
    }
    while (saferotp_otpdir_get_current_entry_type().as_uint16_t != entryType.as_uint16_t) {
        if (!saferotp_otpdir_find_next_entry()) {
            return false;
        }
    }
    return true;
}
bool saferotp_otpdir_find_next_entry_of_type(SAFEROTP_OTPDIR_ENTRY_TYPE entryType) {
    if (!saferotp_otpdir_find_next_entry()) {
        return false;
    }
    while (saferotp_otpdir_get_current_entry_type().as_uint16_t != entryType.as_uint16_t) {
        if (!saferotp_otpdir_find_next_entry()) {
            return false;
        }
    }
    return true;
}

// Reads the data from OTP on behalf of the caller.  If the data is successfully read (and validated,
// for all types except RAW), the data will be in the caller-supplied buffer.
// Automatically handles the various data encoding schemes (RAW, byte3x, RBIT3, RBIT8, etc.).
// On success, returns the number of bytes actually read.
// On failure, returns zero.
// The buffer provided must be at least saferotp_otpdir_get_current_entry_buffer_size() bytes in size.
size_t saferotp_otpdir_get_current_entry_data(void* buffer, size_t buffer_size) {
    return x_otp_direntry_get_current_entry_data(buffer, buffer_size);
}

SAFEROTP_OTPDIR_ENTRY_TYPE saferotp_otpdir_get_current_entry_type(void) {
    return x_otp_direntry_get_current_type();
}
// Returns the buffer size (in bytes) required to get the data referenced by the current entry.
// Gives a consistent API for all the various data encoding schemes (RAW, byte3x, RBIT3, RBIT8, etc.).
// NOTE: By abstracting away the various encoding schemes, callers can simply allocate a buffer
//       of the returned size, and simply deal with byte-based buffers, simplifying use of the API.
size_t saferotp_otpdir_get_current_entry_buffer_size(void) {
    return x_otp_direntry_get_current_buffer_size_required();
}

bool saferotp_otpdir_add_entry_for_existing_ecc_data(
    SAFEROTP_OTPDIR_ENTRY_TYPE entryType,
    uint16_t start_row,
    size_t valid_data_byte_count
)
{
    bool failure = false;
    size_t required_rows;
    bool is_ascii_string = false;
    // Validate entry type's encoding is either ECC or ECC_ASCII_STRING
    if (!failure) {
        if (entryType.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC_ASCII_STRING) {
            is_ascii_string = true;
        } else if (entryType.encoding_type == SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC) {
            is_ascii_string = false;
        } else {
            PRINT_ERROR("Entry type 0x%04x has encoding type 0x%x (expected encoding 0x%x or 0x%x)",
                entryType.encoding_type, entryType.encoding_type,
                SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC,
                SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC_ASCII_STRING
            );
            failure = true;
        }
    }
    // Validate all rows would exist within the user data OTP rows
    // Validate the byte_count is reasonable
    if (!failure) {
        required_rows = (valid_data_byte_count/2u) + (valid_data_byte_count % 2u) ? 1 : 0;
        if (required_rows > xUSER_CONTENT_ROW_COUNT) {
            PRINT_ERROR("valid_data_byte_count 0x%zx exceeds size of OTP user data space (0x%zx)",
                entryType.encoding_type, required_rows, xUSER_CONTENT_ROW_COUNT
            );
            failure = true;
        } else if (start_row < xFIRST_USER_CONTENT_ROW) {
            PRINT_ERROR("Start row of %03x must be >= first user content row (%03x).",
                start_row, xFIRST_USER_CONTENT_ROW
            );
            failure = true;
        } else if (start_row >= xFIRST_USER_CONTENT_ROW + xUSER_CONTENT_ROW_COUNT) {
            PRINT_ERROR("Start row of %03x must be < first user content row (%03x + %03x).",
                start_row, xFIRST_USER_CONTENT_ROW, xUSER_CONTENT_ROW_COUNT
            );
            failure = true;
        } else if (start_row + required_rows > xFIRST_USER_CONTENT_ROW + xUSER_CONTENT_ROW_COUNT) {
            PRINT_ERROR("Start row of %03x ECC encoding %zx bytes (%x rows) exceeds end of user content space (%03x)",
                start_row, valid_data_byte_count, required_rows,
                xFIRST_USER_CONTENT_ROW + xUSER_CONTENT_ROW_COUNT
            );
            failure = true;
        }
    }
    
    // Validate all rows are readable, have valid ECC-encded data,
    // and (for ASCII strings) that the data is actually valid ASCII string
    if (!failure) {
        bool found_trailing_null = false;
        size_t remaining_bytes_to_check = valid_data_byte_count;
        for (uint_fast16_t current_row = start_row; current_row < start_row + required_rows; ++current_row, remaining_bytes_to_check -= 2u) {
            uint8_t data[2];
            if (!saferotp_read_ecc_data(current_row, data, sizeof(data))) {
                PRINT_ERROR("Failed to read ECC data from OTP row %03x", current_row);
                failure = true;
                break;
            }
            if (is_ascii_string) {
                if (data[0] == 0u) {
                    found_trailing_null = true;
                } else if (found_trailing_null) {
                    PRINT_ERROR("ECC ASCII string data contains embedded NULL followed by non-NULL at row %03x", data[0], current_row);
                    failure = true;
                    break;
                } else if ((data[1] < 0x20u) || (data[0] > 0x7Eu)) {
                    PRINT_ERROR("ECC ASCII string data contains non-printable character 0x%02x in row %03x", data[0], current_row);
                    failure = true;
                    break;
                }
                // repeat with the second byte
                if (remaining_bytes_to_check < 2) {
                    // no second byte to check ... ignore final filler byte
                } else if (data[0] == 0u) {
                    found_trailing_null = true;
                } else if (found_trailing_null) {
                    PRINT_ERROR("ECC ASCII string data contains embedded NULL followed by non-NULL at row %03x", data[0], current_row);
                    failure = true;
                    break;
                } else if ((data[1] < 0x20u) || (data[0] > 0x7Eu)) {
                    PRINT_ERROR("ECC ASCII string data contains non-printable character 0x%02x in row %03x", data[1], current_row);
                    failure = true;
                    break;
                }
            }
        }

        if (!failure && is_ascii_string && !found_trailing_null) {
            PRINT_ERROR("ECC ASCII string must include a trailing NULL character, but none found");
            failure = true;
        }
    }

    // If the entry appears to point to valid ECC-encoded data,
    // construct a new entry, find the end of the current directory,
    // and write the new entry to extend the directory with the new entry.
    if (!failure) {
        PRINT_ERROR("Not yet implemented"); // BUGBUG / TODO - NYI
        failure = true;
    }

    return failure;
}

// Adds a new directory entry of another type to the OTP directory.

