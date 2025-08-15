#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

// THREE SECTIONS OF INTEREST TO CLIENTS OF THIS HEADER FILE:
// 1. The list of debug categories, for enabling debug output on more granular basis.
// 2. The debug macros themselves.
// 3. The ability to set a default category for a file (and thus use simpler macros).
// 4. Options to specify whether to prefix messages with file/line/function.
//
// Search for "CLIENT_INTEREST" to find notes for each below.

// C11 doesn't have typesafe enums, but the following tomfoolery can partially
// emulates typesafe enums, with a minor one-time coding cost when adding
// a new enum value.
#if 1 // Some type-safety even in C11

    // define the enumeration here, but not as a type that would naturally be used in code
    // outside the debug header.
    // NOTE: intentionally non-consecutive values.
    typedef enum { 
        _MY_E_DEBUG_LEVEL_FATAL   = 0x00u,
        _MY_E_DEBUG_LEVEL_ERROR   = 0x20u,
        _MY_E_DEBUG_LEVEL_WARNING = 0x60u,
        _MY_E_DEBUG_LEVEL_INFO    = 0xA0u,
        _MY_E_DEBUG_LEVEL_VERBOSE = 0xC0u,
        _MY_E_DEBUG_LEVEL_DEBUG   = 0xE0u,
        _MY_E_DEBUG_LEVEL_NEVER   = 0xFFu,
    } _my_debug_level_enum_t;

    // This enum is used two ways:
    // 1. as an index into an array of debug levels
    // 2. to set a single bit within a `uint32_t`, which is usable to enable/disable the category's output.
    //
    // Any value greater than 31u will cause the bitmask to be zero, and that category's output will never appear.
    //
    // NOTE: Current maximum value for this enum is *** 31u ***.
    //       Expanding categories is possible, by adjusting `rtt_debug_should_print()`
    //       below to use an array of debug level uint32_t instead of a single uint32_t.
    typedef enum _my_debug_category_enum_t {
        _MY_E_DEBUG_CAT_CATCHALL         =  0u, // [ 0], (((uint32_t)1u) << ( 0u % 32u)), // for messages that are not (yet) categorized
        _MY_E_DEBUG_CAT_01               =  1u, // [ 1], (((uint32_t)1u) << ( 1u % 32u)), // one category of messages / functionality
        _MY_E_DEBUG_CAT_02               =  2u, // [ 2], (((uint32_t)1u) << ( 2u % 32u)), // another category...
        _MY_E_DEBUG_CAT_03               =  3u, // [ 3], (((uint32_t)1u) << ( 3u % 32u)), // a third category...
        _MY_E_DEBUG_CAT_04               =  4u, // [ 4], (((uint32_t)1u) << ( 4u % 32u)), // a fourth category...
        _MY_E_DEBUG_CAT_05               =  5u, // [ 5], (((uint32_t)1u) << ( 5u % 32u)), // ...
        _MY_E_DEBUG_CAT_06               =  6u, // [ 6], (((uint32_t)1u) << ( 6u % 32u)), // ...
        _MY_E_DEBUG_CAT_07               =  7u, // [ 7], (((uint32_t)1u) << ( 7u % 32u)), // ...
        _MY_E_DEBUG_CAT_08               =  8u, // [ 8], (((uint32_t)1u) << ( 8u % 32u)), // ...
        _MY_E_DEBUG_CAT_09               =  9u, // [ 9], (((uint32_t)1u) << ( 9u % 32u)), // ...
        _MY_E_DEBUG_CAT_10               = 10u, // [10], (((uint32_t)1u) << (10u % 32u)), // ...
        _MY_E_DEBUG_CAT_11               = 11u, // [11], (((uint32_t)1u) << (11u % 32u)), // ...
        _MY_E_DEBUG_CAT_12               = 12u, // [12], (((uint32_t)1u) << (12u % 32u)), // ...
        _MY_E_DEBUG_CAT_13               = 13u, // [13], (((uint32_t)1u) << (13u % 32u)), // ...
        _MY_E_DEBUG_CAT_14               = 14u, // [14], (((uint32_t)1u) << (14u % 32u)), // ...
        _MY_E_DEBUG_CAT_15               = 15u, // [15], (((uint32_t)1u) << (15u % 32u)), // ...
        _MY_E_DEBUG_CAT_16               = 16u, // [16], (((uint32_t)1u) << (16u % 32u)), // ...
        _MY_E_DEBUG_CAT_17               = 17u, // [17], (((uint32_t)1u) << (17u % 32u)), // ...
        _MY_E_DEBUG_CAT_18               = 18u, // [18], (((uint32_t)1u) << (18u % 32u)), // ...
        _MY_E_DEBUG_CAT_19               = 19u, // [19], (((uint32_t)1u) << (19u % 32u)), // ...
        _MY_E_DEBUG_CAT_20               = 20u, // [20], (((uint32_t)1u) << (20u % 32u)), // ...
        _MY_E_DEBUG_CAT_21               = 21u, // [21], (((uint32_t)1u) << (21u % 32u)), // ...
        _MY_E_DEBUG_CAT_22               = 22u, // [22], (((uint32_t)1u) << (22u % 32u)), // ...
        _MY_E_DEBUG_CAT_23               = 23u, // [23], (((uint32_t)1u) << (23u % 32u)), // ...
        _MY_E_DEBUG_CAT_24               = 24u, // [24], (((uint32_t)1u) << (24u % 32u)), // ...
        _MY_E_DEBUG_CAT_25               = 25u, // [25], (((uint32_t)1u) << (25u % 32u)), // ...
        _MY_E_DEBUG_CAT_26               = 26u, // [26], (((uint32_t)1u) << (26u % 32u)), // ...
        _MY_E_DEBUG_CAT_27               = 27u, // [27], (((uint32_t)1u) << (27u % 32u)), // ...
        _MY_E_DEBUG_CAT_28               = 28u, // [28], (((uint32_t)1u) << (28u % 32u)), // ...
        _MY_E_DEBUG_CAT_29               = 29u, // [29], (((uint32_t)1u) << (29u % 32u)), // ...
        _MY_E_DEBUG_CAT_30               = 30u, // [30], (((uint32_t)1u) << (30u % 32u)), // ...
        // NOTE: the last (largest value) category must be the TEMP category.  Depended upon in multiple places in below code.
        _MY_E_DEBUG_CAT_TEMP             = 31u, // [31], (((uint32_t)1u) << (31u % 32u)), // use for temporary debug messages
    } _my_debug_category_enum_t;

    // next, define a structure that wraps the enumeration.
    // the structure's member will have the enum's type.
    typedef struct _my_debug_level_t    { _my_debug_level_enum_t    level;    } my_debug_level_t;
    typedef struct _my_debug_category_t { _my_debug_category_enum_t category; } my_debug_category_t;

    #define MY_DEBUG_CAT_CATCHALL  ((my_debug_category_t){ _MY_E_DEBUG_CAT_CATCHALL }) // for messages that are not (yet) categorized
    #define MY_DEBUG_CAT_01        ((my_debug_category_t){ _MY_E_DEBUG_CAT_01       }) // one category of messages / functionality
    #define MY_DEBUG_CAT_02        ((my_debug_category_t){ _MY_E_DEBUG_CAT_02       }) // another category...
    #define MY_DEBUG_CAT_03        ((my_debug_category_t){ _MY_E_DEBUG_CAT_03       }) // third category...
    #define MY_DEBUG_CAT_04        ((my_debug_category_t){ _MY_E_DEBUG_CAT_04       }) // fourth category...
    #define MY_DEBUG_CAT_05        ((my_debug_category_t){ _MY_E_DEBUG_CAT_05       }) // ...
    #define MY_DEBUG_CAT_06        ((my_debug_category_t){ _MY_E_DEBUG_CAT_06       }) // ...
    #define MY_DEBUG_CAT_07        ((my_debug_category_t){ _MY_E_DEBUG_CAT_07       }) // ...
    #define MY_DEBUG_CAT_08        ((my_debug_category_t){ _MY_E_DEBUG_CAT_08       }) // ...
    #define MY_DEBUG_CAT_09        ((my_debug_category_t){ _MY_E_DEBUG_CAT_09       }) // ...
    #define MY_DEBUG_CAT_10        ((my_debug_category_t){ _MY_E_DEBUG_CAT_10       }) // ...
    #define MY_DEBUG_CAT_11        ((my_debug_category_t){ _MY_E_DEBUG_CAT_11       }) // ...
    #define MY_DEBUG_CAT_12        ((my_debug_category_t){ _MY_E_DEBUG_CAT_12       }) // ...
    #define MY_DEBUG_CAT_13        ((my_debug_category_t){ _MY_E_DEBUG_CAT_13       }) // ...
    #define MY_DEBUG_CAT_14        ((my_debug_category_t){ _MY_E_DEBUG_CAT_14       }) // ...
    #define MY_DEBUG_CAT_15        ((my_debug_category_t){ _MY_E_DEBUG_CAT_15       }) // ...
    #define MY_DEBUG_CAT_16        ((my_debug_category_t){ _MY_E_DEBUG_CAT_16       }) // ...
    #define MY_DEBUG_CAT_17        ((my_debug_category_t){ _MY_E_DEBUG_CAT_17       }) // ...
    #define MY_DEBUG_CAT_18        ((my_debug_category_t){ _MY_E_DEBUG_CAT_18       }) // ...
    #define MY_DEBUG_CAT_19        ((my_debug_category_t){ _MY_E_DEBUG_CAT_19       }) // ...
    #define MY_DEBUG_CAT_20        ((my_debug_category_t){ _MY_E_DEBUG_CAT_20       }) // ...
    #define MY_DEBUG_CAT_21        ((my_debug_category_t){ _MY_E_DEBUG_CAT_21       }) // ...
    #define MY_DEBUG_CAT_22        ((my_debug_category_t){ _MY_E_DEBUG_CAT_22       }) // ...
    #define MY_DEBUG_CAT_23        ((my_debug_category_t){ _MY_E_DEBUG_CAT_23       }) // ...
    #define MY_DEBUG_CAT_24        ((my_debug_category_t){ _MY_E_DEBUG_CAT_24       }) // ...
    #define MY_DEBUG_CAT_25        ((my_debug_category_t){ _MY_E_DEBUG_CAT_25       }) // ...
    #define MY_DEBUG_CAT_26        ((my_debug_category_t){ _MY_E_DEBUG_CAT_26       }) // ...
    #define MY_DEBUG_CAT_27        ((my_debug_category_t){ _MY_E_DEBUG_CAT_27       }) // ...
    #define MY_DEBUG_CAT_28        ((my_debug_category_t){ _MY_E_DEBUG_CAT_28       }) // ...
    #define MY_DEBUG_CAT_29        ((my_debug_category_t){ _MY_E_DEBUG_CAT_29       }) // ...
    #define MY_DEBUG_CAT_30        ((my_debug_category_t){ _MY_E_DEBUG_CAT_30       }) // ...
    #define MY_DEBUG_CAT_TEMP      ((my_debug_category_t){ _MY_E_DEBUG_CAT_TEMP     }) // use for temporary debug messages

