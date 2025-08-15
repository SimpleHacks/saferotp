/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pico/stdlib.h>

#include "zoned_otp.h"
#include "bp_whitelabel.h"
#include "saferotp_ecc.h"

#define MY_DEBUG_OVERRIDE_DEFAULT_CATEGORY MY_DEBUG_CAT_01
#include "debug_rtt.h"


using namespace zoned_otp::_saferotp_read;
//using namespace zoned_otp::_bootrom_read;
using namespace zoned_otp::_saferotp_write;
//using namespace zoned_otp::_actually_writes;




// OVERALL USAGE:
// * 0x000 .. 0x0bf - boot configuration (predefined)
// * 0x0c0 .. 0x0ff - Whitelabel configuration
// * 0x100 .. 0x1ff - OTP "zone"  1
// * 0x200 .. 0x2ff - OTP "zone"  2
// *  ...  ..  ...  -      ...
// * 0xe00 .. 0xeff - OTP "zone" 14
// * 0xf00 .. 0xf3f - directory entries (up to 16 entries, walking backwards)
// * 0xf40 .. 0xfff - otp lock configuration (predefined)

namespace zoned_otp {


    // Should these remaining portions be in learn_otp.cpp?
    // what configuration are we looking at?
    // Each zone: 3-way or ECC for base data type, + bitflip type
    static const zone_config_t zone_configs[] = {
        { otp_zone_data_type_t::raw,      otp_bit_flip_t::none,                   "raw3x_none",     },
        { otp_zone_data_type_t::raw,      otp_bit_flip_t::single_bit_flip_0_to_1, "raw3x_0_to_1",   },
        { otp_zone_data_type_t::raw,      otp_bit_flip_t::single_bit_flip_1_to_0, "raw3x_1_to_0",   },
        { otp_zone_data_type_t::raw,      otp_bit_flip_t::multiple_bits_flipped,  "raw3x_multibit", },
        { otp_zone_data_type_t::ecc,      otp_bit_flip_t::none,                   "ecc_none",       },
        { otp_zone_data_type_t::ecc,      otp_bit_flip_t::single_bit_flip_0_to_1, "ecc_0_to_1",     },
        { otp_zone_data_type_t::ecc,      otp_bit_flip_t::single_bit_flip_1_to_0, "ecc_1_to_0",     },
        { otp_zone_data_type_t::ecc,      otp_bit_flip_t::multiple_bits_flipped,  "ecc_multibit",   },
        { otp_zone_data_type_t::brbp_ecc, otp_bit_flip_t::none,                   "brbp_none",      },
        { otp_zone_data_type_t::brbp_ecc, otp_bit_flip_t::single_bit_flip_0_to_1, "brbp_0_to_1",    },
        { otp_zone_data_type_t::brbp_ecc, otp_bit_flip_t::single_bit_flip_1_to_0, "brbp_1_to_0",    },
        { otp_zone_data_type_t::brbp_ecc, otp_bit_flip_t::multiple_bits_flipped,  "brbp_multibit",  },
    };


    bool select_otp_zone_menu(otp_zone_t& out_zone) {
        out_zone = otp_zone_t::zone1;
        const char exit_char = 'X';
        // SEGGER_RTT_discard_all_input();
        char selection_indices[] = "123456789ABCD";
        static_assert(count_of(selection_indices) == (MAX_USABLE_ZONE - MIN_USABLE_ZONE + 1 + 1), ""); // +1 for MAX-MIN, +1 for null terminator

        while (true) {
            for (uint8_t i = MIN_USABLE_ZONE; i <= MAX_USABLE_ZONE; ++i) {
                MY_PRINTF("Press <%c> for Zone%c (Start @ Row 0x%03x)\n",
                    selection_indices[i - MIN_USABLE_ZONE],
                    selection_indices[i - MIN_USABLE_ZONE],
                    (i+MIN_USABLE_ZONE) * 0x100
                );
            }
            MY_PRINTF("Press <%c> to exit without selecting a zone\n", exit_char); 

            int char_entered = MY_DEBUG_WAIT_FOR_KEY_TOUPPER();
            MY_DEBUG_DISCARD_REMAINING_LINE_INPUT();
            if ((char_entered == 0x1B) || (char_entered == exit_char)) {
                MY_PRINTF("Exiting without selecting a zone\n");
                return false;
            }
            for (size_t i = 0; i < count_of(selection_indices); ++i) {
                if (char_entered == selection_indices[i]) {
                    MY_PRINTF("Selected %c: %s\n", char_entered, zone_configs[i].name);
                    out_zone = (otp_zone_t)(i); // HACK -- enumeration and menu were designed to cause this to match
                    return true;
                }
            }
        }
    }
    const zone_config_t* select_zone_cfg_menu(void) {

        const char exit_char = 'X';
        // SEGGER_RTT_discard_all_input();

        while (true) {
            MY_PRINTF("Press <ESC> to exit.\r\n");
            char selection_indices[] = "123456789ABCDEFGHIJKLMNOPQRST";
            for (size_t i = 0; i < count_of(zone_configs); ++i) {
                const zone_config_t& cfg { zone_configs[i] };
                MY_PRINTF("Press <%c> for %s\n", selection_indices[i], cfg.name);
            }
            MY_PRINTF("Press <%c> to exit without selecting a zone configuration\n", exit_char); 

            int char_entered = MY_DEBUG_WAIT_FOR_KEY_TOUPPER();
            MY_DEBUG_DISCARD_REMAINING_LINE_INPUT();

            if ((char_entered == 0x1B) || (char_entered == exit_char)) {
                MY_PRINTF("Exiting without selecting a zone configuration\n");
                return nullptr;
            }
            for (size_t i = 0; i < count_of(selection_indices); ++i) {
                if (char_entered == selection_indices[i]) {
                    MY_PRINTF("Selected %c: %s\n", char_entered, zone_configs[i].name);
                    return &zone_configs[i];
                }
            }
        } while(1);
    }

