#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

// BUGBUG / TODO - Change prefix from `bp_otp_` or `BP_OTP_` to `saferotp_` or `SAFEROTP_`
//                 to help avoid conflicts with other libraries.

#ifdef __cplusplus
extern "C" {
#endif


#pragma region    // BP_OTP_DIRENTRY_TYPE
// TODO: REQUIRES --std:c23 or --std:gcc23 -- Fix these to be constexpr instead of macros  
// TODO: REQUIRES --std:c23 or --std:gcc23 -- Fix initialization to use above constexpr structs

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

    /* Encoding types 0x8..0xE are reserved for future use */

    // This is an invalid entry / cannot read data for this entry
    SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_INVALID                 = 0xFu,
} SAFEROTP_OTPDIR_DATA_ENCODING_TYPE;

typedef struct _SAFEROTP_OTPDIR_ENTRY_TYPE {
    union {
        uint16_t as_uint16_t;
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
#define SAFEROTP_OTPDIR_ENTRY_TYPE_END      ((SAFEROTP_OTPDIR_ENTRY_TYPE){ .id = 0x00u, .encoding_type = SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_NONE                          })
#define SAFEROTP_OTPDIR_ENTRY_TYPE_INVALID  ((SAFEROTP_OTPDIR_ENTRY_TYPE){ .id = 0xFFu, .encoding_type = SAFEROTP_OTPDIR_DATA_ENCODING_TYPE_INVALID, .must_be_zero = 0xFu })

// C11 and C23 don't seem to enable a way to statically assert SAFEROTP_OTPDIR_ENTRY_TYPE_INVALID.as_uint16 == 0xFFFFu.  Sigh...
#pragma endregion // BP_OTP_DIRENTRY_TYPE

#pragma region    // OTP Read / Write functions

// RP2350 OTP can encode data in multiple ways:
// * 24 bits of raw data (no error correction / detection)
// * 16 bits of ECC protected data (using the RP2350's standard ECC to correct 1-bit, detect 2-bit errors)
// *  8 bits of triple-redundant data (using a majority vote to correct each bit, aka 2-of-3 voting in a single OTP ROW)
// * Using multiple rows for N-of-M voting (e.g., boot critical fields might be recorded in eight rows...)
// There are many edge cases when reading or writing an OTP row,

// NOT RECOMMENDED DUE TO LIKELIHOOD OF UNDETECTED ERRORS:
// Writes a single OTP row with 24-bits of data.  No ECC / BRBP is used.
// Returns false if the data could not be written (e.g., if any bit is
// already set to 1, and the new value sets that bit to zero).
bool saferotp_write_single_row_raw(uint16_t row, uint32_t new_value);
// Reads a single OTP row raw, returning the 24-bits of data without interpretation.
// Using raw encoding is NOT recommended for general use.  However,
// reading of an OTP row as RAW can help differentiate between
// being unable to access the OTP row (e.g., locked) vs. ECC errors
// making the resulting values undecodable.
bool saferotp_read_single_row_raw(uint16_t row, uint32_t* out_data);

// Writes a single OTP row with 16-bits of data, protected by ECC.
// Writes will not fail due to a single bit already being set to one.
// Returns false if the data could not be written (e.g., if more than
// one bit was already written to 1).
bool saferotp_write_single_row_ecc(uint16_t row, uint16_t new_value);
// Reads a single OTP row, applies bit recovery by polarity,
// and corrects any single-bit errors using ECC.  Returns the
// corrected 16-bits of data.
// Returns false if the data could not be successfully read (e.g.,
// uncorrectable errors detected, etc.)
bool saferotp_read_single_row_ecc(uint16_t row, uint16_t* out_data);

// Read raw OTP row data, starting at the specified
// OTP row and continuing until the buffer is filled.
// Unlike reading of ECC data, the buffer here must be an integral multiple
// of four bytes.  This restriction is reasonable because the caller
// must already handle the 3-bytes-in-4 for the buffers.
bool saferotp_read_raw_data(uint16_t start_row, void* out_data, size_t count_of_bytes);
// Write the supplied buffer to OTP, starting at the specified
// OTP row and continuing until the buffer is fully written.
// Unlike writing ECC data, the buffer must be an integral multiple
// of four bytes.  This restriction is reasonable because the caller
// must already handle the 3-bytes-in-4 for the buffers.
bool saferotp_write_raw_data(uint16_t start_row, const void* data, size_t count_of_bytes);


// Write the supplied buffer to OTP, starting at the specified
// OTP row and continuing until the buffer is fully written.
// Allows writing an odd number of bytes, so caller does not have to
// do extra work to ensure buffer is always an even number of bytes.
// In this case, the extra byte written will be zero.
bool saferotp_write_ecc_data(uint16_t start_row, const void* data, size_t count_of_bytes);
// Fills the supplied buffer with ECC data, starting at the specified
// OTP row and continuing until the buffer is filled.
// Allows reading an odd number of bytes, so caller does not have to
// do extra work to ensure buffer is always an even number of bytes.
bool saferotp_read_ecc_data(uint16_t start_row, void* out_data, size_t count_of_bytes);

// Writes a single OTP row with 8-bits of data stored with 3x redundancy.
// For each bit of the new value that is zero:
//   the existing OTP row is permitted to have that bit set to one in
//   at most one of the three locations, without causing a failure.
// After writing the new value, the function reads the value back to
// ensure that (with voting applied) the new value was correctly stored.
// This style of storage is mostly used for flags that are independently
// updated over multiple OTP writes. 
bool saferotp_write_single_row_redundant_byte3x(uint16_t row, uint8_t new_value);
// Reads a single OTP row with 8-bits of data stored with 3x redundancy.
// Returns the 8-bits of data, after applying 2-of-3 voting.
bool saferotp_read_single_row_redundant_byte3x(uint16_t row, uint8_t* out_data);

// Writes three consecutive rows of OTP data with same 24-bit data.
// For each bit with a new value of zero:
//   the existing OTP rows are permitted to have that bit set to one
//   in one of the three rows, without this function failing.
// After writing the new values, the function reads the value back
// and will return false if the value (with voting applied) is not
// the expected new value.
bool saferotp_write_redundant_rows_RBIT3(uint16_t start_row, uint32_t new_value);
// Writes eight consecutive rows of OTP data with same 24-bit data.
// For each bit with a new value of zero:
//   the existing OTP rows are permitted to have that bit set to one
//   in one of the eight rows, without this function failing.
// After writing the new values, the function reads the value back
// and will return false if the value (with voting applied) is not
// the expected new value.
bool saferotp_write_redundant_rows_RBIT8(uint16_t start_row, uint32_t new_value);
// Reads three consecutive rows of raw OTP data (24-bits), and applies
// 2-of-3 voting for each bit independently.
// So long as at least two reads succeed, the data will be returned.
// Returns the 24-bits of voted-upon data.
bool saferotp_read_redundant_rows_RBIT3(uint16_t start_row, uint32_t* out_data);
// Reads eight consecutive rows of raw OTP data (24-bits), and applies
// 3-of-8 voting for each bit independently.
// So long as at least three reads succeed, the data will be returned.
// Returns the 24-bits of voted-upon data.
bool saferotp_read_redundant_rows_RBIT8(uint16_t start_row, uint32_t* out_data);
#pragma endregion // OTP Read / Write functions
#pragma region    // OTP Directory related functions (layer above the OTP read/write functions)
// Resets the OTP directory iterator to the first entry.
// For now, iterator state is kept per-CPU.  May change later to externally-allocated state.
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
#pragma endregion // OTP Directory related functions

#ifdef __cplusplus
}
#endif