#endif

// Define the user-facing constants using the wrapper structure and values.
// A function can then define a parameter using these types.
// 1. If passed an incorrect type (e.g., `my_debug_level_t` instead of
//    `my_debug_category_t`,  the compiler will emit an error
//    (incompatible argument type)
// 2. The generated code will have ZERO overhead vs. a straight enum

// CLIENT_INTEREST
// The following are the default debug levels passed to the debug macros.
// Lower values are more critical.
#define MY_DEBUG_LEVEL_FATAL          ((my_debug_level_t)   { _MY_E_DEBUG_LEVEL_FATAL          }) // unrecoerable errors that normally crash or require reset
#define MY_DEBUG_LEVEL_ERROR          ((my_debug_level_t)   { _MY_E_DEBUG_LEVEL_ERROR          }) // recoverable errors
#define MY_DEBUG_LEVEL_WARNING        ((my_debug_level_t)   { _MY_E_DEBUG_LEVEL_WARNING        }) // warnings, such as improper user input, rarely tested edge cases occurring, potential (unveritifer) issues
#define MY_DEBUG_LEVEL_INFO           ((my_debug_level_t)   { _MY_E_DEBUG_LEVEL_INFO           }) // informational messages
#define MY_DEBUG_LEVEL_VERBOSE        ((my_debug_level_t)   { _MY_E_DEBUG_LEVEL_VERBOSE        }) // verbose messages
#define MY_DEBUG_LEVEL_DEBUG          ((my_debug_level_t)   { _MY_E_DEBUG_LEVEL_DEBUG          }) // debug message ... may impact usability, such as function entry/exit
#define MY_DEBUG_LEVEL_NEVER          ((my_debug_level_t)   { _MY_E_DEBUG_LEVEL_NEVER          })

