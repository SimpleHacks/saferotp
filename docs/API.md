
# SaferOTP library for RP2350

## API

The API provides read and write wrappers for the one time programmable (OTP)
fuses on the RP2350.  It provides support for each of the data encodings
used by the bootrom and described in the datasheet.  In general, data will
likely be `ECC` encoded.

### `ECC` Read / Write functions

#### Summary for `ECC` encoding

`ECC` is the recommended encoding for general use.  Each OTP row stores
16-bits of user data.  Although data read or written must start aligned
on an OTP row, these APIs allow reading or writing an odd number of bytes.

Each of these read functions will return false, unless all the data is
read.  Each of these write functions will return false unless all the
data is written *and* decodes back to the intended value.

#### `bool saferotp_read_single_value_ecc(uint16_t row, uint16_t* out_data);`

Reads data from a single OTP row.

The method applies BRBP, corrects single-bit errors, and returns the decoded data.

#### `bool saferotp_read_data_ecc(uint16_t start_row, void* out_data, size_t count_of_bytes);`

Starting at the specified start_row, reads ECC encoded data, decoding
and correcting each row.  `count_of_bytes` may be odd, in which case
the final OTP row's data will discard the second byte, so calling code
can use native buffer sizes.

#### `bool saferotp_write_single_value_ecc(uint16_t row, uint16_t new_value);`

Writes a single OTP row with 16-bits of data, protected by ECC.
Writes will NOT fail even if a single bit in the OTP row is already set.

#### `bool saferotp_write_data_ecc(uint16_t start_row, const void* data, size_t count_of_bytes);`

Writes the supplied buffer to OTP, 16-bits of user data per OTP row,
starting at the specified start row and continuing until the buffer
is fully written.  Data is `ECC` encoded prior to writing.

The buffer may be of arbitrary size.  If the count of bytes is odd,
the API will pad a zero byte before writing the final row.


### `BYTE3X` Read / Write functions

#### Summary for `BYTE3X` encoding

`BYTE3X` is used by the bootrom to store independent bit flags, where
they may be individually transitions from `0` -> `1`.  See, for example,
the page lock encoding (section 13.5.3 Lock Encoding in OTP).

Examples of independent bit flags stored as `BYTE3X` include:
* `KEYx_VALID` at OTP rows `0xf79..0xf7e`
* `PAGEn_LOCK0` and `PAGEn_LOCK1` at OTP rows `0xf80..0xffd`

`BYTE3X` may also be used to provide a thermometer counter, although
the current bootrom uses `RBIT3` for current thermometer counter values.

`BYTE3X` stores a single byte of data per OTP row.

That single byte is recorded three times, and decoding uses a 2-of-3
voting scheme to determine the final value.  Thus, the resulting byte
will have a bit set to `1` if at least two of the three copies of
that bit are set to `1`.

Each of these read functions will return `false` if one of the
OTP rows cannot be read.

Each of these write functions will return `false` unless each byte
can be written *and* reads back data that decodes to the intended value.


#### `bool saferotp_read_single_value_byte3x(uint16_t row, uint8_t* out_data);`

Reads a single OTP row to retrieve the 8-bit data stored using `BYTE3X` encoding.

#### `bool saferotp_read_data_byte3x(uint16_t start_row, void* out_data, size_t count_of_bytes);`

TODO: Not yet implemented.

#### `bool saferotp_write_single_value_byte3x(uint16_t row, uint8_t new_value);`

Writes a single OTP row with 8-bits of data stored with 3x redundancy.

Note: This function will SUCCEED so long as the resulting value written
has the correct votes to read correctly.

<details><summary>An extreme example</summary><P/>

Writing value `0x57`, where the raw OTP row contains 0x5708A1`:

`0x575757` --> `0b0101'0111'0101'0111'0101'0111` (3x redundancy)
`0x5708A1` --> `0b0101'0111'0000'1000'1010'0001` (existing OTP row)
`0x575FF7` --> `0b0101'0111'0101'1111'1111'0111` (OTP stores the logical OR)

When read back, the three votes are:

 Bytes | Binary        | Note
-------|---------------|----------------
`0x57` | `0b0101'0111` | votes for each bit position
`0x5F` | `0b0101'1111` | votes for each bit position
`0xF7` | `0b1111'0111` | votes for each bit position
`0x57` | `0b0101'0111` | Bit is `1` where 2+ votes

Thus, because it would read back correctly as `0x57`, the API would succeed.

Best-effort detection of data that makes it impossible to succeed
occurs prior to writing the OTP row with addition bits set to `1`.

</details>