    void interactive_zone_config(void) {

        // two ways to exit:
        // 1. select no zone configuration (e.g., ESC or menu option for same)
        // 2. no more blank zones available to write to

        while (true) {
            bool still_success = true;

            otp_zone_t zone;
            if (!find_blank_zone(zone)) {
                MY_PRINTF("Exiting Interactive Zone Config: No more blank zones available\n");
                return;
            }
            MY_PRINTF("--------------------------------------------------\n");
            MY_PRINTF("Select configuration to *** WRITE *** to 0x%03x .. 0x%03x\n", get_zone_start_row(zone), get_zone_start_row(zone) + OTP_ZONE_ROWCOUNT - 1);
            MY_PRINTF("--------------------------------------------------\n");
            const zone_config_t* cfg = select_zone_cfg_menu();
            if (!cfg) {
                MY_PRINTF("Exiting Interactive Zone Config: No zone configuration selected\n");
                return;
            }
            otp_zone_raw_buffer_t buffer = {0};
            if (still_success) {
                still_success = prepare_zone_buffer(*cfg, buffer);
                if (!still_success) {
                    MY_PRINTF("Failed to initialize config %s (data type %d, bitflip type %d)\n", cfg->name, cfg->base_data_type, cfg->bit_flip_type);
                }
            }

            if (still_success) {
                // log the prepared data and where it went...
                MY_PRINTF("OTP Zone @ 0x%03x - buffer %s prepared (datatype %d, bitflip type %d)\n",
                    get_zone_start_row(zone),
                    cfg->name,
                    cfg->base_data_type,
                    cfg->bit_flip_type
                );
                hexdump(buffer);
                dump_c_struct(buffer, cfg->name);
            }
            if (still_success) {
                int ret = write_zone(zone, buffer);
                still_success = (BOOTROM_OK == ret);
                if (!still_success) {
                    MY_PRINTF(
                        "Failed to write OTP Zone @ 0x%03x: %d\n",
                        get_zone_start_row(zone),
                        ret
                    );
                    still_success = false;
                } else {
                    MY_PRINTF("SUCCESS: Wrote OTP Zone @ 0x%03x with config %s\n", get_zone_start_row(zone), cfg->name);
                }
            }
        }

    }
    void interactive_zone_testing(void) {

        while (true) {
            bool still_success = true;

            MY_PRINTF("--------------------------------------------------\n");
            MY_PRINTF("Select zone to test\n");
            MY_PRINTF("--------------------------------------------------\n");
            otp_zone_t zone;
            if (still_success) {
                still_success = select_otp_zone_menu(zone);
                if (!still_success) {
                    MY_PRINTF("Exiting Interactive Zone Testing: No zone selected\n");
                    return;
                }
            }

            MY_PRINTF("Selected zone @ 0x%03x\n", get_zone_start_row(zone));
            // TODO: auto-detect the zone type
            // TODO: corresponding bootrom / memory-mapped OTP alias read tests (do not trust PICOTOOL)
            MY_PRINTF("ERROR -- NYI\n");
        }
    }



    bool create_custom_otp_zones(void) {

        bool still_success = true;

        static otp_zone_t zones_written[count_of(zone_configs)];

        for (auto i = 0u; still_success && (i < count_of(zone_configs)); ++i) {

            const zone_config_t& cfg { zone_configs[i] };

            otp_zone_t zone;
            otp_zone_raw_buffer_t out_buffer = {0};

            // Step 1: find a blank zone to work with
            if (still_success) {
                if (!find_blank_zone(zone)) {
                    PRINT_ERROR("Failed to find a blank zone to write to\n");
                    still_success = false;
                }
            }

            // Step 2: Prepare the data to be written
            if (still_success) {
                still_success = prepare_zone_buffer(cfg, out_buffer);
                if (!still_success) {
                    PRINT_ERROR("Loop %i: Failed base data init for base data type %d, bitflip type %d\n", i, cfg.base_data_type, cfg.bit_flip_type);
                }
            }

            // Step 3: log the prepared data
            if (still_success) {
                zones_written[i] = zone;
                PRINT_INFO(
                    "%2d: Prepared data for OTP zone starting at 0x%03x, datatype %d, bitflip type %d\n",
                    i,
                    get_zone_start_row(zone),
                    cfg.base_data_type,
                    cfg.bit_flip_type
                );
                hexdump(out_buffer);
                dump_c_struct(out_buffer, cfg.name);
            }

            // Step 4: write to the OTP
            if (still_success) {
                int ret = write_zone(zone, out_buffer);
                if (BOOTROM_OK != ret) {
                    PRINT_ERROR(
                        "Failed to write raw OTP starting at 0x%03x, bitflip type %d: %d\n",
                        get_zone_start_row(zone),
                        cfg.bit_flip_type,
                        ret
                    );
                    still_success = false;
                }
            }

            // No other steps; print success for this zone
            if (still_success) {
                PRINT_INFO(
                    "SUCCESS %2d: Wrote OTP zone starting at 0x%03x, datatype %d, bitflip type %d\n",
                    i,
                    get_zone_start_row(zones_written[i]),
                    cfg.base_data_type,
                    cfg.bit_flip_type
                );
            }
        }
        return still_success;
    }

}


