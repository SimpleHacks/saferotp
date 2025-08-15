# Genesis of the SaferOTP library

## Surprise!  Success doesn't mean correct!

While working on firmware for the [BusPirate6](https://buspirate.com),
I started work on using the OTP to personalize the device, using the
built-in support for custom USB VID/PID and for exposing custom information
via the bootloader file system.

The bootrom API for reading and writing to the OTP is all wrapped into a single API:

```C
int rom_func_otp_access(uint8_t *buf, uint32_t buf_len, otp_cmd_t cmd);
```

The last argument, `cmd` defines three things:
1. the row of the OTP to be accessed
2. if this is a read or write command
3. if the OTP row should stored RAW data, or data protected by ECC, parity, and BRBP

Based on this API returning an error code, it would not be unreasonable to believe that,
if the command indicates a read of an ECC-encoded OTP row, that an unrecoverable ECC
error would return an error from the ROM function.  In fact, this is what I believed
would be the case.

After getting some basic functionality working, I moved to testing
edge cases, including uncorrectable ECC errors in the underlying OTP data.
This resulted in some [startling
discoveries](https://forum.buspirate.com/t/otp-whitelabel-options-for-rp2350-boards/963/99?u=henrygab):

* When using the BOOTROM API to read ECC-encoded rows, detectable uncorrectable ECC errors are silently ignored, and return corrupted data with a successful return code of `BOOTROM_OK`.
* Reading from the memory-mapped section at `OTP_DATA_BASE` (`0x40130000`), detectable uncorrectable ECC errors are silently ignored, and corrupt data is placed into memory.

***Suprise!***  Yes, the `rom_func_otp_access()` API returns `BOOTROM_OK` when asked to read
an ECC-encoded OTP row, even when it has a detected, uncorrectable ECC error.
Interestingly, this is NOT just returning the least significant 16 bits;  Rather, BRBP is
being applied to the returned data, so the value is "closer" to what was written.
Still, where an ECC error is detectable and not correctable, the fact that the bootrom's
API returns `BOOTROM_OK` is not a reasonable result.

## Alternative ... Guarded Reads

The datasheet and first hacking challenge made clear that the RP2350 implemented a
special "Guarded Read" for OTP rows, as a way to harden the bootloader against
certain types of attacks.  

In fact, it's easy to use these guarded reads.  Simply read the data from the
memory-mapped section at `OTP_DATA_GUARDED_BASE` (`0x40138000`), and an uncorrectable
ECC error will cause a bus fault.

What is ***not*** easy is wrapping that read in a wrapper function, hooking up
a bus fault handler, and robustly converting the bus fault into an error return
when the bus fault occurs from that wrapper function because of an OTP ECC error.

If you're curious, ask Claude Sonet or GPT-5 (or newer) to create an example base-metal
RP2350 firmware using RPi SDK2.0+.  Maybe a prompt similar to the following:

> rp2350: How to do guarded read of a row of the OTP?
> Can you provide example of how to setup a robust fault handler, which detects if the fault was caused by accessing the guarded ECC memory-mapped region from a known OTP guarded read wrapper function (e.g., one you provide in the answer), and then robustly converts the bus fault from such an ECC error in that wrapper function into a return code?  Make this a library with simple API for initialization, including chaining to any existing installed bushandler so that it can be used on bare-metal firmware, embedOS, FreeRTOS, or other operating systems.
> Modify this so it supports reading multiple guarded ECC rows with a single begin_guard() / end_guard(). e.g.: ```c int saferfault_read_many_guarded_ecc_otp( uint16_t start_otp_row, void* buffer, size_t bytesToRead ) { // Note: must also handle reads of odd number of bytes } ```

Ok... while using guarded reads and setting up a fault handler is ***possible***,
the resulting code is likely not ***maintainable*** by most coders.  While a
dedicated library author could do this, that level of complexity would not be
the preffered solution.

## Hardware either hides ECC errors, or raises BusFault ... nothing in between

Embedded programming is still generally based around return status codes
from `C` based APIs. Thus, it was critical to have a simple way to read
OTP data, and have assurance that the data was reliably retrieved.   That
alone would have been reason enough to create the library.

The ECC correction, including parity and BRBP bits, would also have been
reason enough, standing alone, to create the library.

## Other Encodings ... Tricky to Validate

The bootrom actually used data that was encoded in the OTP in a few additional
formats.  Generally speaking, those other formats needed redundancy, while
retaining the ability to set additional bits in the data (ECC encoding prevents
future updates to the data, because typically at least one bit will need to
transition from `1` back to `0`).  Three formats are found, which I refer to as:

* `BYTE3X` -- Each OTP row stored a single byte of data, recorded redundantly 3x in that single OTP row.
* `RBIT3` -- Each 24-bits of data is stored redundantly in three consecutive OTP rows.  At least two of the rows must have a bit set to `1` for the bit to be considered a `1`.
* `RBIT8` -- Each 24-bits of data is stored redundantly in eight consecutive OTP rows.  At least three of the rows must have a bit set to `1` for the bit to be considered a `1`.

At first glance, these seem relatively easy to use.  And where the OTP rows
are reliably readable, it is true.  However, especially for `RBIT8`, there
are edge cases that get tricky quickly ... such as what to do when fewer than
all of the eight OTP rows are readable?

This library has comprehensive support for the edge cases.  Considering these
as N-of-M votes required per bit, with `BYTE3X` and `RBIT3` as 2-of-3,
and `RBIT8` as 3-of-8, then for each bit voted upon:

* If the number of votes for that bit is >= `M`
    * Set the bit in the result.  Note that the data stored at OTP rows that
      failed to read are irrelevant, as they cannot reduce the vote.
    * (SUCCESS for this bit, which is `1`)
* Else if the number of failed reads is >= `(N - votes for that bit)`:
    * While current votes say the value is zero, the failed reads ***might***
      change that result if those failed OTP rows become readable later.
    * (ERROR for at least this bit)
* Else:
    * Insufficient votes to set the bit.  Note that the data stored at OTP rows that
      failed to read are irrelevant, as even if they were all `1`, it would not
      be enough votes to be considered a `1` value overall.
    * (SUCCESS for this bit, which is `0`)

The likelihood of this being coded correctly by every consumer of the
OTP on the RP2350 is very low.  Properly implementing an OTP bit-wise
N-of-M voting scheme, by itself, is worthy of a writing a library.

## Conclusion

There are just two of the discoveries that prompted making a library with:

* Simpler, consistent API that returns errors when data is not reliably retrieved.
* Clearly defined behavior for failure conditions, including all data encodings used by bootrom.
* Potential for virtualization of the OTP, to reduce hardware costs during development.

