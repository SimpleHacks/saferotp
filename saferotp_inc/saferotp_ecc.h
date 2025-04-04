#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// Note: looking at these results is only for debugging purposes.
//       Code can simply check for error by checking if any of the top 8 bits are non-zero.
//       (if any bit of the top (most significant) 8 bits are set, it indicates an error)
typedef enum _SAFEROTP_ECC_ERROR {
    // only low 24 bits can contain data
    // Thus, all values from 0x00000000u .. 0x00FFFFFFu are successful results
    SAFEROTP_ECC_ERROR_INVALID_INPUT                   = 0x7F010000u,
    SAFEROTP_ECC_ERROR_DETECTED_MULTI_BIT_ERROR        = 0x7F020000u,
    SAFEROTP_ECC_ERROR_BRBP_NEITHER_DECODING_VALID     = 0x7F030000u, // BRBP = 0b10 or 0b01, but neither decodes precisely
    SAFEROTP_ECC_ERROR_NOT_VALID_SINGLE_BIT_FLIP       = 0x7F050000u, // 
    SAFEROTP_ECC_ERROR_INVALID_ENCODING                = 0x7F040000u, // Syndrome alone generates data, but too many bit flips...
    SAFEROTP_ECC_ERROR_BRBP_DUAL_DECODINGS_POSSIBLE    = 0x40000000u, // TODO: prove code makes this impossible to hit
    SAFEROTP_ECC_ERROR_INTERNAL_ERROR_BRBP_BIT         = 0x40010000u, // TODO: prove code makes this impossible to hit
    SAFEROTP_ECC_ERROR_INTERNAL_ERROR_PERFECT_MATCH    = 0x40020000u, // TODO: prove code makes this impossible to hit
    SAFEROTP_ECC_ERROR_POTENTIALLY_READABLE_BY_BOOTROM = 0x7F990000u, // TODO: prove or disprove bootrom behavior for these cases (all have 3 or 5 flipped bits)
} SAFEROTP_ECC_ERROR;

// When reading ECC OTP data using RAW reads, the result is an opaque 32-bit value.
// Sometimes it's convenient to view as a uint32_t,
// other times it's convenient to view the individual fields.
// (ab)use anonymous unions to allow whichever use is convenient.
typedef struct _SAFER_OTP_RAW_READ_RESULT {
    // anonymous structs are supported in C11
    union {
        uint32_t as_uint32;
        uint8_t  as_bytes[4];
        struct {
            uint8_t lsb;
            uint8_t msb;
            union {
                struct {
                    uint8_t hamming_ecc : 5; // aka syndrome during correction
                    uint8_t parity_bit : 1;
                    uint8_t bit_repair_by_polarity : 2;
                };
                uint8_t correction;
            };
            uint8_t is_error; // 0: ok
        };
    };
} SAFEROTP_RAW_READ_RESULT;
static_assert(sizeof(SAFEROTP_RAW_READ_RESULT) == sizeof(uint32_t));

// Given the 16-bit value to be stored using ECC encoding,
// Get the 32-bit value (castable to SAFEROTP_RAW_READ_RESULT),
// containing both the hamming_ecc and parity_bit fields.
// This function will always leave the BRBP bits as zero.
uint32_t saferotp_calculate_ecc(uint16_t x); // [[unsequenced]]

// Given the raw value read from an OTP row containing ECC encoded data,
// this function corrects all single-bit errors, detects all double-bit errors,
// and will even detect many (most?) other bit corruptions.
// Returns a value with AT LEAST one of the 8 most significant bits set,
// as only 16 bit values can be stored using the ECC encoding.
uint32_t saferotp_decode_raw(uint32_t data); // [[unsequenced]]

#ifdef __cplusplus
}
#endif
