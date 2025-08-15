#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include "rp2350_otp_ecc.h"
#include "saferotp_ecc.h"

#define MY_DEBUG_OVERRIDE_DEFAULT_CATEGORY MY_DEBUG_CAT_02
#include "debug_rtt.h"

/* GCC is awesome. */
#define ARRAY_SIZE(arr) \
    (sizeof(arr) / sizeof((arr)[0]) \
     + sizeof(typeof(int[1 - 2 * \
           !!__builtin_types_compatible_p(typeof(arr), \
                 typeof(&arr[0]))])) * 0)



static const uint32_t SUCCESS_MASK = 0x0000FFFFu;

// Syndrome to bitflip table
// Syndrome is 5 bits, but not all values are used.
static const uint16_t sdk_otp_syndrome_to_bitflip[32] = {
    // [ 3] = 0x0001,
    // [ 5] = 0x0002,
    // [ 6] = 0x0004,
    // [ 7] = 0x0008,
    // [ 9] = 0x0010,
    // [10] = 0x0020,
    // [11] = 0x0040,
    // [12] = 0x0080,
    // [13] = 0x0100,
    // [14] = 0x0200,
    // [15] = 0x0400,
    // [17] = 0x0800,
    // [18] = 0x1000,
    // [19] = 0x2000,
    // [20] = 0x4000,
    // [21] = 0x8000,
    0x0000u, 0x0000u, 0x0000u, 0x0001u,  // [ 0.. 3]
    0x0000u, 0x0002u, 0x0004u, 0x0008u,  // [ 4.. 7]
    0x0000u, 0x0010u, 0x0020u, 0x0040u,  // [ 8..11]
    0x0080u, 0x0100u, 0x0200u, 0x0400u,  // [12..15]
    0x0000u, 0x0800u, 0x1000u, 0x2000u,  // [16..19]
    0x4000u, 0x8000u, 0x0000u, 0x0000u,  // [20..23] // 22..31 are unused for error correction
    0x0000u, 0x0000u, 0x0000u, 0x0000u,  // [24..27]
    0x0000u, 0x0000u, 0x0000u, 0x0000u,  // [28..31]
};
static const uint32_t _otp_ecc_parity_table[6] = {
    0b0000001010110101011011,
    0b0000000011011001101101,
    0b0000001100011110001110,
    0b0000000000011111110000,
    0b0000001111100000000000,
    0b0111111111111111111111,
};
static uint32_t _even_parity(uint32_t input) {
    uint32_t rc = 0;
    while (input) {
        rc ^= input & 1;
        input >>= 1;
    }
    return rc;
}
static uint32_t decode_raw_data_with_correction_impl(const OTP_RAW_READ_RESULT data) {
    // If this decodes correctly, only the lower 16-bits will be set.
    // Else, at least the top eight bits will be set, to indicate
    // an error condition.  (FFxxxxxx)

    // Initially based on trying to emulate what reading via
    // the ECC memory-mapped alias does, as described in datasheet:
    // See section 13.6.1 Bit Repair By Polarity @ page 1264
    // See section 13.6.2 Modified Hamming ECC @ page 1265

    // Input must be limited to 24-bit values
    if (data.as_uint32 & 0xFF000000u) {
        return RP2350_OTP_ECC_ERROR_INVALID_INPUT;
    }
    // 0. if only one BRBP bit is set, then check if exact match...
    if ((data.bit_repair_by_polarity == 0x1u) || (data.bit_repair_by_polarity == 0x2u)) {
        const uint16_t decoded_wo_brbp =  data.as_uint32;
        const uint16_t decoded_w__brbp = ~data.as_uint32;
        const OTP_RAW_READ_RESULT src_wo_brbp = { .as_uint32 = calculate_otp_ecc( decoded_wo_brbp )              };
        const OTP_RAW_READ_RESULT src_w__brbp = { .as_uint32 = calculate_otp_ecc( decoded_w__brbp ) ^ 0x00FFFFFF };

        uint32_t diff_wo_brbp = data.as_uint32 ^ src_wo_brbp.as_uint32;
        uint32_t diff_w__brbp = data.as_uint32 ^ src_w__brbp.as_uint32;
        bool match_wo_brbp = (diff_wo_brbp == 0x00800000u) || (diff_wo_brbp == 0x00400000u);
        bool match_w__brbp = (diff_w__brbp == 0x00800000u) || (diff_w__brbp == 0x00400000u);

        // Are both of those decodings an exact match (except for the BRBP bits?  If so, that's 
        if (match_wo_brbp && match_w__brbp) {
            return RP2350_OTP_ECC_ERROR_BRBP_DUAL_DECODINGS_POSSIBLE;
        } else if (match_wo_brbp) {
            return decoded_wo_brbp;
        } else if (match_w__brbp) {
            return decoded_w__brbp;
        } else {
            return RP2350_OTP_ECC_ERROR_BRBP_NEITHER_DECODING_VALID;
        }
    }

    // 1. if BRBP bits are both set, then invert all bits before further processing
    // See section 13.6.1 Bit Repair By Polarity @ page 1264:
    //     When you read an OTP value through an ECC alias,
    //     BRBP checks for two ones in bits 23:22.  When both
    //     bits 23 and 22 are set, BRBP inverts the entire row
    //     before passing it to the modified Hamming code stage.
    const OTP_RAW_READ_RESULT src = {
        .as_uint32 =
            (data.bit_repair_by_polarity == 0x3u) ?
            (data.as_uint32 ^ 0x00FFFFFFu) :
            (data.as_uint32)
    };
    
    // 2. re-calculate the six parity bits using lower 16-bits
    // See section 13.6.2 Modified Hamming ECC @ page 1265
    //     When you read an OTP value through an ECC alias,
    //     ECC recalculates the six parity bits based on the
    //     value read from the OTP row.
    //     ...
    OTP_RAW_READ_RESULT tmp = { .as_uint32 = calculate_otp_ecc(src.as_uint32) };

    // 3. Simplest case: do the values match exactly? If so, done!
    if (tmp.as_uint32 == src.as_uint32) {
        return src.as_uint32 & SUCCESS_MASK; // SUCCESS!
    }
    
    // 4. XOR the recalculated ECC vs. src bits
    // See section 13.6.2 Modified Hamming ECC @ page 1265
    //     ...
    //     Then, ECC XORs the original six (6) parity bits with
    //     the newly-calculated parity bits.
    //
    //     This generates six (6) new bits:
    //     • the five (5) LSBs are the syndrome, a unique
    //       bit pattern that corresponds to each possible
    //       bit flip in the data value
    //     • the MSB distinguishes between odd and even
    //       numbers of bit flips
    //     ...
    tmp.as_uint32 ^= src.as_uint32;

    // NEW: if syndrome has exactly one bit set,
    //      then bit flip was outside the 16 data bits.
    //      Done!
    if ((tmp.as_uint32 & (tmp.as_uint32 - 1u)) == 0u) {
        return src.as_uint32 & SUCCESS_MASK; // SUCCESS!
    }

    // NEW: Not specified in datasheet ...
    //      AFTER XOR ... the parity bit needs to flip
    //      if the low 5 bits have an odd number of set bits?
    if (_even_parity(tmp.hamming_ecc)) {
        tmp.parity_bit ^= 1;
    }
    // NEW: If syndrome is 0b00001 the single-bit error was in one of the BRBP bits?
    if (tmp.hamming_ecc == 0x01u) {
        return RP2350_OTP_ECC_ERROR_INTERNAL_ERROR_BRBP_BIT;
    }

    // 4. Decide result based on the parity & syndrome
    // See section 13.6.2 Modified Hamming ECC @ page 1265
    //     ...
    //     If all 6 bits in this value are zero,
    //         ECC did not detect an error.
    //     If the MSB is 0, but the syndrome contains a value other than 0,
    //         the ECC detected an unrecoverable multi-bit error.
    //     ...
    if (tmp.parity_bit == 0u && tmp.hamming_ecc == 0u) {
        // this is handled above ... checking if the encoded value already matched.
        // return src.as_uint32 & 0x0000FFFFu; // SUCCESS!
        return RP2350_OTP_ECC_ERROR_INTERNAL_ERROR_PERFECT_MATCH;
    }
    if (tmp.parity_bit == 0u && tmp.hamming_ecc != 0u) {
        return RP2350_OTP_ECC_ERROR_DETECTED_MULTI_BIT_ERROR;
    }


    // 5. Else correct the single-bit error ... the syndrome is the index into the bitflip array.
    // See section 13.6.2 Modified Hamming ECC @ page 1265
    //     ...
    //     If the MSB is 1,
    //         the syndrome should indicate a
    //         single-bit error.
    //         ECC flips the corresponding data bit
    //         to recover from the error.
    //     ...
    uint16_t bitflip = sdk_otp_syndrome_to_bitflip[tmp.hamming_ecc];
    return (src.as_uint32 ^ bitflip) & SUCCESS_MASK;
}

