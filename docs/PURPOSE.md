
# SaferOTP library for RP2350

## Purpose

The one-time programmable fuses on the Raspberry Pi RP2350 chip
provides a great deal of flexibility.  The design has some nice
features, such as built-in multiple ways to store redundant data.

## Problems

These can allow additional errors to sneak through.

* ECC algorithm in the datasheet was underspecified.
* Bootrom does not normally report errors when reading ECC data.
  * When using guarded reads, a bus fault is reported for ECC errors;
    While important for security-critical boot, to not normally
    report ECC decoding errors is a poor design choice.  Moreover,
    there is no well-defined method for handling bus faults caused
    by a library's code.
  * Bootrom does not validate that decoded ECC data (if it detected
    correctable errors) actually re-encodes to the expected raw data.
    This lets 3-bit and 5-bit errors slip through(!).
* Memory-mapped OTP regions normally fail to provide any
  error reporting for invalidly-encoded / corrupt data.
  * Same problem as above ... the bootrom appears to be calling
    into the Synopsys IP block, rather than implementing the ECC
    checks itself.
  * The memory-mapped regions are thus also likely using the same
    undocumented Synopsys IP block, and has the same problems as
    calling into the bootrom functions.

Development for mere mortals can be expensive, as any
mistake may make the board unusable (one-time programmable).
Alternatively, development becomes very slow work, as each
instruction must be carefully tested.

Having a set of well-tested higher-level APIs can greatly
reduce this burden.

## Complexity

* There are many ways data could be encoded:
  * `RAW` ... 24 bits per row, without any redundancy or ECC
  * `RBIT3` ... Storing the same 24 bits of data on three rows;
    Reads use per-bit majority voting to determine set bits
    (2 of 3 majority voting)
  * `RBIT8` ... Storing the same 24 bits of data on eight rows;
    Reads use per-bit majority voting to determine set bits
    (3 of 8 majority voting)
  * `BYTE3` ... Storing a the same 8 bits in a row three times;
    Reads use per-bit majority voting to determine set bits
    (2 of 3 majority voting)
  * `ECC` ... Storing 16 bits of usable data in a row, with
    the ability to correct any single-bit error, and detect
    any two-bit error via six ECC bits.  The final two bits
    (BRBP) allow a sector with any single bit flipped to still
    be usable to store an arbitrary value by indicating that
    all the 22 remaining bits should be inverted prior to
    performing error correction/detection.

Unfortunately, not only is it non-trivial to use for common tasks,
the Error Correction that the BIOS applies does not always do
what is expected.  For example, when an OTP row has ECC encoded
data, and multiple bits are flipped (e.g., forcing errors in the
encoded data), the BIOS may improperly return incorrect data.

In addition, when reading an OTP row with ECC encoded data, the
API appears to require reading **_TWO_** rows at a time.  The
datasheet says it will return 0xFFFFFFFF on a failure.
Since the API reads two rows at a time, it cannot indicate which
of the two rows had correct data (if any).  However, it appears
that the bootrom does NOT even report ECC errors?

The only way that **_might_** (untested) get notified of ECC errors
is by using "guarded" reads.  However, using guarded reads will
result in a hard fault ... requiring complex error handling
code, if the caller expects to simply report the error and
continue execution.

As for the bit-by-bit voting ... That's template code that is
not glamorous, and requires great care to implement correctly.
Better to have that occur once in a single library, than to have
many projects each implement and debug their own version.

Without simple, tested helper APIs, many projects may choose to
ignore the edge cases, or read only one copy ... effectively
losing all redundancy.


## API

The API uses source data byte counts for all encodings.
In other words, the byte count reflects the size of the buffer
that the API will read from / write to.

This allows simplified use of the API, as the caller may
use `sizeof(DATA_STRUCTURE)` when writing that data structure.

### Format specifics

* `RAW` -- Caller provides a `uint32_t` for each OTP row.
  Only the least significant 24 bits of each `uint32_t` will
  contain data.  This reflects the API defined by the RP2350 hardware.
* `RBIT3` and `RBIT8` -- Caller provides a `uint32_t` for each
  set of three (or eight, respectively) rows that store the data.
  * For reads, the bit-by-bit majority voting is applied prior
    to returning the data.
* `BYTE3` -- Caller provides one `uint8_t` for each row.
  * For reads, the bit-by-bit majority voting is applied prior
    to returning the data.
* `ECC` -- Caller provides one `uint16_t` for each row.
  * Validly encoded ECC data is returned.
  * Invalid data returns an error result.

### Error handling / reporting details

#### `ECC` data

The OTP is always read as a raw 24-bit value, which is then manually
decoded into a potential result.  The potential result is then
re-encoded using the ECC algorithm into a re-encoded result.
(BRBP may be applied to the re-encoded result to match the raw data).

Excluding the BRBP bits, it is permissible for the re-encoded result
and the raw data to differ by at most one bit.  Otherwise, the data
is considered to not be validly encoded ECC data, and an error is
reported.  This avoids many false-positive decodings where three or
five bits were flipped in the raw data.

