<!--
Copyright 2023 jacqueline <me@jacqueline.id.au>

SPDX-License-Identifier: CC0-1.0
-->

# Building and flashing

1. Make sure you've got all of the submodules in this repo correctly initialised:

```
git submodule update --init --recursive
```

2. If this is your first time setting up the repo, then you will need to install
the ESP-IDF tools. You can consult the [ESP-IDF docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html)
for more detailed instructions, but the TL;DR is that you'll want to run
something like this:

```
./lib/esp-idf/install.sh esp32
```

3. As a final piece of setup, you will need to source the env file in this repo
to correctly set up your environment for building.

```
. ./.env
```

There is also a `.env.fish` for fish users.

4. You can now build the project using `idf.py build`. Or to flash the project
onto your board, something like:

```
idf.py -p /dev/ttyUSB0 -b 115200 flash
```

(give or take the correct serial port)

# Running tests

Tests are implemented as a separate application build, located in the `test`
directory. We use Catch2 as our test framework.

To run them, navigate to the test directory, then build and flash as normal.
Connect to your device via UART, and you will be presented with a terminal
prompt that you may run tests from.

To add new tests to a components, you must:
 1. Create a `test` subcomponent within that component. See `drivers/test` for
    an example of this.
 2. Include the component in the test build and list of testable components, in
    `test/CMakeLists.txt`.


# VSCode setup

When using the Espressif IDF extension, you may want to set the following in your settings.json file:
```
  "idf.espIdfPath": "${workspaceFolder}/lib/esp-idf",
  "idf.espIdfPathWin": "${workspaceFolder}/lib/esp-idf"
```

# LSP (clangd) setup

A regular build will generate `build/compile_commands.json`, which clangd will
automatically pick up. However, there are a couple of additional steps to get
everything to play nicely.

First, you will need to download the xtensa clang toolchain. You can do this
via ESP-IDF by running  `idf_tools.py install xtensa-clang`

This will install their prebuild clang into a path like `~/.espressif/tools/xtensa-clang/VERSION/xtensa-esp32-elf-clang/`

Next, you will need to configure clangd to use this version of clang, plus
forcible remove a couple of GCC-specific build flags. Do this by creating
`.clangd` in the root directory of your project, with contents like so:

```
CompileFlags:
  Add: [
    -ferror-limit=0,
    -I/Users/YOU/.espressif/tools/xtensa-esp32-elf/esp-2021r2-patch5-8.4.0/xtensa-esp32-elf/include,
    -I/Users/YOU/.espressif/tools/xtensa-esp32-elf/esp-2021r2-patch5-8.4.0/xtensa-esp32-elf/xtensa-esp32-elf/include,
    -I/Users/YOU/.espressif/tools/xtensa-esp32-elf/esp-2021r2-patch5-8.4.0/xtensa-esp32-elf/xtensa-esp32-elf/include/c++/8.4.0,
    -I/Users/YOU/.espressif/tools/xtensa-esp32-elf/esp-2021r2-patch5-8.4.0/xtensa-esp32-elf/xtensa-esp32-elf/include/c++/8.4.0/xtensa-esp32-elf,
    -isysroot=/Users/YOU/.espressif/tools/xtensa-esp32-elf/esp-2021r2-patch5-8.4.0/xtensa-esp32-elf/xtensa-esp32-elf,
  ]
  Remove: [
    -Wduplicated-cond,
    -Wduplicated-branches,
    -Wlogical-op,
    -fno-tree-switch-conversion,
    -mtext-section-literals,
    -mlongcalls,
    -fstrict-volatile-bitfields,
  ]
  Compiler: /Users/YOU/.espressif/tools/xtensa-clang/esp-clang/bin/clang++
```

You should then get proper LSP integration via clangd.
