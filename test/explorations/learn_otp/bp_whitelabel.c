#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include "pico.h"

#include "bp_whitelabel.h"
#include <boot/bootrom_constants.h>
#include "pico/bootrom.h"
#include "saferotp.h"

#define MY_DEBUG_OVERRIDE_DEFAULT_CATEGORY MY_DEBUG_CAT_03
#include "debug_rtt.h"


static volatile bool g_WaitForKey_Whitelabel = false;
#define WAIT_FOR_KEY()                 \
    do {                               \
        if (g_WaitForKey_Whitelabel) { \
            MY_DEBUG_WAIT_FOR_KEY();   \
        }                              \
    } while (0)

static char byte_to_printable_char(uint8_t byte) {
    if (byte < 0x20u) {
        return '.';
    }
    if (byte > 0x7Eu) {
        return '.';
    }
    return byte;
}

/* GCC is awesome. */
#define ARRAY_SIZE(arr) \
    (sizeof(arr) / sizeof((arr)[0]) \
     + sizeof(typeof(int[1 - 2 * \
           !!__builtin_types_compatible_p(typeof(arr), \
                 typeof(&arr[0]))])) * 0)


typedef struct _OTP_USB_BOOT_FLAGS {
    union {
        uint32_t as_uint32;
        struct {
            uint32_t usb_vid_valid          : 1; //  0: 0x000001
            uint32_t usb_pid_valid          : 1; //  1: 0x000002
            uint32_t usb_bcd_valid          : 1; //  2: 0x000004
            uint32_t usb_lang_id_valid      : 1; //  3: 0x000008
            uint32_t usb_manufacturer_valid : 1; //  4: 0x000010
            uint32_t usb_product_valid      : 1; //  5: 0x000020
            uint32_t usb_serial_valid       : 1; //  6: 0x000040
            uint32_t usb_max_power_valid    : 1; //  7: 0x000080
            uint32_t volume_label_valid     : 1; //  8: 0x000100
            uint32_t scsi_vendor_valid      : 1; //  9: 0x000200
            uint32_t scsi_product_valid     : 1; // 10: 0x000400
            uint32_t scsi_rev_valid         : 1; // 11: 0x000800
            uint32_t redirect_url_valid     : 1; // 12: 0x001000
            uint32_t redirect_name_valid    : 1; // 13: 0x002000
            uint32_t info_uf2_model_valid   : 1; // 14: 0x004000
            uint32_t info_uf2_boardid_valid : 1; // 15: 0x008000
            uint32_t _rfu_16                : 1; // 16: 0x010000
            uint32_t _rfu_17                : 1; // 17: 0x020000
            uint32_t _rfu_18                : 1; // 18: 0x040000
            uint32_t _rfu_19                : 1; // 19: 0x080000
            uint32_t _rfu_20                : 1; // 20: 0x100000
            uint32_t _rfu_21                : 1; // 21: 0x200000
            uint32_t white_label_addr_valid : 1; // 22: 0x400000
            uint32_t _rfu_23                : 1; // 23: 0x800000
            uint32_t _must_be_zero          : 8; // 24..31 are not used in raw OTP data
        };
    };
} OTP_USB_BOOT_FLAGS;



//#define USB_WHITELABEL_PRODUCT_STRING "5XL"
#define USB_WHITELABEL_PRODUCT_STRING "6"
// #define USB_WHITELABEL_PRODUCT_STRING "22" // force an odd string length, ensure tests trailing null works OK
// #define USB_WHITELABEL_PRODUCT_STRING "Really Long Version" // expected to error ... extension limited to 7 characters
// #define USB_WHITELABEL_PRODUCT_STRING "Bus Pirate 6" // expected to error ... just the extension "6"

#define USB_WHITELABEL_MAX_CHARS_BP_VERSION 8
#define USB_WHITELABEL_MAX_CHARS_BP_MANU    40


