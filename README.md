
# SaferOTP library for RP2350

## Purpose

Three main purposes:
1. Provide a simple, consistent API for reading and writing ECC
   protected data to the OTP rows.
2. Provide a simple, consistent API for reading and writing
   data in the other supported formats (`BYTE3X`, `RBIT3`, and `RBIT8`).
3. Provide a simple API for using virtualized OTP (to reduce the
   costs of testing and developing anything that modifies OTP data).

## Background

The one-time programmable (OTP) fuses on the Raspberry Pi RP2350 chip
provide a great deal of flexibility.  However, the existing
hardware limits error reporting to raising bus faults when asking for
the reads to be "guarded" reads.  This same restriction applies to
the memory-mapped OTP areas.

The problem is that using "guarded" reads causes bus faults on detected
ECC errors, but using normal reads simply returns corrupted data.

This makes it difficult to use the ECC capabilities without significant
coding effort (handling bus faults, adjustments to faulting thread context,
and restarting execution of the faulting code ... which might not
even be possible).  It would have been much nicer if the bootrom APIs
simply returned an error code for detected ECC errors, rather than
silently returning corrupt data.

This library provides that set of simpler APIs, and provides a simple,
consistent API for both reading and writing data in all supportedreporting errors
as return values, rather than raising bus faults.

The library then grew to provide similar APIs for the other encoding formats
used by the bootrom, and to enable "virtualize" OTP when using this library's
APIs, to reduce the number of boards that need to be thrown away when testing
and developing features that modify the OTP data.

## Usage

### Brute-force

Copy the library files into your project however you like.
Ensure all the `.c` files are compiled and linked into your
project.

### CMake based RP2350 projects

As this is the default for SDK projects, I'll try to list the steps.

Disclaimer: I am not a CMake expert, and thus there may be "better"
ways to do this.  No support is provided.

* This presumes your project root directory contains a `CMakeLists.txt`
* Copy the library's entire tree / directory structure into a subdirectory.
  * e.g., I'll presume you added it to a directory called `saferotp`
  * This directory should be in the same level as your main `CMakeLists.txt`
* In this library's directory, modify the following files:
  * `saferotp_lib/saferotp_debug_stub.h`
    * Define the `PRINT_` macros listed in the comments at the top of the file to use your preferred debug output method.
    * Alternatively, define the macros as nothing. e.g., `#define PRINT_ERROR(...)`
  * `CMakeLists.txt`
    * Modify the few lines grouped around the `HACK` text, to reference any required diretory for your debug macro support
    * e.g., remove those few lines if the debug macros are defined to nothing.
* In your project's main `CMakeLists.txt`:
  * Ensure CMake parses the library's configuration and builds, etc: `add_subdirectory(saferotp)`.
  * Ensure your binary links to the library: `target_link_libraries(saferotp_lib)`

### Why not just use the existing APIs?

See additional details in `docs/PURPOSE.md` and `docs/USAGE.md`.

## WORK IN PROGRESS

This library doesn't even have a version number yet.
However, given how many edge cases were uncovered during testing
of the RP2350 OTP implementation, it seemed this might be useful
to many other folks working with the RP2350 ... even if not
feature complete yet.




