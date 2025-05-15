
# SaferOTP library for RP2350

## Purpose

Improve the error reporting and error handling options for using
the one-time programmable fuses on the Raspberry Pi RP2350 chip.

## Genesis

When testing how the OTP API worked in practice, it was discovered
that, even when data was encoded with ECC, the bootrom APIs would
silently report corrupted data, rather than reporting an error when
reading ECC data with multiple bits flipped.

This lead down a rabbit hole of understanding the limitations of
the Synopsys IP block that Raspberry Pi used for the OTP fuses,
and how to provide an API that avoids suprising behaviors (such
as the above successful reads of detectably corrupt ECC data).)

Reads of corrupted ECC-encoded OTP data silently succeeds
***by design***.  You see, the Synopsys IP block does not
indicate any error unless using so-called "Guarded Reads".
When using guarded reads, the hardware will report a bus fault.

A bus fault is a hard fault, and thus requires a complex
error handler to be written to detect the cause was an OTP
ECC decoding error, and then adjust the CPU to resume
execution ... WAY outside the scope of a hobbyist project.

What was needed was a way to read `ECC`-encoded OTP data,
where the API would simply report `false` if the data could
not be read, or `true` if the data was read successfully.

## Method of Operation

### `ECC`-encoded data

Because the bootrom could not report ECC errors without
raising a bus fault, this library could not rely on the
hardware ECC implementation to decode the raw data.

Instead, this library performs a four-step process to
read from each ECC-encoded OTP row:

1. Read the OTP row as a `RAW` (24-bit) value
2. Decode the `RAW` value into a ***potential*** result
3. Re-encode the ***potential*** result into ***re-encoded*** raw data
4. Verify that the ***re-encoded*** raw data is equivalent to the raw data read in the first step.

If any of these steps fail, the library will report an error.

#### Step 1 - Read `RAW` value

This simply translates to the existing API for reading the raw 24-bit value.

#### Step 2 - Decode the `RAW` value into a ***potential*** result

As per the datasheet, the first thing checked in the `RAW` value are
the `BRBP` bits.  These are the two most significant of the 24-bit
`RAW` value.  If both set (`0b11`), it indicates that the remaining
22 bits of data in the OTP row should be inverted before ***any***
other interpretation of their value.

Of the remaining 22 bits, the ECC decoding algorithm is applied.
If the most significant ECC bit is 0, but any of the other five ECC bits
are non-zero, then there was an even number of bits flipped ...
a detectable (but not correctable) error.
If the most significant ECC bit is 1, then the remaining five ECC bits
define which of the 16 bits of user data needs to be flipped to obtain
the corrected user data.

#### Step 3 - Re-encode the ***potential*** result into a ***re-encoded*** 22-bit raw value

Yes, encoding can only provide 22-bits of data (16-bits of user data + 5 bits ECC + 1 bit even polarity).

#### Step 4 - Verify that the ***re-encoded*** raw data is equivalent to the raw data read in the first step.

As the re-encoded value does not include `BRBP` bits, equivalent here
is defined to consider the `RAW` value after it was (possibly)
inverted because of its own `BRBP` bits.

The re-encoded value is then XOR'd with the (`BRBP` adjusted) `RAW`
value.  Counting the number of the low 22 bits are set in the XOR'd
result indicates how many bits would need to be flipped to get from
the re-encoded value to what is stored in the OTP row.

Since the ECC algorithm can only correct a single bit error, if this
re-encoding indicates that zero or one bits were flipped, then the
***potential*** result becomes a confirmed (final) result.

Otherwise, either the data was not encoded as ECC data, or too many
bits were flipped.  In either case, this is reported as an ERROR.


### Error handling / reporting details

Excluding the BRBP bits, it is permissible for the re-encoded result
and the raw data to differ by ***at most*** one bit.  Otherwise, the data
is considered to not be validly encoded ECC data, and an error is
reported.  This avoids many false-positive decodings where
three or five bits were flipped in the raw data.

#### `RBIT3` and `RBIT8` data

Both of these use bit-oriented voting, to determine if a bit
should be set to `1`.  `RBIT3` requires two votes from the the
bits stored in three rows, while `RBIT8` requires three votes from
the bits stored in eight rows.

Currently, the behavior for v1.0 of the library is intended to be:
* Unreadable OTP rows that cannot modify the resulting data are ignored / hidden.
* Unreadable OTP rows that have the potential to affect the resulting data result in an error.

Generically, reads occur as follows:
* Keep count of how many rows failed to be read.
* For OTP rows successfully read, each set bit adds a vote for its corresponding bit.
* After reading all rows, determine final bit value for each bit:
  * If (votes >= `REQUIRED_BITS`) then set bit to 1
  * else if (read failures >= `REQUIRED_BITS`) then ERROR CONDITION
  * else if (votes >= `REQUIRED_BITS` - read failures) then ERROR CONDITION
  * else set bit to 0

Generically, writes occur as follows:
* Determine if impossible to safely write the data by reading the old data
  * if so, exit before writing any data.
* Read/Modify/Write the requested bits into each rows (ignoring failures for now)
* Verify the data read back (using the `RBIT3` / `RBIT8` read function)
  matches the request new data value

How to determine it's impossible to write the requested data:
* Read the voted-upon value using existing API
  * Fails on various error conditions that should also prevent writing
* Compare the voted-upon value to the requested new value:
  * If any bits are zero in the requested new value, but report as one
    when voted upon, then it's impossible to reduce the number of votes,
    so this results in an ERROR before any value is written.

## Stretch Goals

* OTP Directory Entries
  * Dynamically locate data stored in OTP
  * Add new entries to the directory
  * Increase yield for boards shipped with imperfect OTP ...
    fewer binned compared to using fixed rows
  * Directory entry type specifies how the data is encoded
    into the OTP rows (RAW, BYTE3, RBIT3, RBIT8, ECC, ...)

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
However, it uses debug macros that allow integration with existing
debug output options.  See:
* `saferotp_lib/saferotp_debug_stub.h`
* `saferotp_lib/saferotp_debug_stub.c`


This has been tested with input and output sent via Segger's RTT,
with `printf`-style output sent via TinyUSB serial port, and it
likely supports other debug output modes ... by simply defining
those few macros (and perhaps a few simple functions).

## WORK IN PROGRESS

This library doesn't even have a version number yet.
However, given how many edge cases were uncovered during testing
of the RP2350 OTP implementation, it seemed this might be useful
to many other folks working with the RP2350 ... even if not
feature complete yet.