static const uint16_t _static_portion[] = {
    // offset 0x10, chars 0x16: "https://buspirate.com/"
    // offset 0x14, chars 0x0D:       "buspirate.com"
    0x7468, // `ht` // Row 0x0d0 -- offset 0x10
    0x7074, // `tp` // Row 0x0d1 -- offset 0x11
    0x3a73, // `s:` // Row 0x0d2 -- offset 0x12
    0x2f2f, // `//` // Row 0x0d3 -- offset 0x13
    0x7562, // `bu` // Row 0x0d4 -- offset 0x14
    0x7073, // `sp` // Row 0x0d5 -- offset 0x15
    0x7269, // `ir` // Row 0x0d6 -- offset 0x16
    0x7461, // `at` // Row 0x0d7 -- offset 0x17
    0x2e65, // `e.` // Row 0x0d8 -- offset 0x18
    0x6f63, // `co` // Row 0x0d9 -- offset 0x19
    0x2f6d, // `m/` // Row 0x0da -- offset 0x1a
    // offset 0x1B, chars 0x08: "BP__BOOT"
    0x5042, // `BP` // Row 0x0db -- offset 0x1b
    0x5f5f, // `__` // Row 0x0dc -- offset 0x1c
    0x4f42, // `BO` // Row 0x0dd -- offset 0x1d
    0x544f, // `OT` // Row 0x0de -- offset 0x1e
    // offset 0x1F, chars 0x08: "Bus Pir8"
    0x7542, // `Bu` // Row 0x0df -- offset 0x1f
    0x2073, // `s ` // Row 0x0e0 -- offset 0x20
    0x6950, // `Pi` // Row 0x0e1 -- offset 0x21
    0x3872, // `r8` // Row 0x0e2 -- offset 0x22
    // offset 0x23, chars 0x0A: "Bus Pirate"
    0x7542, // `Bu` // Row 0x0e3 -- offset 0x23
    0x2073, // `s ` // Row 0x0e4 -- offset 0x24
    0x6950, // `Pi` // Row 0x0e5 -- offset 0x25
    0x6172, // `ra` // Row 0x0e6 -- offset 0x26
    0x6574, // `te` // Row 0x0e7 -- offset 0x27
};
static_assert(ARRAY_SIZE(_static_portion) == 24);
static const char  _product_string[] = " " USB_WHITELABEL_PRODUCT_STRING;
// note: using a string ensures the byte immediately following the last charcter is readable
//       which is important since writing ECC requires an even number of bytes.
static_assert(sizeof(char) == sizeof(uint8_t), "char must be 8-bits");
static_assert(ARRAY_SIZE(_product_string) <= USB_WHITELABEL_MAX_CHARS_BP_VERSION + 1);

// NOTE: Reserved for product string:
//                  // Rows 0x0e8 .. 0x0eb (space character + 7 additional characters maximum)

// NOTE: Reserved for manufacturing data:
//                  // Rows 0x0ec .. 0x0ff (40 characters maximum)

// Leaving the manufacturing data portion uncoded...
// That will be filled in by another process

