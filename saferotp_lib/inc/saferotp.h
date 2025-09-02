#pragma once

#ifndef SAFEROTP_H
#define SAFEROTP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO: CMakefile declaring option to enable/disable OTP virtualization
//       because this overly-simple method currently uses ~16k of RAM.
#define SAFEROTP_ENABLE_HARDWARE_HAL
#define SAFEROTP_ENABLE_VIRTUALIZATION

// Some of the APIs only make sense when virtualized OTP support exists....
#if defined(SAFEROTP_ENABLE_VIRTUALIZATION) // OTP Virtualization support
/// @brief 
/// Initializes the virtualization layer.
/// After initialization, all OTP rows will default to being unwritten / all-zero.
/// It is strongly encouraged to immediately afterwards call
/// `saferotp_virtualization_restore_all_pages_from_hardware()`,
/// to initialize OTP contents based on what's actually stored in the hardware OTP.
/// @return true if the virtualization layer was successfully initialized.
bool saferotp_virtualization_init();
/// @brief Provides a way to restore a set of virtualized OTP rows, regardless of current values.
///        This is intended to be used to allow state to be stored / restored externally, enabling
///        testing of virtualized OTP across reboots.  Restoration can be done at any time, and
///        in multiple calls, to align with various storage alignment restrictions.
/// @param starting_row The first row of the OTP pages to restore with the provided values.
/// @param buffer A buffer containing a `uint32_t` value for each consecutive OTP row to be restored.
/// @param buffer_size Count of bytes in the buffer. This must be a multiple of 4 bytes.
/// @return true if the virtualized OTP rows were successfully restored.
bool saferotp_virtualization_restore(uint16_t starting_row, const void* buffer, size_t buffer_size);
/// @brief Provides a way to retrieve a set of virtualized OTP rows, regardless of current values
///        and permissions.  This is intended to be used to allow state to be stored / restored externally,
///        enabling testing of virtualized OTP across reboots.  Retrieval can be done at any time, and
///        in multiple calls, to align with various storage alignment restrictions.
/// @param starting_row The first row of the OTP pages to retrieve with the provided values.
/// @param buffer A buffer in which to write a `uint32_t` value for each consecutive OTP row to be retrieved.
/// @param buffer_size Count of bytes in the buffer. This must be a multiple of 4 bytes.
/// @return true if the virtualized OTP rows were successfully retrieved.
bool saferotp_virtualization_save(uint16_t starting_row, void* buffer, size_t buffer_size);
#endif // defined(SAFEROTP_ENABLE_VIRTUALIZATION)

// A couple of the APIs only make sense when virtualized OTP support AND using real hardware OTP
#if defined(SAFEROTP_ENABLE_VIRTUALIZATION) && defined(SAFEROTP_ENABLE_HARDWARE_HAL)
/// @brief Resets the virtualized OTP value to reflect the values stored in the hardware OTP rows.
/// Note that, if any page(s) fail to read, the virtualized OTP will also be set to report an
/// error when read, and the function will return false.  However, processing of later pages will
/// continue.   For finer-grained control and feedback, use
/// `saferotp_virtualization_restore_page_from_hardware(uint16_t page)`;
/// @return true if the restoration of all pages was successful.; false if any page(s)
bool saferotp_virtualization_restore_all_pages_from_hardware();
/// @brief Resets a single virtualized OTP page to reflect the avlues stored in the hardware OTP rows.
/// If the page fails to read, the virtualized OTP will be set to report an error when read, and
/// the return value will be `false`.  
/// @param page An OTP page (64 rows), valid range for rp2350 is [0..63].
/// @return true if the page was successfully read (in raw mode).
bool saferotp_virtualization_restore_page_from_hardware(uint16_t page);
#endif // defined(SAFEROTP_ENABLE_VIRTUALIZATION) && defined(SAFEROTP_ENABLE_HARDWARE_HAL)

#pragma region    // Safer OTP Read / Write functions

// RP2350 OTP can encode data in multiple ways:
// * 24 bits of raw data (no error correction / detection)
// * 16 bits of ECC protected data (using the RP2350's standard ECC to correct 1-bit, detect 2-bit errors)
// *  8 bits of triple-redundant data (using a majority vote to correct each bit, aka 2-of-3 voting in a single OTP ROW)
// * Using multiple rows for N-of-M voting (e.g., boot critical fields might be recorded in eight rows...)
// There are many edge cases when reading or writing an OTP row,

// `RAW` - NOT RECOMMENDED DUE TO LIKELIHOOD OF UNDETECTED ERRORS:
// `RAW` - Writes a single OTP row with 24-bits of data.  No ECC / BRBP is used.
//         It is up to the caller to define and use some type of error correction / detection.
// Returns false unless all data is written and verified.
bool saferotp_write_single_value_raw_unsafe(uint16_t row, uint32_t new_value);
// `RAW` - NOT RECOMMENDED DUE TO LIKELIHOOD OF UNDETECTED ERRORS:
// `RAW` - Reads a single OTP row raw, returning the 24-bits of data without interpretation.
//         It is up to the caller to define and use some type of error correction / detection.
// Returns false unless all requested data is read.
bool saferotp_read_single_value_raw_unsafe(uint16_t row, uint32_t* out_data);
// `RAW` - NOT RECOMMENDED DUE TO LIKELIHOOD OF UNDETECTED ERRORS:
// `RAW` - Write the supplied buffer to OTP, starting at the specified
//         OTP row and continuing until the buffer is fully written.
//         It is up to the caller to define and use some type of error correction / detection.
// Note: count_of_bytes must be an integral multiple of four.
// Returns false unless all data is written and verified.
bool saferotp_write_data_raw_unsafe(uint16_t start_row, const void* data, size_t count_of_bytes);
// `RAW` - NOT RECOMMENDED DUE TO LIKELIHOOD OF UNDETECTED ERRORS:
// `RAW` - Read raw OTP row data, starting at the specified
//         OTP row and continuing until the buffer is filled.
//         It is up to the caller to define and use some type of error correction / detection.
// Unlike reading of ECC data, the buffer here must be an integral multiple
// of four bytes.  This restriction is reasonable because the caller
// must already handle the 3-bytes-in-4 for the buffers.
// Returns false unless all requested data is read.
bool saferotp_read_data_raw_unsafe(uint16_t start_row, void* out_data, size_t count_of_bytes);

