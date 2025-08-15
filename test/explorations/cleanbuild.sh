#!/bin/bash

# set the script to exit on any command that returns a non-zero status
set -e

# * Current working directory must be depot root.
# * Run as `./clean_build.sh`
# TODO:
# * [ ] Automatically set proper working directory (depot root, parent of script directory)

# update CMake for both rp2040 builds and rp2350 builds
cmake -S . -B build_rp2350 -DBP_PICO_PLATFORM=rp2350   || exit 12

# optionally, do a clean build each time
# How to make build fail on all warnings?
cmake --build ./build_rp2350 --parallel --target clean || exit 32

# build everything
cmake --build ./build_rp2350 --parallel --target all   || exit 52

