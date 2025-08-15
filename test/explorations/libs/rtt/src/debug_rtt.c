#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <pico.h>
#include "debug_rtt.h"

#if 1 // Include additional headers based on defined DEBUG_INPUT_xxx and DEBUT_OUTPUT_xxx

#if defined(DEBUG_OUTPUT_SEGGER_RTT) || defined(DEBUG_INPUT_SEGGER_RTT)
    #include "SEGGER_RTT.h"
#endif
#if defined(DEBUG_OUTPUT_TINYUSB_CDC_ITF) || defined(DEBUG_INPUT_TINYUSB_CDC_ITF)
    #include "tusb.h"
#endif

#endif // Include additional headers based on defined DEBUG_INPUT_xxx and DEBUT_OUTPUT_xxx

#if 1 // Validate reasonable values for DEBUG_INPUT_xxx and DEBUT_OUTPUT_xxx
#if defined(DEBUG_OUTPUT_TINYUSB_CDC_ITF)
    #if !(DEBUG_OUTPUT_TINYUSB_CDC_ITF >= 0)
        #error "DEBUG_OUTPUT_TINYUSB_CDC_ITF must be non-negative integer"
    #endif
    #if defined(CFG_TUD_CDC) && !(DEBUG_OUTPUT_TINYUSB_CDC_ITF < CFG_TUD_CDC)
        #error "DEBUG_OUTPUT_TINYUSB_CDC_ITF defined, but greater than CFG_TUD_CDC"
    #endif
#endif
#if defined(DEBUG_INPUT_TINYUSB_CDC_ITF)
    #if !(DEBUG_INPUT_TINYUSB_CDC_ITF >= 0)
        #error "DEBUG_INPUT_TINYUSB_CDC_ITF must be non-negative integer"
    #endif
    #if defined(CFG_TUD_CDC) && !(DEBUG_INPUT_TINYUSB_CDC_ITF < CFG_TUD_CDC)
        #error "DEBUG_INPUT_TINYUSB_CDC_ITF defined, but greater than CFG_TUD_CDC"
    #endif
#endif
#endif // Validate reasonable values for DEBUG_INPUT_xxx and DEBUT_OUTPUT_xxx


my_debug_level_t _MY_DEBUG_LEVELS[_MY_E_DEBUG_CAT_TEMP+1u] = { // default levels for all debug ouput
    [_MY_E_DEBUG_CAT_CATCHALL] = MY_DEBUG_LEVEL_NEVER, // means "always" when set here
    // [_MY_E_DEBUG_CAT_01      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_02      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_03      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_04      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_05      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_06      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_07      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_08      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_09      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_10      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_11      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_12      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_13      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_14      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_15      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_16      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_17      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_18      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_19      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_20      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_21      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_22      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_23      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_24      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_25      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_26      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_27      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_28      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_29      ] = MY_DEBUG_LEVEL_DEBUG,
    // [_MY_E_DEBUG_CAT_30      ] = MY_DEBUG_LEVEL_DEBUG,
    // NOTE: the last (largest value) category must be the TEMP category.  Depended upon in multiple places in below code.
    //       This is used for temporary debug messages.
    // [_MY_E_DEBUG_CAT_TEMP    ] = MY_DEBUG_LEVEL_DEBUG,
};

inline static const char * get_string_for_debug_level(my_debug_level_t level) {
    if (level.level == MY_DEBUG_LEVEL_FATAL.level) {
        return "  FATAL: ";
    } else if (level.level == MY_DEBUG_LEVEL_ERROR.level) {
        return "  ERROR: ";
    } else if (level.level == MY_DEBUG_LEVEL_WARNING.level) {
        return "WARNING: ";
    } else if (level.level == MY_DEBUG_LEVEL_INFO.level) {
        return "   INFO: ";
    } else if (level.level == MY_DEBUG_LEVEL_VERBOSE.level) {
        return "VERBOSE: ";
    } else if (level.level == MY_DEBUG_LEVEL_DEBUG.level) {
        return "  DEBUG: ";
    } else if (level.level == MY_DEBUG_LEVEL_NEVER.level) {
        return "  NEVER: ";
    } else {
        return "  (...): ";
    }
}


// TODO: Format the string to a temporary buffer (per-core buffer?)
// Dependent upon formatting the string to a temporary buffer....
// TODO: Way to enable/disable level/cateogry/file/line/func for each message?
// TODO: Add terminal ID to the output (2 byte prefix, 2 byte suffix)
// TODO: Output via SEGGER_RTT_WriteDownBuffer() to ensure atomic output.