uint32_t calculate_otp_ecc(uint16_t x) {
    uint32_t p = x;
    for (uint_fast8_t i = 0; i < 6; ++i) {
        p |= _even_parity(p & _otp_ecc_parity_table[i]) << (16 + i);
    }
    return p;
}

uint32_t decode_raw_data(const OTP_RAW_READ_RESULT data) {
    // This function DISABLES raw data that the bootrom MIGHT accept as
    // being validly encoded ECC data.  This can occur when the count
    // of bitflips is 3, 5 (also 19, 21 for BRBP variants) vs. the correct encoding.
    uint32_t result = decode_raw_data_with_correction_impl(data);

    // if use of syndrome calculates matching low 16 bits,
    // then final check that original data has <2 bit flips vs.
    // the ECC encoded value.
    // This excludes erroneous acceptance of encodings with 3 or 5 bit flips.
    // (also excludes erroneous encodings with 19 or 21 bitflips ... see BRBP)
    // As a result, this function provides heightened reliability for the ECC decoding,
    // vs. a function that skips this step.

    if ((result & 0xFFFF0000u) == 0) {
        // result could be encoded two ways: with or without use of BRBP
        uint32_t chk  = calculate_otp_ecc(result);
        uint32_t brbp = chk ^ 0xFFFFFFu;
        uint32_t chk_bits  = chk  ^ data.as_uint32;
        uint32_t brbp_bits = brbp ^ data.as_uint32;
        if ((__builtin_popcount(chk_bits) <= 1) && (data.bit_repair_by_polarity != 0x3u)) {
            // this is OK
        } else if ((__builtin_popcount(brbp_bits) <= 1) && (data.bit_repair_by_polarity != 0x0u)) {
            // this is OK
        } else {
            // this is NOT a valid ECC decoding ... but maybe the bootrom will think it is.  :-)
            return 0xFF990000u; // make this into a symbolic named error (internal error)
        }
    }
    return result;
}