#### `bool saferotp_write_data_byte3x(uint16_t start_row, const void* data, size_t count_of_bytes);`

TODO: Not yet implemented.

### `RBIT3` Read / Write functions

#### Summary for `RBIT3` encoding

`RBIT3` is used by the bootrom to store independent bit flags,
using 2-of-3 voting for each bit, similar to `BYTE3X` encoding.

However, while `BYTE3X` stores a single byte per OTP row,
`RBIT3` stores a 24-bit value (preferably the same value)
in each of three consecutive OTP rows. 

Examples of independent bit flags include:
* `BOOT_FLAGS0` at OTP rows `0x048..0x04A`
* `BOOT_FLAGS1` at OTP rows `0x04B..0x04D`
* `USB_BOOT_FLAGS` at OTP rows `0x059..0x05B`

Examples of a thermometer counter includes:
* `DEFAULT_BOOT_VERSION0` and `DEFAULT_BOOT_VERSION1` at OTP rows `0x04F..0x051` and `0x052..0x054`, respectively.

#### `bool saferotp_read_single_value_rbit3(uint16_t start_row, uint32_t* out_data);`

Starting at the specified row, reads a 24-bit value from three consecutive rows.
If all three rows are readable, the 2-of-3 voting is applied to determine the final value.

If a single row is not readable, but the remaining two rows report identical 24-bit values,
the value is returned.  This is possible because, regardless of the bits stored in the third
OTP row, the voted-upon value would not change.

#### `bool saferotp_read_data_rbit3(uint16_t start_row, void* out_data, size_t count_of_bytes);`

TODO: Not yet implemented.

#### `bool saferotp_write_single_value_rbit3(uint16_t start_row, uint32_t new_value);`

Starting at the specified row, writes the 24-bit new value to three consecutive rows.

This function will succeed if, after writing the new value to those rows, an `RBIT3` read
reports the new value.

#### `bool saferotp_write_data_rbit3(uint16_t start_row, const void* data, size_t count_of_bytes);`

TODO: Not yet implemented.

### `RBIT8` Read / Write functions

#### Summary for `RBIT8` encoding

The `RBIT8` encoding is similar to `RBIT3`, except that it uses 3-of-8 voting
instead of 2-of-3 voting for each bit.  This creates a large number of edge
cases and slightly more complex analysis to ensure only correct values are
decoded.

At time of writing, this encoding was only used in two locations:
* `CRIT0` at OTP rows `0x038..0x03F`, "Page 0 Critical Boot Flags"
* `CRIT1` at OTP rows `0x040..0x047`, "Page 1 Critical Boot Flags"

Writing is straightforward, and similar to `RBIT3`.

<details><summary>Reading has the additional edge cases, if one or more rows failed to read.</summary><P/>

If there are no read failures, the eight read values contain the votes
for each bit.  Any bit with at least three votes will be set in the result,
while all other bits will be zero.

If there are three or more read failures, the function will return an error
unless there are at least three votes for every bit (e.g., result of 0xFFFFFF).
This is because, no matter what the values of the successfully read rows, the
rows that failed to read could add enough votes to cause the bit to be set to `1`.

If there are two failed reads, the function will return an error unless all
the rows fully agree on the value of every bit.  This is because, if any bit
has one or two votes, the two OTP rows (if successfully read later) could
flip the result from `0` to `1` for that bit.

If there is only one failed read, the function will return an error if any
bit has exactly two votes.  This is because, the OTP row later reads successfully,
it could flip the vote for that bit from `0` to `1`.

</summary>

Each of these read functions will return `false` if the voted-upon result
cannot be determined.

Each of these write functions will return `false` unless, after writing the
updates, a validating `RBIT8` read returns the new values.

#### `bool saferotp_read_single_value_rbit8(uint16_t start_row, uint32_t* out_data);`

Reads a 24-bit value from OTP rows `start_row .. start_row+7`.

Applies 3-of-8 voting to determine the final value.

#### `bool saferotp_read_data_rbit8(uint16_t start_row, void* out_data, size_t count_of_bytes);`

TODO: Not yet implemented.

#### `bool saferotp_write_single_value_rbit8(uint16_t start_row, uint32_t new_value);`

Writes a 24-bit value to OTP rows `start_row .. start_row+7`.

#### `bool saferotp_write_data_rbit8(uint16_t start_row, const void* data, size_t count_of_bytes);`

TODO: Not yet implemented.

### `Raw` Encoding functions

#### Summary for `RAW` encoding

