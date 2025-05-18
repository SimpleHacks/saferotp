#pragma once

#ifndef SAFEROTP_DIRENTRY_H
#define SAFEROTP_DIRENTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma region    // enums and structures
// TODO: If supporting --std:c23 or --std:gcc23 --> Fix these to be constexpr instead of macros
// TODO: If supporting --std:c23 or --std:gcc23 --> Fix initialization to use above constexpr structs

// Must be defined at same level as OTP directory item API,
// to allow defining the OTPDIR_ENTRY_TYPE constants.
typedef enum _SAFEROTP_OTPDIR_DATA_ENCODING_TYPE {
    // No data is associated with this row.
    //  Single row, 24-bits of raw data, no error correction or detection
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE                    = 0x0u,
    // Single row, 24-bits of raw data, no error correction or detection
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RAW                     = 0x1u,
    // Single row, 8-bits of data, stored triple-redundant.
    // Majority voting (2-of-3) is applied for each bit independently.
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_BYTE3X                  = 0x2u,
    // Identical data stored in three consecutive OTP rows.
    // Majority voting (2-of-3) is applied for each bit independently.
    // APIs should be provided the first (lowest) OTP row number.
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT3                   = 0x3u,
    // Identical data stored in eight consecutive OTP rows.
    // Special voting is applied for each bit independently:
    // If 3 or more rows have the bit set, then that bit is considered set.
    // This is used ONLY for the critical boot rows.
    // APIs should be provided the first (lowest) OTP row number.
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_RBIT8                   = 0x4u,
    // Single row, 16-bits of data, ECC detects 2-bit errors, corrects 1-bit errors
    // and BRBP allows writing even where a row has a single bit already set to 1
    // (even if stored value would normally store zero for that bit).
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC                     = 0x5u,
    // Identical to SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC, with the added
    // requirement that the record includes at least one trailing zero byte.
    // This is useful for ASCII / UTF-8 strings.
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_ECC_ASCII_STRING        = 0x6u,
    // Value is encoded directly within the directory entry as an (up to) 32-bit value.
    // The least significant 16 bits are stored within the `start_row` field.
    // The most significant 16 bits are stored within the `byte_count` field.
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_EMBEDED_IN_DIRENTRY     = 0x7u,

    /* Encoding data types 0x8..0xE are reserved for future use */

    // This is an invalid entry / cannot read data for this entry
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_INVALID                 = 0xFu,
} SAFEROTP_OTPDIR_DATA_ENCODING_TYPE;

typedef struct _SAFEROTP_OTPDIR_ENTRY_TYPE {
    union {
        uint16_t as_uint16;
        struct {
            uint16_t id            : 8; // can be extended to 12-bits if ever exceed 255 types for the given encoding type
            uint16_t must_be_zero  : 4; // reserved for future use
            uint16_t encoding_type : 4; // e.g., ECC, ECC_String, BYTE3X, RBIT3, RBIT8, RAW
        };
    };
} SAFEROTP_OTPDIR_ENTRY_TYPE;
static_assert(sizeof(SAFEROTP_OTPDIR_ENTRY_TYPE) == sizeof(uint16_t));

// Two common entry types are required here ...
// * one for blank entries (allowing appending of more entries)
// * one for all-0xFF entries (finalized, non-appendable end of directory)
// Note that, although the encoding type is shown as separate from the ID,
// the full 32-bit value is used as a single identifier for the entry.
#define SAFEROTP_OTPDIR_ENTRY_TYPE_END      ((SAFEROTP_OTPDIR_ENTRY_TYPE){ .as_uint16 = 0x0000u })
#define SAFEROTP_OTPDIR_ENTRY_TYPE_INVALID  ((SAFEROTP_OTPDIR_ENTRY_TYPE){ .as_uint16 = 0xFFFFu })

// Implementation details of how the directory entries are stored in OTP are intentionally not exposed.
// This will allow changing the implementation details without breaking the API.

// C11 and C23 don't seem to enable a way to statically assert SAFEROTP_OTPDIR_ENTRY_TYPE_INVALID.as_uint16 == 0xFFFFu.  Sigh...
#pragma endregion // enums and structures

#pragma region    // Reading OTP Directory related functions (layer above the OTP read/write functions)
// Resets the OTP directory iterator to the first entry.
// For now, iterator state is kept per-CPU.
// Returns FALSE when no more entries will be enumerated. (e.g., no entries found)
bool saferotp_otpdir_find_first_entry(void);
// Moves the current iterator to the next entry.
// Returns FALSE when no more entries will be enumerated. (e.g., no additional entries found)
bool saferotp_otpdir_find_next_entry(void);
// Resets the OTP directory iterator, and iterates until finding the specified entry type.
// Returns FALSE when no more entries will be enumerated. (e.g., no entries found)
bool saferotp_otpdir_find_first_entry_of_type(SAFEROTP_OTPDIR_ENTRY_TYPE entryType);
// Moves the current iterator to the next entry of the specified type.
// Returns FALSE when no more entries will be enumerated. (e.g., no additional entries found)
bool saferotp_otpdir_find_next_entry_of_type(SAFEROTP_OTPDIR_ENTRY_TYPE entryType);
// Returns the TYPE of the current entry.
// If the iterator is at the end, then the type is SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE.
SAFEROTP_OTPDIR_ENTRY_TYPE saferotp_otpdir_get_current_entry_type(void);
// Returns the buffer size (in bytes) required to get the data referenced by the current entry.
// Gives a consistent API for all the various data encoding schemes (RAW, byte3x, RBIT3, RBIT8, etc.)
// NOTE: This provides a count of bytes required to retrieve the data, abstracting away the various encoding schemes
//       with their various conversions to/from row counts.   Simplifies things for the caller to only deal with bytes.
size_t saferotp_otpdir_get_current_entry_buffer_size(void);
// Reads the data from OTP on behalf of the caller.  If the data is successfully read (and validated,
// for all types except RAW), the data will be in the caller-supplied buffer.
// Automatically handles the various data encoding schemes (RAW, byte3x, RBIT3, RBIT8, etc.)
size_t saferotp_otpdir_get_current_entry_data(void* buffer, size_t buffer_size);
#pragma endregion // Reading OTP Directory related functions (layer above the OTP read/write functions)

// TODO: Add API layer that allows external (opaque) iterator state to be allocated and used.

#pragma region    // Writing OTP Directory entries -- particularly unstable API
// Verification includes:
// * entry type encoding is either ECC or ECC_ASCII_STRING
// * valid_byte_count is reasonable
// * all rows would exist within the user data OTP rows
// * all rows are readable
// * all rows encode valid ECC-encoded data
// * For ECC_ASCII_STRING
//   * First N bytes are all in range 0x20..0x7E

//   * All remaining bytes are 0x00
//   * The final byte must be 0x00
bool saferotp_otpdir_add_entry_for_existing_ecc_data(SAFEROTP_OTPDIR_ENTRY_TYPE entryType, uint16_t start_row, size_t valid_data_byte_count);
#pragma endregion // Writing OTP Directory entries -- particularly unstable API


#ifdef __cplusplus
}
#endif


#endif // SAFEROTP_DIRENTRY_H

