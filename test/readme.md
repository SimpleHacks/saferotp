# Descriptions


## explorations

This directory is historical in nature.
It is an archive of an exploratory application
that was used to:

* investigate USB whitelabel behavior
* discover the issues that prevent
  reliance on non-guarded OTP reads
* exhaustive validation of encoded ECC space
  * All single-bit-flip decodes to same original value
  * Application of BRBP decodes to same original value
  * BRBP + single-bit flip decodes to same original value
* discover 24-bit **"phantom"** ECC patterns

### Phantom ECC patterns

A **"phantom"** 24-bit ECC pattern (`P`) is one
which decodes to 16-bit data (`D`) without
error, but where re-encoding that 16-bit
data (`D`) into its canonical 24-bit ECC
representation (`C`) results in a **_different_**
bit pattern.

Discovery of these phantom patterns can start
by ignoring the BRBP bits, as BRBP is applied
prior to ECC correction, and each discovered
22-bit phantom pattern can be converted into
four 24-bit phantom patterns by applying BRBP
in a trivial fashion.  A further 22 phantom
patterns can then be generated that have a
single-bit error intentionally added.  This
reduced the search space to 22 bits.

Thus, a "phantom" encoding `P` is one where:

* `P` is an arbitrary 22-bit string
* `ECC_DECODE(P) -> D` (16-bit string, w/o decoding errors)
* `ECC_ENCODE(D) -> C` (22-bit canonical encoding)
* `C != P`

Example for `D` of `0x8b 0xb9` (`0xb98b` 16-bit LE):

* `C` == `0x8b 0xb9 0x25`
* `P` == `0x8b 0xb9 0x12`
* `P` == `0x8b 0xb9 0x18`
* `P` == `0x8b 0xb9 0x1b`
* `P` == `0x8b 0xb9 0x1d`
* `P` == `0x8b 0xb9 0x1e`
* `P` == `0x8b 0xb9 0x33`
* `P` == `0x8b 0xb9 0x39`
* `P` == `0x8b 0xb9 0x3a`
* `P` == `0x8b 0xb9 0x3c`
* `P` == `0x8b 0xb9 0x3f`


## Others to be added later?
