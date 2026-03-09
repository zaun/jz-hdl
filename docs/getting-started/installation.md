---
title: Installation
lang: en-US

layout: doc
outline: deep
---

# Installation

## Prerequisites

- A C99-compatible compiler (GCC, Clang, or MSVC)
- CMake 3.16 or later

## Building from source

```bash
# Clone the repository
git clone <repository-url>
cd JZ-HDL

# Configure the build
cmake -S jz-hdl -B jz-hdl/build

# Build
cmake --build jz-hdl/build
```

The compiler binary is produced at `jz-hdl/build/jz-hdl`.

## Building with tests

```bash
cmake -S jz-hdl -B jz-hdl/build -DBUILD_TESTING=ON
cmake --build jz-hdl/build

# Run all tests
ctest --test-dir jz-hdl/build
```

## Verifying the installation

```bash
# Check that the compiler runs
jz-hdl/build/jz-hdl --help

# Lint a test file
jz-hdl/build/jz-hdl --lint jz-hdl/tests/blink.jz
```