void apply_whitelabel_data(void) {
    static const uint16_t base = 0x0c0; // written so this can be changed easily

    static const size_t product_extension_rows = sizeof(_product_string) /2u; // sizeof() includes null; round down to even number

    uint16_t product_char_count = strlen("Bus Pirate") + strlen(_product_string);
    
    // 1. write the static portion         --> Rows 0x0d0 .. 0x0e7
    for (uint16_t i = 0; i < ARRAY_SIZE(_static_portion); ++i) {
        uint16_t row = base + 0x10u +  i;
        PRINT_DEBUG("Write static portion index %d: row 0x%03x, data 0x%04x\n", i, row, _static_portion[i]);
        WAIT_FOR_KEY();
        if (!saferotp_write_single_row_ecc(row, _static_portion[i])) {
            PRINT_ERROR("Failed to write static portion index %d: row 0x%03x, data 0x%04x\n", i, row, _static_portion[i]);
            WAIT_FOR_KEY();
            return;
        }
    }
    PRINT_DEBUG("Version extension: '%s'\n", _product_string);
    // 2. also write the product version extension
    for (uint16_t i = 0; i < product_extension_rows; ++i) {
        uint16_t row = base + 0x28u + i;
        uint16_t data = 0;
        // First  character is stored in LSB
        // Second character is stored in MSB
        data  |= (uint8_t)(_product_string[2u * i + 1]);
        data <<= 8;
        data  |= (uint8_t)(_product_string[2u * i    ]);

        PRINT_DEBUG("Write product version extension index %d: row 0x%03x, data 0x%04x\n", i, row, data);
        WAIT_FOR_KEY();
        if (!saferotp_write_single_row_ecc(row, data)) {
            PRINT_ERROR("Failed to write product version extension index %d: row 0x%03x, data 0x%04x\n", i, row, data);
            WAIT_FOR_KEY();
            return;
        }
    }

    // NOTE: DOES NOT WRITE THE MANUFACTURING DATA PORTION

    // 3. encode the product string length and offset
    uint16_t tmp_p = 0x2300 | product_char_count;

    // 4. Write the portions of the first 16 rows that have valid data:
    //write_single_otp_ecc_row_or_die(base + 0x.u, 0x....u); // USB BCD Device
    //write_single_otp_ecc_row_or_die(base + 0x.u, 0x....u); // USB LangID for strings
    //write_single_otp_ecc_row_or_die(base + 0x.u, 0x....u); // USB Serial Number
    //write_single_otp_ecc_row_or_die(base + 0x.u, 0x....u); // USB config attributes & max power
    uint16_t data;
    data = 0x1209u; PRINT_DEBUG("Write WHITELABEL index 0: row 0x%03x data 0x%04x\n", base + 0x0u, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0x0u, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0x0u, data); WAIT_FOR_KEY(); };   // USB VID == 0x1209
    data = 0x7332u; PRINT_DEBUG("Write WHITELABEL index 1: row 0x%03x data 0x%04x\n", base + 0x1u, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0x1u, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0x1u, data); WAIT_FOR_KEY(); };   // USB PID == 0x7332
    data = 0x230Au; PRINT_DEBUG("Write WHITELABEL index 4: row 0x%03x data 0x%04x\n", base + 0x4u, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0x4u, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0x4u, data); WAIT_FOR_KEY(); };   // USB MANU              ten   chars @ offset 0x23
    data = tmp_p  ; PRINT_DEBUG("Write WHITELABEL index 5: row 0x%03x data 0x%04x\n", base + 0x5u, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0x5u, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0x5u, data); WAIT_FOR_KEY(); };   // USB PROD              XXX   chars @ offset 0x23
    data = 0x1B08u; PRINT_DEBUG("Write WHITELABEL index 8: row 0x%03x data 0x%04x\n", base + 0x8u, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0x8u, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0x8u, data); WAIT_FOR_KEY(); };   // STOR VOLUME LABEL     eight chars @ offset 0x1b
    data = 0x1F08u; PRINT_DEBUG("Write WHITELABEL index 9: row 0x%03x data 0x%04x\n", base + 0x9u, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0x9u, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0x9u, data); WAIT_FOR_KEY(); };   // SCSI INQUIRY VENDOR   eight chars @ offset 0x1f
    data = tmp_p  ; PRINT_DEBUG("Write WHITELABEL index A: row 0x%03x data 0x%04x\n", base + 0xAu, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0xAu, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0xAu, data); WAIT_FOR_KEY(); };   // SCSI INQUIRY PRODUCT  XXX   chars @ offset 0x23
    data = 0x1016u; PRINT_DEBUG("Write WHITELABEL index C: row 0x%03x data 0x%04x\n", base + 0xCu, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0xCu, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0xCu, data); WAIT_FOR_KEY(); };   // redirect URL          22    chars @ offset 0x10
    data = 0x140du; PRINT_DEBUG("Write WHITELABEL index D: row 0x%03x data 0x%04x\n", base + 0xDu, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0xDu, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0xDu, data); WAIT_FOR_KEY(); };   // redirect name         13    chars @ offset 0x14
    data = tmp_p  ; PRINT_DEBUG("Write WHITELABEL index E: row 0x%03x data 0x%04x\n", base + 0xEu, data); WAIT_FOR_KEY(); if (saferotp_write_single_row_ecc(base + 0xEu, data)) { PRINT_ERROR("Failed write whitelabel OTP row 0x%03x data %04x\n", base + 0xEu, data); WAIT_FOR_KEY(); };   // info_uf2.txt product  XXX   chars @ offset 0x23
    //write_single_otp_ecc_row_or_die(base + 0xFu, 0x2Cxxu); // info_uf2.txt manufacturing string ... to be added later

    // 5. write the `WHITE_LABEL_ADDR` to point to the base address used here
    PRINT_DEBUG("Write WHITELABEL_BASE_ADDR 0x%03x to row 0x05C\n", base);
    WAIT_FOR_KEY();
    if (!saferotp_write_single_row_ecc(0x05c, base)) {
        PRINT_ERROR("Failed to write WHITELABEL_BASE_ADDR 0x%03x to row 0x05C\n", base);
        WAIT_FOR_KEY();
        return;
    }

    // 6. NON-ECC write the USB boot flags ... writes go to three (3) consecutive rows
    OTP_USB_BOOT_FLAGS usb_boot_flags = {
        .usb_vid_valid          = 1,
        .usb_pid_valid          = 1,
        .usb_bcd_valid          = 0,
        .usb_lang_id_valid      = 0,
        .usb_manufacturer_valid = 1,
        .usb_product_valid      = 1,
        .usb_serial_valid       = 0,
        .usb_max_power_valid    = 0,
        .volume_label_valid     = 1,
        .scsi_vendor_valid      = 1,
        .scsi_product_valid     = 1,
        .scsi_rev_valid         = 0,
        .redirect_url_valid     = 1,
        .redirect_name_valid    = 1,
        .info_uf2_model_valid   = 1,
        .info_uf2_boardid_valid = 0,
        .white_label_addr_valid = 1,
    };
    hard_assert(usb_boot_flags.as_uint32 == 0x407733u); // manually calculated ... should match the friendly description above.


    // 7. Read existing USB boot flags as RBIT3
    OTP_USB_BOOT_FLAGS old_usb_boot_flags;
    PRINT_DEBUG("Reading USB BOOT FLAGS as RBIT3 starting at OTP Row 0x59\n");
    if (!saferotp_read_redundant_rows_RBIT3(0x059, &old_usb_boot_flags.as_uint32)) {
        PRINT_DEBUG("WHITELABEL ERROR: Could not read existing OTP rows 0x59..0x5B (USB BOOT FLAGS) as RBIT3 ... perhaps did not find majority agreement?\n");
        WAIT_FOR_KEY();
        return;
    }

    if ((old_usb_boot_flags.as_uint32 & usb_boot_flags.as_uint32) == usb_boot_flags.as_uint32) {
        // No need to write ... all the requested flags are already set
        PRINT_DEBUG("WHITELABEL INFO: USB BOOT FLAGS already set\n");
        WAIT_FOR_KEY();
    }
    else {
        if ((old_usb_boot_flags.as_uint32 & ~usb_boot_flags.as_uint32) != 0) {
            PRINT_DEBUG("WHITELABEL WARNING: Old USB BOOT FLAGS 0x%06x vs. intended %06x has additional bits: %06x\n",
                old_usb_boot_flags.as_uint32, usb_boot_flags.as_uint32,
                old_usb_boot_flags.as_uint32 & ~usb_boot_flags.as_uint32
            );
            // continue anyways?  This allow the code to run even after manufacturing string has been applied....
        }
        OTP_USB_BOOT_FLAGS new_flags = { .as_uint32 = old_usb_boot_flags.as_uint32 | usb_boot_flags.as_uint32 };
    
        // 8. Write the new USB BOOT FLAGS, RBIT3 encoded
        PRINT_DEBUG("Writing USB BOOT FLAGS: 0x%06x as RBIT3 starting at OTP Row 0x59\n", new_flags.as_uint32);
        WAIT_FOR_KEY();
        if (!saferotp_write_redundant_rows_RBIT3(0x059, usb_boot_flags.as_uint32)) {
            PRINT_DEBUG("WHITELABEL ERROR: Failed to write USB BOOT FLAGS\n");
            WAIT_FOR_KEY();
            return;
        }
    }
    PRINT_DEBUG("WHITELABEL INFO: USB BOOT FLAGS updated\n");
    WAIT_FOR_KEY();
    return;

    // That's it!
}


