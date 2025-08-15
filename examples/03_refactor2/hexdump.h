#pragma once

#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>

/* Callback: called once per line with full formatted string (start of line to end of line + \n)
             if callback needs access to string after callback returns,
             it must copy the string elsewhere.

*/
typedef void (*output_emitter_t)(void *emitter_context, const char * null_terminated_string);


void hexdump_emitter(const void * buffer, size_t buffer_size, size_t address_of_first_byte, output_emitter_t emitter, void* emitter_context);
void hexdump_header_emitter(output_emitter_t emitter, void* emitter_context);


// emits to fputs(..., stdout)
void hexdump(const void *buffer, size_t buffer_size, size_t address_of_first_byte);
void hexdump_header();