int main_learn_otp(void) {
    for (size_t loop_count = 0; ; ++loop_count) {
        // SEGGER_RTT_discard_all_input();
        MY_PRINTF("Press <1> to create custom OTP zones.\r\n");
        MY_PRINTF("Press <2> to test custom OTP zones.\r\n");
        MY_PRINTF("Press <3> to exhaustively test ECC recovery (takes a few minutes)\r\n");
        MY_PRINTF("Press <4> to initialize whitelabel\r\n");
        MY_PRINTF("Press <5> to add whitelabel manufacturing info string (test data)\r\n");
        MY_PRINTF("Press <X> to exit.\r\n");
 
        char r = 0;
        // wait for 1, space, or escape
        while ( (!(r >= '1' && r <= '5')) ) {
            r = MY_DEBUG_WAIT_FOR_KEY_TOUPPER();
            MY_DEBUG_DISCARD_REMAINING_LINE_INPUT();
        }
        if (r == '1') {
            MY_PRINTF("START: Creating custom OTP zones...\r\n");
            zoned_otp::interactive_zone_config();
            MY_PRINTF("  END: Creating custom OTP zones\r\n");
        } else if (r == '2') {
            MY_PRINTF("START: Testing custom OTP zones...\r\n");
            zoned_otp::interactive_zone_testing();
            MY_PRINTF("  END: Testing custom OTP zones\r\n");
        } else if (r == '3') {
            MY_PRINTF("START: Exhaustive ECC test\r\n");
            MY_PRINTF("       Stage0...\r\n");
            ECC_RECOVERY_EXHAUSTIVE_TEST_RESULT r;
            r = exhaustive_test_of_ecc_recovery(ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_0);
            if (r.as_uint32 != 0) {
                MY_PRINTF("       Stage%d failed: index %d, bit_flipped %d\n", r.stage, r.index, r.bit_flipped);
            }
            MY_PRINTF("       Stage1...\r\n");
            r = exhaustive_test_of_ecc_recovery(ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_1);
            if (r.as_uint32 != 0) {
                MY_PRINTF("       Stage%d failed: index %d, bit_flipped %d\n", r.stage, r.index, r.bit_flipped);
            }
            MY_PRINTF("       Stage2...\r\n");
            r = exhaustive_test_of_ecc_recovery(ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_2);
            if (r.as_uint32 != 0) {
                MY_PRINTF("       Stage%d failed: index %d, bit_flipped %d\n", r.stage, r.index, r.bit_flipped);
            }
            MY_PRINTF("       Stage3...\r\n");
            r = exhaustive_test_of_ecc_recovery(ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_3);
            if (r.as_uint32 != 0) {
                MY_PRINTF("       Stage%d failed: index %d, bit_flipped %d\n", r.stage, r.index, r.bit_flipped);
            }
            MY_PRINTF("       Stage4...\r\n");
            r = exhaustive_test_of_ecc_recovery(ECC_RECOVERY_EXHAUSTIVE_TEST_STAGE_4);
            if (r.as_uint32 != 0) {
                MY_PRINTF("       Stage%d failed: index %d, bit_flipped %d\n", r.stage, r.index, r.bit_flipped);
            }
            MY_PRINTF("  END: Exhaustive ECC test\r\n");
        } else if (r == '4') {
            MY_PRINTF("START: Initializing whitelabel...\r\n");
            MY_PRINTF(" NOTE: this will hard assert on error.\r\n");
            apply_whitelabel_data();
            MY_PRINTF("  END: Initializing whitelabel\r\n");
        } else if (r == '5') {
            MY_PRINTF("START: Adding (test) manufacturing string to existing whitelabel.\r\n");
            const 
            bool worked = apply_manufacturing_string("Rev0 LineA 20250214T012759Z");
            MY_PRINTF("     : Test %s\r\n", worked ? "succeeded" : "FAILED");
            MY_PRINTF("  END: Testing custom OTP zones\r\n");
        } else if (r == 'X') {
            MY_PRINTF("Exiting...\r\n");
            break; // out of for loop
        } else {
            // User pressed unsupported key
        }

    }
    MY_DEBUG_WAIT_FOR_KEY();
    MY_PRINTF("\r\n");

    return 0;
}
