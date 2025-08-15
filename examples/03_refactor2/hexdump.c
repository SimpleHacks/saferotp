#include "hexdump.h"

#include <stdint.h>
#include <ctype.h>
#include <stddef.h>

// Could exclude if not using stdio...
#include <stdio.h>

#include <pico.h>
#include "hardware/platform_defs.h" // for get_core_num()
#include <pico/platform.h> // for PICO_NUM_CORES

#define MAXIMUM_BYTES_PER_OUTPUT_LINE (80u)
void hexdump_header_emitter(output_emitter_t emitter, void* emitter_context) {
    emitter(emitter_context, "Address   00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII\n");
    emitter(emitter_context, "--------  -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --   ----------------\n");
    //                        0....-....1....-....2....-....3....-....4....-....5....-....6....-....7....-. == 77 chars + \n + null
}

static uint8_t gs_line_buffer[NUM_CORES][MAXIMUM_BYTES_PER_OUTPUT_LINE] = {0}; // number of characters (includes `\n` and terminating null)

static inline char q_printable(uint8_t c) {
    return (c >= 0x20 && c <= 0x7E) ? (char)c : '.';
}
void hexdump_emitter(const void * buffer, size_t buffer_size, size_t address_of_first_byte, output_emitter_t emitter, void* emitter_context) {

    const uint8_t *as_bytes = (const uint8_t *)buffer;
    uint8_t * line_buffer = gs_line_buffer[get_core_num()];

    for (size_t offset = 0u; offset < buffer_size; offset += 16u) {
        char *p = line_buffer;
        size_t remaining_buffer_size = MAXIMUM_BYTES_PER_OUTPUT_LINE;

        /* Address: explicitly use lower 32 bits (in case size_t is 64-bit) */
        uint32_t addr = (uint32_t)(address_of_first_byte + offset);
        p[ 0] = "0123456789ABCDEF"[(addr >> (7*4)) & 0xF];
        p[ 1] = "0123456789ABCDEF"[(addr >> (6*4)) & 0xF];
        p[ 2] = "0123456789ABCDEF"[(addr >> (5*4)) & 0xF];
        p[ 3] = "0123456789ABCDEF"[(addr >> (4*4)) & 0xF];
        p[ 4] = "0123456789ABCDEF"[(addr >> (3*4)) & 0xF];
        p[ 5] = "0123456789ABCDEF"[(addr >> (2*4)) & 0xF];
        p[ 6] = "0123456789ABCDEF"[(addr >> (1*4)) & 0xF];
        p[ 7] = "0123456789ABCDEF"[(addr >> (0*4)) & 0xF];

        p[ 8] = ' ';
        p[ 9] = ' ';

        // Hex data ... [10..58]
        // 16 iterations, 3 characters per iteration + 1 extra space at iteration 7 == 49 bytes output
        uint8_t * pp = p+10;
        for (size_t i = 0u; i < 16u; ++i) {
            
            if (i + offset < buffer_size) {
                uint8_t byte = as_bytes[offset + i];
                *pp++ = "0123456789ABCDEF"[(byte >> (1*4)) & 0xF];
                *pp++ = "0123456789ABCDEF"[(byte >> (0*4)) & 0xF];
            } else {
                *pp++ = ' ';
                *pp++ = ' ';
            }
            *pp++ = ' ';
            if (i == 7)  {
                *pp++ = ' ';
            }
        }
        p[59] = ' ';
        p[60] = ' ';

        // Accesses p[61..76]
        for (size_t i = 0; i < 16 && (i + offset) < buffer_size; ++i) {
            p[61 + i] = q_printable(as_bytes[offset + i]);
        }
        p[77] = '\n';
        p[78] = '\0';

        /* Emit full line */
        emitter(emitter_context, line_buffer);
    }
}

void emitter_fputs(void *emitter_context, const char * null_terminated_string) {
    fputs(null_terminated_string, emitter_context);
}

void hexdump(const void *buffer, size_t buffer_size, size_t address_of_first_byte) {
    hexdump_emitter(buffer, buffer_size, address_of_first_byte, emitter_fputs, stdout);
}
void hexdump_header() {
    hexdump_header_emitter(emitter_fputs, stdout);
}