#define MY_DEBUG_CAT_DEFAULT          MY_DEBUG_CAT_CATCHALL
// #define MY_DEBUG_CAT_FOO           MY_DEBUG_CAT_01
// #define MY_DEBUG_CAT_BAR           MY_DEBUG_CAT_02
// #define MY_DEBUG_CAT_BAZ           MY_DEBUG_CAT_03

// Define your own categories as needed.
// MY_DEBUG_CAT_04 .. MY_DEBUG_CAT_30 are predefined



#if 1 // internal implementation details

// While these are implementation details, to allow for greatest optimization,
// these aspects must be in the header file.  Since it is expected that the
// debug level and category provided to the macro will be constant, the compiler
// should elide most of these inline checks.

#ifdef __cplusplus
extern "C" {
#endif

// Need one uint32_t for every 32 categories defined above.
// This is why MY_DEBUG_CAT_TEMP must be the last / largest value in the list.
extern my_debug_level_t _MY_DEBUG_LEVELS[_MY_E_DEBUG_CAT_TEMP+1u]; // up to 32 categories, each with a debug level

__attribute__((always_inline))
static inline my_debug_level_t _my_get_default_debug_level(void) {
    // The global (catch-all) category level applies to all categories.
    static_assert(_MY_E_DEBUG_CAT_CATCHALL == 0u, "Catch-all category must be index zero");

    return _MY_DEBUG_LEVELS[0u];
}
__attribute__((always_inline))
static inline my_debug_level_t _my_get_category_debug_level(my_debug_category_t category) {
    // The global (catch-all) category level applies to all categories.
    static_assert(_MY_E_DEBUG_CAT_CATCHALL == 0u, "Catch-all category must be index zero");
    if (category.category >= MY_DEBUG_CAT_TEMP.category) {
        return MY_DEBUG_LEVEL_NEVER;
    }
    return _MY_DEBUG_LEVELS[category.category];
}
__attribute__((always_inline))
static inline void _my_set_debug_category_level(my_debug_category_t category, my_debug_level_t level) {
    // The global (catch-all) category level applies to all categories.
    static_assert(_MY_E_DEBUG_CAT_CATCHALL == 0u, "Catch-all category must be index zero");
    if (category.category >= MY_DEBUG_CAT_TEMP.category) {
        return;
    }
    if (level.level > MY_DEBUG_LEVEL_NEVER.level) {
        return;
    }
    _MY_DEBUG_LEVELS[category.category] = level;
}

// Both attribute *AND* `static inline` are required to ensure inlining,
// which is necessary to minimize overhead when a debug print is disabled.
__attribute__((always_inline))
static inline bool _my_debug_should_print(my_debug_level_t level, my_debug_category_t category) {

    // CLIENT_INTEREST -- Logic for what gets printed when.
    // 1. FATAL messages are always printed.
    // 2. The global (catch-all) debug level applies to all categories,
    //    to make it easy to enable/disable debug messages system-wide.
    // 3. The category-specific debug level is then checked.
    // Else, the message is not printed.

    // ALWAYS print fatal messages, regardless of category or variable settings.
    if (level.level == (MY_DEBUG_LEVEL_FATAL).level) {
        return true;
    }
    // The global (catch-all) category level applies to all categories.
    if (level.level <= _my_get_default_debug_level().level) {
        return true;
    }
    // Is the category-specific level indicate to print?
    if (level.level <= _my_get_category_debug_level(category).level) {
        return true;
    }
    // no other tests, so ... 
    return false;
}

int my_debug_print_impl(
    my_debug_level_t level,
    my_debug_category_t category,
    const char * file,
    size_t line,
    const char * function,
    const char* format,
    ...
);
int  my_printf_impl(const char* format, ...);
void my_debug_discard_all_input_impl(void);
void my_debug_discard_remaining_line_input_impl(void);
int  my_debug_wait_for_key_impl(void);
int  my_debug_wait_for_key_toupper_impl(void);
void my_debug_initialize_impl(bool prompt_for_mode);

#ifdef __cplusplus
} // end of extern "C" block
#endif