bool apply_manufacturing_string(const char* manufacturing_data_string) {

    // Follow the breadcrumbs to find the base address
    OTP_USB_BOOT_FLAGS old_usb_boot_flags;
    if (!saferotp_read_redundant_rows_RBIT3(0x059, &old_usb_boot_flags.as_uint32)) {
        PRINT_DEBUG("WHITELABEL ERROR: Raw OTP rows 0x59..0x5B (USB BOOT FLAGS) did not find majority agreement\n");
        WAIT_FOR_KEY();
        return false;
    }
    PRINT_DEBUG("found USB BOOT FLAGS: %06x\n", old_usb_boot_flags.as_uint32);

    // validate the USB BOOT FLAGS?
    if (!old_usb_boot_flags.white_label_addr_valid) {
        PRINT_DEBUG("WHITELABEL ERROR: No white label data to update (%06x)\n", old_usb_boot_flags.as_uint32);
        WAIT_FOR_KEY();
        return false;
    }

    uint16_t base;
    if (!saferotp_read_single_row_ecc(0x5C, &base)) {
        PRINT_DEBUG("WHITELABEL ERROR: OTP row 0x5C could not be read (WHITE_LABEL_ADDR)\n");
        WAIT_FOR_KEY();
        return false;
    }
    if (base != 0x0C0) {
        // TODO: other validation as needed
        PRINT_DEBUG("WHITELABEL ERROR: OTP row 0x5C (WHITE_LABEL_ADDR) is not set to 0x0C0, but 0x%03x\n", base);
        WAIT_FOR_KEY();
        return false;
    }
    PRINT_DEBUG("WHITE_LABEL_ADDR points to row index 0x%03x\n", base);

    // Validate the static portion of the strings matches
    // That static portion starts at offset 0x10 from the base whitelabel structure.
    if (true) {
        uint16_t otp_static_copy[ARRAY_SIZE(_static_portion)] = { 0 };
        if (!saferotp_read_ecc_data(base + 0x10u, otp_static_copy, sizeof(otp_static_copy))) {
            PRINT_DEBUG("WHITELABEL ERROR: Failed to read static portion of white label data at row 0x%03x\n", base + 0x10);
            WAIT_FOR_KEY();
            return false;
        }
        if (memcmp(otp_static_copy, _static_portion, sizeof(otp_static_copy)) != 0) {
            PRINT_DEBUG("WHITELABEL ERROR: Static portion of white label data at row 0x%03x does not match\n", base);
            WAIT_FOR_KEY();
            return false;
        }
    }

    PRINT_DEBUG("Static portion of the strings look ok\n");
    size_t char_count = strlen(manufacturing_data_string);
    if (char_count > 40) {
        PRINT_DEBUG("WHITELABEL ERROR: Manufacturing data string is too long (%d chars)\n", char_count);
        WAIT_FOR_KEY();
        return false;
    }
    uint16_t strdef = 0x2c00u | char_count;

    // check if the STRDEF is already set ... if it is, verify it matches the current provided string's length
    uint16_t oldstrdef;
    if (!saferotp_read_single_row_ecc(base + 0xFu, &oldstrdef)) {
        PRINT_DEBUG("WHITELABEL ERROR: Failed to read prior manufacturing STRDEF\n");
        WAIT_FOR_KEY();
        return false;
    } else if (oldstrdef == 0u) {
        // This is OK, it means the string was not set yet.
    } else if (oldstrdef != strdef) {
        PRINT_DEBUG("WHITELABEL ERROR: Old manufacturing STRDEF (0x%06x) does not match new (0x%06x)\n", oldstrdef, strdef);
        WAIT_FOR_KEY();
        return false;
    }

    PRINT_DEBUG("Writing the manufacturing string data\n");
    WAIT_FOR_KEY();

    // first write the actual string's data
    for (size_t i = 0; i < (char_count+1)/2; ++i) {
        uint16_t row = base + 0x2Cu + i;
        uint16_t data = 0;
        // First  character is stored in LSB
        // Second character is stored in MSB
        data  |= (uint8_t)(manufacturing_data_string[2u * i + 1]);
        data <<= 8;
        data  |= (uint8_t)(manufacturing_data_string[2u * i    ]);

        PRINT_DEBUG("Writing row 0x%03x: 0x%04x (`%c%c`)\n", row, data, byte_to_printable_char(data & 0xFFu), byte_to_printable_char(data >> 8));
        WAIT_FOR_KEY();

        if (!saferotp_write_single_row_ecc(row, data)) {
            PRINT_DEBUG("WHITELABEL ERROR: Failed with partial manufacturing string data written.\n");
            WAIT_FOR_KEY();
            return false;
        }
    }

    PRINT_DEBUG("Writing row 0x%03x: STRDEF %04x\n", base + 0xFu, strdef);
    WAIT_FOR_KEY();
    if (!saferotp_write_single_row_ecc(base + 0xFu, strdef)) {
        PRINT_DEBUG("WHITELABEL ERROR: Failed to write manufacturing string length and offset\n");
        WAIT_FOR_KEY();
    }

    PRINT_DEBUG("Updating the USB_BOOT_FLAGS to mark the manufacturing string as valid\n");

    OTP_USB_BOOT_FLAGS new_usb_boot_flags = { .as_uint32 = old_usb_boot_flags.as_uint32 };
    new_usb_boot_flags.info_uf2_boardid_valid = 1;

    PRINT_DEBUG("Writing rows 0x059..0x05B: 0x%06x --> 0x%06x\n", old_usb_boot_flags.as_uint32, new_usb_boot_flags.as_uint32);
    WAIT_FOR_KEY();

    if (!saferotp_write_redundant_rows_RBIT3(0x059, new_usb_boot_flags.as_uint32)) {
        PRINT_DEBUG("Failed to update USB BOOT FLAGs to value %06x\n", new_usb_boot_flags.as_uint32);
        WAIT_FOR_KEY();
        return false;
    }

    if (base != 0x0c0) {
        PRINT_DEBUG("WHITELABEL ERROR: Not yet supporting locking of OTP page unless base address is 0x0C0\n");
        WAIT_FOR_KEY();
        return false;
    }
    // Finally, update page protections for page 3 (`0x0c0 .. 0x0ff`).
    // Row `0xF86`, PAGE3_LOCK0 = `0x3F3F3F`; 0x3F = `0b00'111'111`  : NO_KEY_STATE: 0b00, KEY_R: 0b111, KEY_W: 0b111 (no key; read-only without key)
    // Row `0xF87`, PAGE3_LOCK1 = `0x151515`; 0x15 = `0b00'01'01'01` : LOCK_S: 0b01, LOCK_NS: 0b01, LOCK_BL: 0b01 (read-only for all)
    PRINT_DEBUG("Setting PAGE3_LOCK0 (0xF86) to `0x3Fu` (no keys; read-only without key)\n");
    WAIT_FOR_KEY();
    if (!saferotp_write_single_row_redundant_byte3x(0xF86u, 0x3Fu)) {
        PRINT_DEBUG("WHITELABEL ERROR: Failed to write PAGE3_LOCK0\n");
        WAIT_FOR_KEY();
        return false;
    }

    PRINT_DEBUG("Setting PAGE3_LOCK1 (0xF87) to `0x151515` (read-only for all three)\n");
    WAIT_FOR_KEY();
    if (!saferotp_write_single_row_redundant_byte3x(0xF87, 0x15u)) {
        PRINT_DEBUG("WHITELABEL ERROR: Failed to write PAGE3_LOCK1\n");
        WAIT_FOR_KEY();
        return false;
    }

    PRINT_DEBUG("Manufacturing string successfully applied and locked down.\n");
    WAIT_FOR_KEY();
    return true;
}



                 