// This function returns zero on success.
// On error, it maps to:
// 

ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT exhaustive_test_of_ecc_recovery2(ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_BITMASK stages_to_run) {

    uint8_t stage = 0;
    // 1. For every possible 16-bit value, calculate the expected ECC ("expected_raw")
    // 2. For the single case of zero bits being flipped
    //    a. Verify it decodes to the same 16-bit value.
    //    b. Verify the bitmap does not yet mark the value as having been used.
    //    c. Mark that value as used in the bitmap.
    stage = 1;
    if ((stages_to_run & (1u << stage)) != 0) {
        for (uint_fast32_t i = 0; i < 0x10000; ++i) {
            const OTP_RAW_READ_RESULT expected_raw = {
                .as_uint32 = saferotp_calculate_ecc(i),
            };
            // a. Verify it decodes to the same 16-bit value.
            const uint32_t decoded_data = saferotp_decode_raw(expected_raw.as_uint32);
            if (decoded_data != i) {
                return (ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT){
                    .index = i,
                    .stage = stage,
                };
            }
        }
        PRINT_INFO("PROGRESS: 16-bit ECC encoded - OK\n");
    }
    

    // 1. For every possible 16-bit value, calculate the expected ECC ("pre_brbp")
    // 2. Invert the 24-bit pre_brbp value ("expected raw")
    // 3. For the single case of zero bits being flipped
    //    a. Verify it decodes to the same 16-bit value.
    //    b. Verify the bitmap does not yet mark that value as having been used.
    //    c. Mark that value as used in the bitmap.
    stage = 2;
    if ((stages_to_run & (1u << stage)) != 0) {
        for (uint_fast16_t i = 0; i < 0x10000; ++i) {
            const OTP_RAW_READ_RESULT pre_brbp = {
                .as_uint32 = saferotp_calculate_ecc(i),
            };
            const OTP_RAW_READ_RESULT expected_raw = {
                .as_uint32 = pre_brbp.as_uint32 ^ 0x00FFFFFFu,
            };
            // a. Verify it decodes to the same 16-bit value.
            const uint32_t decoded_data = saferotp_decode_raw(expected_raw.as_uint32);
            if (decoded_data != i) {
                return (ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT){
                    .index = i,
                    .stage = stage,
                };
            }
        }
        PRINT_INFO("PROGRESS: 16-bit ECC encoded [BRBP] - OK\n");
    }

    // 1. For every possible 16-bit value, calculate the expected ECC ("expected_raw")
    // 2. For every possible single-bit error of that expected_raw value:
    //    a. Verify it decodes to the same 16-bit value.
    //    b. Verify the bitmap does not yet mark that value as having been used.
    //    b. Mark that value as used in the bitmap.
    stage = 3;
    if ((stages_to_run & (1u << stage)) != 0) {
        for (uint_fast16_t i = 0; i < 0x10000; ++i) {
            const OTP_RAW_READ_RESULT pre_bit_error_raw = {
                .as_uint32 = saferotp_calculate_ecc(i),
            };
            for (uint_fast8_t j = 0; j < 24; ++j) {
                const OTP_RAW_READ_RESULT expected_raw = {
                    .as_uint32 = pre_bit_error_raw.as_uint32 ^ (1u << j),
                };
            
                // a. Verify it decodes to the same 16-bit value.
                const uint32_t decoded_data = saferotp_decode_raw(expected_raw.as_uint32);
                if (decoded_data != i) {
                    return (ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT){
                        .index = i,
                        .bit_flipped = j,
                        .stage = stage,
                    };
                }
            }
        }
        PRINT_INFO("PROGRESS: 16-bit ECC encoded w/1-bit - OK\n");
    }

    // 1. For every possible 16-bit value, calculate the expected ECC ("pre_brbp")
    // 2. Invert the 24-bit pre_brbp value ("expected raw")
    // 3. For every possible single-bit error of that expected_raw value:
    //    a. Verify it decodes to the same 16-bit value.
    //    b. Verify the bitmap does not yet mark that value as having been used.
    //    b. Mark that value as used in the bitmap.
    stage = 4;
    if ((stages_to_run & (1u << stage)) != 0) {
        for (uint_fast16_t i = 0; i < 0x10000; ++i) {
            const OTP_RAW_READ_RESULT pre_brbp = {
                .as_uint32 = saferotp_calculate_ecc(i),
            };
            const OTP_RAW_READ_RESULT pre_bit_error_raw = {
                .as_uint32 = pre_brbp.as_uint32 ^ 0x00FFFFFFu,
            };
            for (uint_fast8_t j = 0; j < 24; ++j) {
                const OTP_RAW_READ_RESULT expected_raw = {
                    .as_uint32 = pre_bit_error_raw.as_uint32 ^ (1u << j),
                };
            
                // a. Verify it decodes to the same 16-bit value.
                const uint32_t decoded_data = saferotp_decode_raw(expected_raw.as_uint32);
                if (decoded_data != i) {
                    return (ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT){
                        .index = i,
                        .bit_flipped = j,
                        .stage = stage,
                    };
                }
            }
        }
        PRINT_INFO("PROGRESS: 16-bit ECC encoded w/1-bit [BRBP] - OK\n");
    }

    // END OF LINE

    return (ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT){
        .as_uint32 = 0u,
    };
}

ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT exhaustive_test_of_ecc_recovery(ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_BITMASK stages_to_run) {
    ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT result2 = exhaustive_test_of_ecc_recovery2(stages_to_run);
    return result2;
}
