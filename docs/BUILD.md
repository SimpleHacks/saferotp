# How to build the library and examples

## Prerequisites

* CMake 3.13 or later
* Raspberry Pi Pico SDK 2.1.0 or later
* ...

Basically, if you can build the Pico SDK examples,
you should be able to build this library and its
examples.

Tested on Ubuntu 24.04 LTS, using WSL2 (Windows 11).

## One-time setup

* Install the Pico SDK
* Install other prerequisites (e.g., using `apt` for Ubuntu systems)
* Clone the repository
* Perform the build steps

## Build steps

BUGBUG / TODO - Not working yet.  Don't know why yet.

```bash
# Configure CMake according to the various CMakeLists.txt files
cmake -S . -B build
# cmake -S . -B build -DBP_PICO_PLATFORM=rp2350

# Clean the build directory (if needed)
cmake --build ./build --parallel --target clean

# Build everything ... both the library and the examples
cmake --build ./build --parallel --target all
```

## Where are the executable files?

The executable files will be under the `build` subdirectory.
For example, the `00_hello_otp` example will compile to
`./build/00_hello_otp/00_hello_otp.uf2` (and `.bin`, `.elf`, etc.).



