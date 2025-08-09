
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

<details><summary>Background</summary><P/>

The one-time programmable (OTP) fuses on the Raspberry Pi RP2350 chip
provide a great deal of flexibility.  However, the existing
hardware limits error reporting to raising bus faults when asking for
the reads to be "guarded" reads.  This same restriction applies to
the memory-mapped OTP areas, which perform unguarded reads of the OTP.

Using "guarded" reads causes bus faults when an ECC error is
detected, which is useful for improving security of bootloaders or
other cases where a bus fault could be handled or crashing is
an appropriate error state.

The problem is that ECC errors, when using normal (unguarded) reads
will simply return corrupted data ... even when that ECC error could be
detected.

This makes it difficult to use the ECC capabilities without significant
coding effort (handling bus faults, adjustments to faulting thread context,
and restarting execution of the faulting code ... which might not
even be realistic as it requires global changes or OS-level support).

Wouldn't it be more useful if the bootrom APIs returned an error code
for detected ECC errors, rather than silently returning corrupt data?

This library provides that set of simpler APIs, and provides a simple,
consistent API for both reading and writing data in all supported
formats.  The API reports errors as return values, and avoids raising
bus faults for ECC errors, by only reading the OTP rows in `RAW` form,
and then applying the ECC and other checks in software.

This core API for reading and writing to the OTP seems stable.

The library is expanding to provide additional functionality:
* A compile-time option to enable "virtualized" OTP when using this library's
  APIs, helping reduce the number of boards that need to be thrown away when
  testing (due to the one-time programmable nature) feature that modify the
  OTP data.  This has been successfully prototyped, and may be sufficient
  for development purposes.
* A set of APIs to allow a project to define its own set of "directory entries",
  which record where the corresponding data is stored in the OTP.   Recording
  this simple indirection data allows otherwise unusable boards (e.g., due to
  errors in the OTP from the factory, or partially failed writes, or latent
  OTP errors) to be used, reducing e-waste.  These APIs are currently **_very_**
  unstable, have not been fully tested, and are thus **_particularly_** more
  likely to have bugs or fatal flaws.

</details>

## Usage

### CMake based RP2350 projects

As this is the default for SDK projects, I'll try to list the steps.

A later version will likely include a "before" and "after" sample
CMake project.

Disclaimer: I am not a CMake expert, and thus there may be "better"
ways to do this. Pull requests (with explanations) are welcome.

<details><summary>Current steps</summary><P/>


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

### Brute-force

Copy the library files into your project however you like.
Ensure all the `.c` files are compiled and linked into your
project.

### Why not just use the existing APIs?

See additional details in `docs/PURPOSE.md` and `docs/USAGE.md`.

## WORK IN PROGRESS

This library doesn't even have a version number yet.

That said, given how many edge cases were uncovered during testing
of the RP2350 OTP implementation, it seemed this might be useful
to many other folks working with the RP2350 ... even if not
feature complete yet ... especially for detecting ECC errors in
data read from the OTP using simple return-code based APIs.  
