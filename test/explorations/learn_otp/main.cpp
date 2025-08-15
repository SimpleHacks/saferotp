#include "pico/stdlib.h"
#include "learn_otp.h"
#include "debug_rtt.h"

#if defined(DEBUG_INPUT_TINYUSB_CDC_ITF) || defined(DEBUG_OUTPUT_TINYUSB_CDC_ITF)
    #include "tusb.h"
#endif

#include "zoned_otp.h"
#include "saferotp.h"
#include "saferotp_ecc.h"
#include "debug_rtt.h"
#include "SEGGER_RTT.h"

volatile bool g_WaitForKey_Temp = true;
#define WAIT_FOR_KEY()               \
    do {                             \
        if (g_WaitForKey_Temp) {     \
            MY_DEBUG_WAIT_FOR_KEY(); \
        }                            \
    } while (0)


int do_not_trust_bootrom_ecc_read_nor_memory_mapped_ecc(void) {
    using namespace zoned_otp;

    MY_PRINTF("Hello from temporary funtcion...\n");
    //WAIT_FOR_KEY();

    // Read OTP rows 0x200 .. 0x208
    enum {
        TEST_ROW_COUNT = 14u,
    };
    _Static_assert(TEST_ROW_COUNT % 2 == 0, "TEST_ROW_COUNT must be even"); // b/c MM reads are 2x rows at a time
    SAFEROTP_RAW_READ_RESULT rows[TEST_ROW_COUNT] = {
        {.as_uint32 = 0x25b98b }, // Row 200 == properly encoded
        {.as_uint32 = 0x000803 }, // Row 201 == three-bit error?
        {.as_uint32 = 0x1db98b }, // Row 202 
        {.as_uint32 = 0x33b98b }, // Row 203 
        {.as_uint32 = 0x39b98b }, // Row 204 
        {.as_uint32 = 0x3ab98b }, // Row 205 
        {.as_uint32 = 0x3cb98b }, // Row 206 
        {.as_uint32 = 0x3fb98b }, // Row 207
        {.as_uint32 = 0xda4674 }, // Row 208 == BRBP properly encoded
        {.as_uint32 = 0xcc4674 }, // Row 209 == BRBP phantom encoding 33..
        {.as_uint32 = 0xdd4674 }, // Row 20A == BRBP phantom 33, 1 BRBP flip, 1 ECC flip
        {.as_uint32 = 0xcd4675 }, // Row 20B == BRBP phantom 33, 1 ECC flip, 1 value flip
        {.as_uint32 = 0xda4671 }, // Row 20C == BRBP, 2 value bits flipped (0x000005)
        {.as_uint32 = 0xda4644 }, // Row 20D == BRBP, 2 value bits flipped (0x000030)
    };

    // First, update the rows with the above data, if not already set
    for (uint16_t i = 0; i < TEST_ROW_COUNT; ++i) {
        SAFEROTP_RAW_READ_RESULT tmp = {0};
        SAFEROTP_RAW_READ_RESULT desired = rows[i];
        bool r;
        r = saferotp_read_single_row_raw(0x200 + i, &tmp.as_uint32);
        if (!r) {
            MY_PRINTF("Failed to read row %04X\n", 0x200 + i);
            return -1;
        }
        if (tmp.as_uint32 == desired.as_uint32) {
            MY_PRINTF("Row %04X already set to %06X\n", 0x200 + i, tmp.as_uint32);
            continue;
        }
        if (((~tmp.as_uint32) & desired.as_uint32) != desired.as_uint32) {
            MY_PRINTF("Row %04X is %06x ... cannot become %06x\n", 0x200 + i, tmp.as_uint32, desired.as_uint32);
            return -1;
        }
        r = saferotp_write_single_row_raw(0x200 + i, desired.as_uint32);
        if (!r) {
            MY_PRINTF("Failed to write row %04X\n", 0x200 + i);
            return -1;
        }
        MY_PRINTF("Row %04X now set to %06X\n", 0x200 + i, desired.as_uint32);
    }

    // For tracking all the results
    SAFEROTP_RAW_READ_RESULT raw[TEST_ROW_COUNT] = {0};
    uint32_t safer_decoded[TEST_ROW_COUNT] = {0};
    bool r_raw[TEST_ROW_COUNT] = {0};
    uint16_t safer_ecc[TEST_ROW_COUNT] = {0};
    bool r_safer_ecc[TEST_ROW_COUNT] = {0};
    uint16_t bootrom_ecc[TEST_ROW_COUNT] = {0};
    int r_bootrom_ecc[TEST_ROW_COUNT] = {0};
    union {
        uint16_t as_uint16[TEST_ROW_COUNT];
        uint32_t as_uint32[TEST_ROW_COUNT/2u];
    } from_mm_ecc = {0};

    for (uint16_t i = 0; i < TEST_ROW_COUNT; ++i) {
        safer_decoded[i] = saferotp_decode_raw(rows[i].as_uint32);

        uint16_t row = 0x200 + i;
        r_raw[i] = saferotp_read_single_row_raw(row, &raw[i].as_uint32);
        r_safer_ecc[i] = saferotp_read_single_row_ecc(row, &safer_ecc[i]);
        do {
            otp_cmd_t cmd;
            cmd.flags = row | OTP_CMD_ECC_BITS;
            r_bootrom_ecc[i] = rom_func_otp_access((uint8_t*)(&bootrom_ecc[i]), 2, cmd);
        } while (0);
    }
    for (uint_fast8_t i = 0; i < (TEST_ROW_COUNT/2u); ++i) {
        uintptr_t ptr = OTP_DATA_BASE + (0x200u * 2u) + (i * 4u);
        from_mm_ecc.as_uint32[i] = *(volatile uint32_t*)ptr;
        MY_PRINTF("MM ECC %06X: %04X %04X\n",
            0x200 + (i * 2u),
            from_mm_ecc.as_uint16[(2*i) + 0],
            from_mm_ecc.as_uint16[(2*i) + 1]
        );
    }

    
    


    otp_zone_raw_buffer_t data = {0};
    if (!saferotp_read_raw_data(0x200, &data.raw_data[0], sizeof(otp_zone_raw_buffer_t))) {
        MY_PRINTF("Failed to read raw data\n");
        return -1;
    }
    MY_PRINTF("Read data: \n");
    hexdump(data);

    // if (!saferotp_write_raw_data(0x100, &to_write.raw_data[0], sizeof(otp_zone_raw_buffer_t))) {
    //     MY_PRINTF("Failed to write raw data\n");
    //     return -1;
    // }
    // MY_PRINTF("Success?\n");

    return 0;
}

int main(void) {
    stdio_init_all();

    SEGGER_RTT_Init();
    return do_not_trust_bootrom_ecc_read_nor_memory_mapped_ecc();


    // MY_DEBUG_INITIALIZE(true);
    // return temporary();
    // return main_learn_otp();
}