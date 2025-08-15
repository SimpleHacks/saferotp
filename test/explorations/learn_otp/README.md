
# Learn OTP

## Purpose

Use the one-time programmable fuses on the Raspberry Pi RP2350 chip,
testing various configurations and data, to understand how the chip
actually responds under various conditions.

Uses RTT for debug output and for user input.

The genesis of the `saferotp` library was this code base.
The behaviors (and problems) of the initial boards were found
here, and only broken into the separate library after stable.

## Problems

These can allow additional errors to sneak through.

* ECC algorithm in the datasheet was underspecified.
* Bootrom does not report errors when reading ECC data.
* Bootrom does not validate decoded ECC data (if detected
a bitflip), if re-encoded with ECC, matches the read data.

Development for mere mortals is either expensive, as any
mistake may make the board unusable, or really, really slow
work.  Having a set of well-tested higher-level APIs can
greatly reduce this burden.

## Compile / debug

Compilation presumes the PICO SDK is installed.

```bash
pushd ~/pico/my
# Configure CMake in a custom build directory
cmake -S . -B build_rp2350 -DBP_PICO_PLATFORM=rp2350
# Clean any leftover files from previous builds
cmake --build build_rp2350 --target clean
# build the project
cmake --build build_rp2350 --target all
popd
```

Debugging presumes the PICO SDK version of openocd is installed.

```bash
# important to start from the directory indicated
pushd ~/pico/my
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000"
popd
```

Then, from another terminal, `telnet localhost 4444` and
send the following commands to openocd:

```bash
# Relative path is based on CWD being ~/pico/my
reset halt
program ./build_rp2350/learn_otp/learn_otp.elf
# This next line may error the first time it's run....
rtt stop
# enable core 1 first, then enable core 0
rp2350.dap.core1 arp_reset assert 0
rp2350.dap.core0 arp_reset assert 0

# Allow the cores to run for ~500ms so SEGGER RTT control block is setup
sleep 500
# Search for the control block
rtt setup 0x20000000 0x100000 "SEGGER RTT"
# Start RTT between openOCD and the target
rtt start
# This next line may error the 2nd and later time it's run....
rtt server start 4321 0
```

```
# First time around
reset halt; program ./build_rp2350/learn_otp/learn_otp.elf; rp2350.dap.core1 arp_reset assert 0; rp2350.dap.core0 arp_reset assert 0; sleep 500; rtt setup 0x20000000 0x100000 "SEGGER RTT"; rtt start; rtt server start 4321 0
```

```
# If RTT already setup
reset halt; program ./build_rp2350/learn_otp/learn_otp.elf; rtt stop; rp2350.dap.core1 arp_reset assert 0; rp2350.dap.core0 arp_reset assert 0; sleep 500; rtt setup 0x20000000 0x100000 "SEGGER RTT"; rtt start
```


In a third console, run `telnet localhost 4321` to see
the RTT output and to provide input to the program.


