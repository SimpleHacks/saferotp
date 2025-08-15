#pragma once

#include <stdint.h>
#include <assert.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef enum _RP2350_OTP_ECC_ERROR {
    // only low 24 bits can contain data
    // Thus, all values from 0x00000000u .. 0x00FFFFFFu are successful results
    RP2350_OTP_ECC_ERROR_INVALID_INPUT                 = 0xFF010000u,
    RP2350_OTP_ECC_ERROR_DETECTED_MULTI_BIT_ERROR      = 0xFF020000u,
    RP2350_OTP_ECC_ERROR_BRBP_NEITHER_DECODING_VALID   = 0xFF030000u, // BRBP = 0b10 or 0b01, but neither decodes precisely
    RP2350_OTP_ECC_ERROR_INVALID_ENCODING              = 0xFF040000u, // Syndrome alone generates data, but too many bit flips...

    RP2350_OTP_ECC_ERROR_BRBP_DUAL_DECODINGS_POSSIBLE  = 0x80000000u, // TODO: prove this is impossible
    RP2350_OTP_ECC_ERROR_INTERNAL_ERROR_BRBP_BIT       = 0x80010000u, // TODO: prove code makes this impossible to hit
    RP2350_OTP_ECC_ERROR_INTERNAL_ERROR_PERFECT_MATCH  = 0x80020000u, // TODO: prove code makes this impossible to hit
} RP2350_OTP_ECC_ERROR;

typedef struct _OTP_RAW_READ_RESULT {
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
} OTP_RAW_READ_RESULT;
static_assert(sizeof(OTP_RAW_READ_RESULT) == sizeof(uint32_t));
uint32_t calculate_otp_ecc(uint16_t x);


typedef enum _ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_BITMASK {
    ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_NONE =  0u,
    ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_0    =  1u,
    ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_1    =  2u,
    ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_2    =  4u,
    ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_3    =  8u,
    ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_4    = 16u,
    ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_ALL  = 30u,
} ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_BITMASK;

typedef struct _ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT {
    union {
        uint32_t as_uint32;
        struct {
            uint32_t index       : 16;
            uint32_t bit_flipped :  8;
            uint32_t stage       :  8;
        };
    };
} ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT;
static_assert(sizeof(ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT) == sizeof(uint32_t));

ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT exhaustive_test_of_ecc_recovery(ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_BITMASK stages_to_run);


#ifdef __cplusplus
}
#endif