#### `RBIT3` and `RBIT8` data

Both of these use bit-oriented voting, to determine if a bit
should be set to `1`.  `RBIT3` requires two votes from the the
bits stored in three rows, while `RBIT8` requires three votes from
the bits stored in eight rows.

Currently, the behavior for v1.0 of the library is intended to be:
* Error conditions that cannot modify the resulting data are ignored / hidden.
* Error conditions that have the potential to affect the resulting data are reported.

The error conditions here refer to failures to read or write an OTP row.

A future version of the library may allow for a behavior which simply
**_IGNORES_** OTP rows that cannot be read, at least for `RBIT8`.
The alternative behavior would have no effect on `RBIT3` nor `BYTE3`
data.

* Generically, reads occur as follows:
  * Keep count of how many rows failed to be read.
  * For OTP rows successfully read, each set bit adds a vote for its corresponding bit.
  * After reading all rows, determine final bit value for each bit:
    * If (votes >= `REQUIRED_BITS`) then set bit to 1
    * else if (read failures >= `REQUIRED_BITS`) then ERROR CONDITION
    * else if (votes >= `REQUIRED_BITS` - read failures) then ERROR CONDITION
    * else set bit to 0

* Generically, writes occur as follows:
  * Determine if impossible to safely write the data by reading the old data, and
    if so, exit before writing any data.
  * Read/Modify/Write the requested bits into each rows (ignoring failures for now)
  * Verify the data read back (using the `RBIT3` / `RBIT8` function) matches the requested data

* How to determine it's impossible to write the requested data:
  * Read the voted-upon value using existing API
    * Fails on various error conditions that should also prevent writing
    * Also fail if any bits in result are `1`, but new value has them as `0`
      This prevents writing rows when the requested value could not be stored.
    * Write the new value (read/modify/write) to each OTP row
    * Read back the new voted-upon value using existing API
    * Fail if the voted-upon value does not match the requested value.
    * Else, success!

#### `BYTE3` data

`BYTE3` uses bit-oriented voting, similar to `RBIT3`.  However, by only storing one byte
in each OTP row, the error conditions are much simpler to handle (the OTP row is either
readable or not ... whereas `RBIT3` and `RBIT8` have to consider some votes being readable
and some votes being unreadable).

Thus, if the OTP row is unreadable, it reports an error condition.
Otherwise, the bit-by-bit voting is applied to retrieve a single byte for each OTP row.

## Stretch Goals

* Support for OTP row ranges outside the user data area.

* Virtualized OTP ... 
  * At any time, switch from interacting with the real OTP
    to interacting with a virtual copy.
  * By default, caches existing values from OTP.
  * Option to load from any other source (e.g., flash, ROM,
    config file, ...) a program desires.
  * Option to persist the current (real or) virtualized OTP
    to any other source (e.g., flash, config file, ...).

* Enforcing permissions for access to OTP registers.
  * ***Excluding*** OTP access keys (see below)
  * Unique permissions support for Secure-mode vs. Non-secure-mode
     * Initial implementation presumes all access is from secure mode
  * Application of OTP permissions in PAGEn_LOCK0 and PAGEn_LOCK1 OTP rows
  * Hard-coded write restriction for PAGE0 (per datasheet)
  * Reading soft-lock registers, at least at initialization
    * Detecting other writes would require use of the memory
      protection features of the RP2350 (to virtualize access
      to the soft-lock registers).

* By default, failing writes to special OTP rows with unsupported functionality
  * e.g., OTP access keys, bootloader keys, encryption keys, etc.

* OTP Directory Entries
  * Dynamically locate data stored in OTP
  * Add new entries to the directory
  * Increase yield for boards shipped with imperfect OTP ...
    fewer binned compared to using fixed rows
  * Directory entry type specifies how the data is encoded
    into the OTP rows (RAW, BYTE3, RBIT3, RBIT8, ECC, ...)



## Non-Goals

* Emulation of OTP Access Keys
  * Emulation of OTP access keys would require use of the memory
    protection features of the RP2350 (to virtualize access
    to the OTP Access Key registers).
  * OTP Access Key rows are currently treated the same
    as any other row.
* Having virtualized OTP have any effect on bootloader,
  boot encryption, etc.
* Other interactions with the CPU / hardware.
  * e.g., don't expect debug access to be locked out
    or require a key specified only in virtualized OTP.

## Debugging

This is a static library, and so gets embedded into other projects.
However, it has rich debug outputs through macros that can be
redefined as appropriate for your system... See:
* `saferotp_lib/saferotp_debug_stub.h`
* `saferotp_lib/saferotp_debug_stub.c`


This has been tested with input and output sent via Segger's RTT,
sent via TinyUSB serial port, and likely supports other debug output
modes, by simply defining a few macros at the head of the file.

## WORK IN PROGRESS

This library doesn't even have a version number yet.
However, given how many edge cases were uncovered during testing
of the RP2350 OTP implementation, it seemed this might be useful
to many other folks working with the RP2350 ... even if not
feature complete yet.