Use of `RAW` encoding, and by extension use of these APIs, is not
recommended for general use, at least because `RAW` provides no
ECC (nor bit recovery by polarity) protection.  These API are
provided in case there is a need to add another encoding scheme,
or for advanced testing purposes.

In each of these functions, the caller is responsible for:
* Packing or unpacking the 24-bits of data into each `uint32_t`
* Defining and using some type of error correction / detection
* Ensuring that any data buffer is aligned to 4-byte boundary
* Ensuring that any data buffer is an integral multiple of 4 bytes

These last two can be easily accomplished by allocating the buffer
as an array of `uint32_t` values.

Each of these read functions will return false, unless all the data is
read.  Each of these write functions will return false unless all the
data is written *and* verified.

#### `bool saferotp_write_single_value_raw_unsafe(uint16_t row, uint32_t new_value);`

Writes a single OTP row with 24-bits of data.

#### `bool saferotp_read_single_value_raw_unsafe(uint16_t row, uint32_t* out_data);`

Reads a single OTP row raw.

#### `bool saferotp_write_data_raw_unsafe(uint16_t start_row, const void* data, size_t count_of_bytes);`

Write the supplied buffer to OTP, starting at the specified
OTP row and continuing until the buffer is fully written.

#### `bool saferotp_read_data_raw_unsafe(uint16_t start_row, void* out_data, size_t count_of_bytes);`

Read raw OTP row data, starting at the specified
OTP row and continuing until the buffer is filled.

### OTP Virtualization support

The library supports virtualization of OTP rows, to speed up development
and testing of applications that use OTP.

The API is not stable yet.  The following generally corresponds to the
API at the time of writing, but is expected to change after creating
some sample applications that use it, with the goal of keeping the
usage simple and intuitive.

#### OTP Virtualization Summary

The current implementation is simple in concept and operation,
but uses ~16kB of RAM.  Is it intended to add CMakefile options that
enable virtualization support, and to disable it by default.

A single large buffer corresponding the full set of OTP rows
exists as a static variable in the library.  This buffer is,
therefore, always allocated in RAM, even if the functionality
is not actively used.

Each OTP row is represented by a `uint32_t` value, with the
actualy OTP data stored in the least significant 24 bits.

If the top eight bits are `0x00`, then the remaining 24 bits
store the raw OTP data, which are returned when reading the
row by the virtualization layer.

If the top eight bits are `0xFF`, then the row will return an
error when it is read by the virtualization layer.  This not
only simplifies copying existing OTP values, but ensures a
method to indicate a row that is 100% unreadable ... useful
for testing purposes.

ALL OTHER VALUES FOR THE TOP EIGHT BITS ARE RESERVED FOR
FUTURE USE.  For example, a future version of the library
may encode an OTP row that is unreliable, such as by failing
reads some percentage of the time, or flipping some bit(s)
some percentage of the time.

#### `bool saferotp_virtualization_init_pages(uint64_t ignored_pages_mask);`

Initializes the virtualization layer.

The OTP is split into 64 pages, each of which stores 64 rows of data.
If the corresponding bit in `ignored_pages_mask` is clear (`0`), then
the values of the corresponding page are initialized by reading the
current values from the real OTP (and errors are stored as such).
If the corresponding bit is set (`1`), then the OTP rows for that page
remain zero-initialized.

This function may only be called once.  Subsequent calls will have
no effect, as OTP access via this library will already be virtualized.

Returns `true` if the virtualization layer was successfully initialized.

#### `bool saferotp_virtualization_restore(uint16_t starting_row, const void* buffer, size_t buffer_size);`

Restores a consecutive set of OTP rows to the values provided in the buffer.
While generally intended to allow simple restoration of virtualized OTP values
to a given state (e.g., for testing purposes), this function may also be used
to restore OTP state across reboots (e.g., by storing the state in FLASH or the like).

This API may be called at any time after the virtualization layer has been initialized.
The restoration may be done one row at a time, or in larger chunks, to support various
storage alignment restrictions.

Each row to be restored is represented by a `uint32_t` value in the buffer.

#### `bool saferotp_virtualization_save(uint16_t starting_row, void* buffer, size_t buffer_size);`

Saves a consecutive set of virtualized OTP rows to the provided buffer,
ignoring any implied permissions or values that would indicate reading
the virtualized row should report an error.

This is intended to allow state to be stored / restored externally,
enabling testing of virtualized OTP across reboots.

This API may be called at any time after the virtualization layer has been initialized.
Saving rows may be done one row at a time, or in larger chunks, to support various
storage alignment restrictions.

Each row to be saved requires four bytes (a `uint32_t` value) in the provided buffer.

