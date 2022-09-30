# Building and flashing

1. Make sure you've got all of the submodules in this repo correctly initialised;
```
git submodule update --init --recursive
```

2. If this is your first time setting up the repo, then you will need to install
the ESP-IDF tools. You can consult the [ESP-IDF docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html)
for more detailed instructions, but the TL;DR is that you'll want to run
something like this:
```
./lib/esp-adf/esp-idf/install.sh esp32
```

3. As a final piece of setup, you will need to source the env file in this repo
to correctly set up your environment for building.
```
. ./.env
```

3. You can now build the project using `idf.py build`. Or to flash the project
onto your board, something like:
```
idf.py -p /dev/ttyUSB0 -b 115200 flash
```
(give or take the correct serial port)

Remember that you will need to boot your ESP32 into software download mode
before you will be able to flash.

# clangd setup

A regular build will generate `build/compile_commands.json`, which clangd will
automatically pick up. However, there are a couple of additional steps to get
everything to place nicely.

First, you will need to download the xtensa clang toolchain. You can do this
via ESP-IDF by running  `idf_tools.py install xtensa-clang`

This will install their prebuild clang into a path like `~/.espressif/tools/xtensa-clang/VERSION/xtensa-esp32-elf-clang/`

Next, you will need to configure clangd to use this version of clang, plus
forcible remove a couple of GCC-specific build flags. Do this by creating
`.clangd` in the root directory of your project, with contents like so:

```
CompileFlags:
  Add: [-mlong-calls, -isysroot=/Users/YOU/.espressif/tools/xtensa-clang/VERSION/xtensa-esp32-elf-clang]
  Remove: [-fno-tree-switch-conversion, -mtext-section-literals, -mlongcalls, -fstrict-volatile-bitfields]
```

You should then get proper LSP integration via clangd, give or take a couple of
oddities (e.g. for some reason, my install still can't see `stdio.h`. NBD tho.)

Expect this integration to improve sometime in the near future, per [this forum
thread](https://esp32.com/viewtopic.php?f=13&t=29563).