#define BYTES_PER_BUFFER 1024u
char per_core_print_buffer[2u][BYTES_PER_BUFFER] = {0}; // 2k RAM just for printing
static size_t maximum_written_statistic[2u] = {0};


__attribute__((weak))
int my_printf_impl(const char* format, ...) {

    // format into a temporary buffer once
    // then write that pre-formatted buffer to the output stream(s)
    // This reduces formatting overhead when using multi-stream output.
    const uint8_t core = get_core_num();
    char * buffer = per_core_print_buffer[core];
    int bytes_written = 0;

    // format the output buffer....
    if (true) {
        int bytes_remaining = BYTES_PER_BUFFER - 1u; // trailing NULL

        // Format the string to the buffer
        va_list ParamList;
        va_start(ParamList, format);
        int count = vsnprintf(&buffer[bytes_written], bytes_remaining, format, ParamList);
        va_end(ParamList);

        // vsnprintf() returns the number of bytes that ***would*** have been written
        // if the buffer was large enough.  If truncated string, don't try to send more
        // than the remaining space in the buffer.
        if (count >= bytes_remaining) {
            // truncated output ... 
            bytes_written += bytes_remaining;
        } else {
            bytes_written += count;
        }
    }

    // track statistics on maximum length string written
    if (bytes_written > maximum_written_statistic[core]) {
        maximum_written_statistic[core] = bytes_written;
    }

#if defined(DEBUG_OUTPUT_SEGGER_RTT)
    // write to the RTT output
    SEGGER_RTT_WriteDownBuffer(0, buffer, bytes_written);
#endif // DEBUG_OUTPUT_SEGGER_RTT
#if defined(DEBUG_OUTPUT_TINYUSB_CDC_ITF)
    // write to the TinyUSB CDC output
    if (tud_cdc_n_connected(DEBUG_OUTPUT_TINYUSB_CDC_ITF)) {
        tud_cdc_n_write(DEBUG_OUTPUT_TINYUSB_CDC_ITF, buffer, bytes_written);
        tud_cdc_n_write_flush(DEBUG_OUTPUT_TINYUSB_CDC_ITF);
    }
#endif // DEBUG_OUTPUT_TINYUSB_CDC_ITF       
    return bytes_written;
}