#endif // internal implementation details



#define MY_PRINTF(...) \
    do { \
        my_printf_impl(__VA_ARGS__); \
    } while (0)

// This is the underlying debug macro logic, also used by the other debug macros.
// TODO: Add __FILE__, __LINE__, and __func__ parameters, and implementation
//       in debug_rtt.c.
#define MY_DEBUG_PRINT(_LEVEL, _CATEGORY, ...)           \
    do {                                                 \
        if (_my_debug_should_print(_LEVEL, _CATEGORY)) { \
            my_debug_print_impl(_LEVEL, _CATEGORY, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        }                                                \
    } while (0);

// USAGE:
// * Files can immediately use the PRINT_* macros.  This will use the `CATCHALL` category.
// * Files can also immediately use the MY_DEBUG_PRINT() macro directly, although it is
//   slightly more typing.
// * Where a file is exclusively (or primarily) a single category, the source file may
//   define the `MY_DEBUG_OVERRIDE_DEFAULT_CATEGORY` token prior to including headers.
//   For example, if `MY_DEBUG_CAT_BAR` is defined and is a good default for the file:
//
//   #define MY_DEBUG_OVERRIDE_DEFAULT_CATEGORY MY_DEBUG_CAT_BAR
//   #include "debug_rtt.h"
//
//   Doing so will change all use of the PRINT_* macros to use the BAR category for
//   that compilation unit (e.g., source C file).  The MY_DEBUG_PRINT() macro can still
//   be used to specify another category wherever that is needed.
//
#if defined(MY_DEBUG_OVERRIDE_DEFAULT_CATEGORY)
    #define MY_DEBUG_DEFAULT_CATEGORY MY_DEBUG_OVERRIDE_DEFAULT_CATEGORY
#else
    #define MY_DEBUG_DEFAULT_CATEGORY MY_DEBUG_CAT_CATCHALL
#endif

// Sometime, it's useful to disable all the debug macros in a build.
// Simply define the DISABLE_DEBUG_PRINT_MACROS token before including
// this header file, and they vanish.
#if defined(DISABLE_DEBUG_PRINT_MACROS)
    #define PRINT_FATAL(...)
    #define PRINT_ERROR(...)
    #define PRINT_WARNING(...)
    #define PRINT_INFO(...)
    #define PRINT_VERBOSE(...)
    #define PRINT_DEBUG(...)
#else
    #define PRINT_FATAL(...)   MY_DEBUG_PRINT(MY_DEBUG_LEVEL_FATAL,   MY_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
    #define PRINT_ERROR(...)   MY_DEBUG_PRINT(MY_DEBUG_LEVEL_ERROR,   MY_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
    #define PRINT_WARNING(...) MY_DEBUG_PRINT(MY_DEBUG_LEVEL_WARNING, MY_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
    #define PRINT_INFO(...)    MY_DEBUG_PRINT(MY_DEBUG_LEVEL_INFO,    MY_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
    #define PRINT_VERBOSE(...) MY_DEBUG_PRINT(MY_DEBUG_LEVEL_VERBOSE, MY_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
    #define PRINT_DEBUG(...)   MY_DEBUG_PRINT(MY_DEBUG_LEVEL_DEBUG,   MY_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
#endif

// Ensures an assertion is checked and file/line number are printed (even in NDEBUG compilation).
// If used hard_assert() directly, file/line number would not be shown.
#define MY_ASSERT(_COND) \
    do {                                                                  \
        if (!(_COND)) {                                                   \
            PRINT_FATAL("ASSERTION FAILED: %s:%d\n", __FILE__, __LINE__); \
            hard_assert(false);                                           \
        }                                                                 \
    } while (0)


// Input functions ... allows to switch from Segger to COM port to SWD to ... ???
#define MY_DEBUG_DISCARD_ALL_INPUT()                   my_debug_discard_all_input_impl()
#define MY_DEBUG_DISCARD_REMAINING_LINE_INPUT()        my_debug_discard_remaining_line_input_impl()
#define MY_DEBUG_WAIT_FOR_KEY()                        my_debug_wait_for_key_impl()
#define MY_DEBUG_WAIT_FOR_KEY_TOUPPER()                my_debug_wait_for_key_toupper_impl()
#define MY_DEBUG_INITIALIZE(X)                         my_debug_initialize_impl((X))
    