// `ECC` - Writes a single OTP row with 16-bits of data, protected by ECC.
// Writes will NOT fail due to a single bit error.
// Returns false unless all data is written and verified.
bool saferotp_write_single_value_ecc(uint16_t row, uint16_t new_value);
// `ECC` - Reads a single OTP row, applies bit recovery by polarity,
// and corrects any single-bit errors using ECC.  Returns the
// corrected 16-bits of data.
// Returns false unless all requested data is read.
bool saferotp_read_single_value_ecc(uint16_t row, uint16_t* out_data);
// `ECC` - Write the supplied buffer to OTP, starting at the specified
// OTP row and continuing until the buffer is fully written.
// Allows writing an odd number of bytes, so caller does not have to
// do extra work to ensure buffer is always an even number of bytes.
// In this case, the extra byte written will be zero.
// Returns false unless all data is written and verified.
bool saferotp_write_data_ecc(uint16_t start_row, const void* data, size_t count_of_bytes);
// `ECC` - Fills the supplied buffer with ECC data, starting at the specified
// OTP row and continuing until the buffer is filled.
// Allows reading an odd number of bytes, so caller does not have to
// do extra work to ensure buffer is always an even number of bytes.
// Returns false unless all requested data is read.
bool saferotp_read_data_ecc(uint16_t start_row, void* out_data, size_t count_of_bytes);

// `BYTE3X` - Writes a single OTP row with 8-bits of data stored with 3x redundancy.
// For each bit of the new value that is zero:
//   the existing OTP row is permitted to have that bit set to one in
//   at most one of the three locations, without causing a failure.
// After writing the new value, the function reads the value back to
// ensure that (with voting applied) the new value was correctly stored.
// This style of storage is mostly used for flags that are independently
// updated over multiple OTP writes. 
// Returns false unless all data is written and verified (with voting applied).
bool saferotp_write_single_value_byte3x(uint16_t row, uint8_t new_value);
// `BYTE3X` - Reads a single OTP row with 8-bits of data stored with 3x redundancy.
// Returns the 8-bits of data, after applying 2-of-3 voting.
// Returns false unless all requested data is read.
bool saferotp_read_single_value_byte3x(uint16_t row, uint8_t* out_data);

// TODO: add `saferotp_write_data_byte3x(uint16_t start_row, const void* data, size_t count_of_bytes);`
// TODO: add `saferotp_read_data_byte3x(uint16_t start_row, void* out_data, size_t count_of_bytes);`

// `RBIT3` - Writes three consecutive rows of OTP data with same 24-bit data.
// For each bit with a new value of zero:
//   the existing OTP rows are permitted to have that bit set to one
//   in one of the three rows, without this function failing.
// After writing the new values, the function reads the value back
// and will return false if the value (with voting applied) is not
// the expected new value.
// Returns false unless all data is written and verified (with voting applied).
bool saferotp_write_single_value_rbit3(uint16_t start_row, uint32_t new_value);
// `RBIT3` - Reads three consecutive rows of raw OTP data (24-bits), and applies
// 2-of-3 voting for each bit independently.
// So long as at least two reads succeed, the data will be returned.
// Returns the 24-bits of voted-upon data.
// Returns false unless all requested data is read.
bool saferotp_read_single_value_rbit3(uint16_t start_row, uint32_t* out_data);

// TODO: add `saferotp_write_data_rbit3(uint16_t start_row, const void* data, size_t count_of_bytes);`
// TODO: add `saferotp_read_data_rbit3(uint16_t start_row, void* out_data, size_t count_of_bytes);`

// `RBIT8` - Writes eight consecutive rows of OTP data with same 24-bit data.
// For each bit with a new value of zero:
//   the existing OTP rows are permitted to have that bit set to one
//   in one of the eight rows, without this function failing.
// After writing the new values, the function reads the value back
// and will return false if the value (with voting applied) is not
// the expected new value.
// Returns false unless all data is written and verified (with voting applied).
bool saferotp_write_single_value_rbit8(uint16_t start_row, uint32_t new_value);
// `RBIT8` - Reads eight consecutive rows of raw OTP data (24-bits), and applies
// 3-of-8 voting for each bit independently.
// So long as at least three reads succeed, the data will be returned.
// Returns the 24-bits of voted-upon data.
// Returns false unless all requested data is read.
bool saferotp_read_single_value_rbit8(uint16_t start_row, uint32_t* out_data);

// TODO: add `saferotp_write_data_rbit8(uint16_t start_row, const void* data, size_t count_of_bytes);`
// TODO: add `saferotp_read_data_rbit8(uint16_t start_row, void* out_data, size_t count_of_bytes);`

#pragma endregion // Safer OTP Read / Write functions

#ifdef __cplusplus
}
#endif

#endif // SAFEROTP_H