// mark this function as `weak` so that it can be overridden
__attribute__((weak))
int my_debug_print_impl(
    my_debug_level_t level,
    my_debug_category_t category,
    const char * file,
    size_t line,
    const char * function,
    const char * format,
    ...
) {

    int core = get_core_num();

    // print to per_core_print_buffer first
    // then write to the output streams
    char * buffer = per_core_print_buffer[get_core_num()];
    int bytes_written = 0;
    const char terminal_id = 0;
    //const char terminal_id = (char)(category.category % 16u); // can opt for non-default terminal ID?
    const bool add_level = true;
    const bool add_file_and_line = (file != NULL);
    const bool add_function = (function != NULL);
    const bool add_message = (format != NULL);

    // format the output buffer...
    if (true) {
        int bytes_remaining = BYTES_PER_BUFFER - 1u; // trailing NULL
        
        // from SEGGER_RTT_SetTerminal(), terminal is changed
        // by sending 0xFF following by the terminal ID (range: [0x00..0x0F])
        // Do NOT send if the terminal ID is 0x00, to avoid accidental truncating
        // of the string....
        if (terminal_id != 0x00) {
            buffer[0] = 0xFF; // terminal ID indicator
            buffer[1] = terminal_id; // terminal ID
            bytes_written += 2;
            static_assert(BYTES_PER_BUFFER > 4u, "Buffer must be larger than 4 bytes to have any usable output");
            bytes_remaining -= 4; // reserve 2 bytes to return to terminal ID 0....
        }

        if (add_level) {
            const char * level_string = get_string_for_debug_level(level);
            int level_string_length = strlen(level_string);
            if (level_string_length < bytes_remaining) {
                memcpy(&buffer[bytes_written], level_string, level_string_length);
                bytes_written += level_string_length;
                bytes_remaining -= level_string_length;
            }
        }
        if (add_file_and_line) {
            int count = snprintf(&buffer[bytes_written], bytes_remaining, "%s:%zu: ", file, line);
            if (count >= bytes_remaining) {
                bytes_written += bytes_remaining;
                bytes_remaining = 0;
            } else {
                bytes_written += count;
                bytes_remaining -= count;
            }
        }        
        if (add_function) {
            int count = snprintf(&buffer[bytes_written], bytes_remaining, "%s ", function);
            if (count >= bytes_remaining) {
                bytes_written += bytes_remaining;
                bytes_remaining = 0;
            } else {
                bytes_written += count;
                bytes_remaining -= count;
            }
        }
        if (add_message) {
            // Format the string to the buffer
            va_list ParamList;
            va_start(ParamList, format);
            int count = vsnprintf(&buffer[bytes_written], bytes_remaining, format, ParamList);
            va_end(ParamList);
            if (count >= bytes_remaining) {
                bytes_written += bytes_remaining;
                bytes_remaining = 0;
            } else {
                bytes_written += count;
                bytes_remaining -= count;
            }
        }

        if (terminal_id != 0x00) {
            // space was reserved above for the "return to terminal zero" characters
            assert(bytes_written < BYTES_PER_BUFFER-3u);
            // return to terminal ID 0x00
            buffer[bytes_written] = 0xFF;
            buffer[bytes_written + 1] = 0x00; // terminal ID
            buffer[bytes_written + 2] = 0x00; // terminal ID
            bytes_written += 2;
        }
    }

    if (bytes_written > maximum_written_statistic[core]) {
        maximum_written_statistic[core] = bytes_written;
    }

    // Note: Buffer may start with RTT-specific prefix for terminal ID,
    //       (and output with same), so output via RTT first.
#if defined(DEBUG_OUTPUT_SEGGER_RTT)
    // write to the RTT output
    SEGGER_RTT_WriteDownBuffer(0, buffer, bytes_written);
#endif // DEBUG_OUTPUT_SEGGER_RTT
    // remove the RTT-specific terminal ID prefix / suffix
    if (terminal_id != 0x00) {
        buffer[bytes_written - 2u] = 0x00; // move NULL terminator two bytes earlier
        bytes_written -= 2u;
        // move buffer pointer two bytes forward
        buffer += 2u;
        bytes_written -= 2u;
    }
#if defined(DEBUG_OUTPUT_TINYUSB_CDC_ITF)
    // write to the TinyUSB CDC output
    if (tud_cdc_n_connected(DEBUG_OUTPUT_TINYUSB_CDC_ITF)) {
        tud_cdc_n_write(DEBUG_OUTPUT_TINYUSB_CDC_ITF, buffer, bytes_written);
        tud_cdc_n_write_flush(DEBUG_OUTPUT_TINYUSB_CDC_ITF);
    }
#endif // DEBUG_OUTPUT_TINYUSB_CDC_ITF       
    return bytes_written;
}

typedef enum _input_source_t {
    INPUT_SOURCE_UNDEFINED   = 0,
    INPUT_SOURCE_SEGGER_RTT  = 1,
    INPUT_SOURCE_TINYUSB_CDC = 2,
} input_source_t;

// keep track of the last input used, so when
// call discard_remaining_line_input() we can
// discard only from that last-used input source.
static input_source_t last_input_source = INPUT_SOURCE_UNDEFINED;

void my_debug_discard_all_input_impl(void) {
    int c;
#if defined(DEBUG_INPUT_SEGGER_RTT)
    do {
        c = SEGGER_RTT_GetKey();
    } while (c >= 0);
#endif // DEBUG_INPUT_SEGGER_RTT

#if defined(DEBUG_INPUT_TINYUSB_CDC_ITF) 
    if (tud_cdc_n_connected(DEBUG_INPUT_TINYUSB_CDC_ITF)) {
        tud_cdc_n_read_flush(DEBUG_INPUT_TINYUSB_CDC_ITF);
    }
#endif // DEBUG_INPUT_TINYUSB_CDC_ITF

    last_input_source = INPUT_SOURCE_UNDEFINED;
    return;
}
void my_debug_discard_remaining_line_input_impl(void) {
    // NOTE: This is a hack specifically for using
    //       telnet to provide input via RTT.
#if defined(DEBUG_INPUT_SEGGER_RTT)   
    while (last_input_source == INPUT_SOURCE_SEGGER_RTT) {
        int c = SEGGER_RTT_GetKey();
        if ((c < 0) || (c == 0x0A)) {
            last_input_source = INPUT_SOURCE_UNDEFINED;
            break;
        }
    }
#endif // DEBUG_INPUT_SEGGER_RTT   
#if defined(DEBUG_INPUT_TINYUSB_CDC_ITF) 
    if ((last_input_source == INPUT_SOURCE_TINYUSB_CDC) && (tud_cdc_n_connected(DEBUG_INPUT_TINYUSB_CDC_ITF))) {
        last_input_source = INPUT_SOURCE_UNDEFINED;
        uint32_t bytes_available = tud_cdc_n_available(DEBUG_INPUT_TINYUSB_CDC_ITF);
        for (uint_fast32_t i = 0; i < bytes_available; ++i) {
            uint8_t c;
            tud_cdc_n_read(DEBUG_INPUT_TINYUSB_CDC_ITF, &c, 1);
            if (c == 0x0A) {
                break;
            }
        }
    }
#endif // DEBUG_INPUT_TINYUSB_CDC_ITF
    return;
}
int my_debug_wait_for_key_impl(void) {

    // waits for a key from any of the input sources.
    // prioritizes the last-used input source.
    // if no input is available, it will wait for a char
    // from any of the enabled input sources.
    bool got_input = false;
    int c = -1;

    // first get input from the last-used source
#if defined(DEBUG_INPUT_SEGGER_RTT)
    if (last_input_source == INPUT_SOURCE_SEGGER_RTT) {
        c = SEGGER_RTT_GetKey();
    }
#endif // DEBUG_INPUT_SEGGER_RTT
#if defined(DEBUG_INPUT_TINYUSB_CDC_ITF) 
    if (last_input_source == INPUT_SOURCE_TINYUSB_CDC) {
        if (tud_cdc_n_connected(DEBUG_INPUT_TINYUSB_CDC_ITF)) {
            c = tud_cdc_n_read_char(DEBUG_INPUT_TINYUSB_CDC_ITF);
        }
    }
#endif // DEBUG_INPUT_TINYUSB_CDC_ITF

    while (c == -1) {
#if defined(DEBUG_INPUT_SEGGER_RTT)
        if (c < 0) {
            c = SEGGER_RTT_GetKey();
            if (c >= 0) {
                last_input_source = INPUT_SOURCE_SEGGER_RTT;
            }
        }
#endif // DEBUG_INPUT_SEGGER_RTT
#if defined(DEBUG_INPUT_TINYUSB_CDC_ITF) 
        if (c < 0) {
            if (tud_cdc_n_connected(DEBUG_INPUT_TINYUSB_CDC_ITF)) {
                c = tud_cdc_n_read_char(DEBUG_INPUT_TINYUSB_CDC_ITF);
                if (c >= 0) {
                    last_input_source = INPUT_SOURCE_TINYUSB_CDC;
                }
            }
        }
#endif // DEBUG_INPUT_TINYUSB_CDC_ITF
    }
    return c;
}
int my_debug_wait_for_key_toupper_impl(void) {
    int c = my_debug_wait_for_key_impl();
    if ((c >= 'a') && (c <= 'z')) {
        c -= ('a' - 'A');
    }
    return c;
}
void my_debug_initialize_impl(bool prompt_for_mode) {

    char r = 0;
#if defined(DEBUG_INPUT_SEGGER_RTT) || defined(DEBUG_OUTPUT_SEGGER_RTT)
    SEGGER_RTT_Init();
    if (prompt_for_mode) {
        MY_PRINTF("SEGGER Real-Time-Terminal Configuration\r\n");
        MY_PRINTF("Press <1> to continue in blocking mode (Application waits if necessary, no data lost)\r\n");
        MY_PRINTF("Press <2> to continue in non-blocking mode (Application does not wait, data lost if fifo full)\r\n");
        do {
            r = MY_DEBUG_WAIT_FOR_KEY_TOUPPER();
            MY_DEBUG_DISCARD_REMAINING_LINE_INPUT();
        } while ((r != '1') && (r != '2'));

        if (r == '1') {
            MY_PRINTF("\r\nSelected <1>. Configuring RTT and starting...\r\n");
            SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
        } else {
            MY_PRINTF("\r\nSelected <2>. Configuring RTT and starting...\r\n");
            SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
        }
    }
#endif // DEBUG_INPUT_SEGGER_RTT

    return;
}


