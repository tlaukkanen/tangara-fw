# Building and flashing

1. First, consult the ESP-IDF docs for the basic toolchain setup:
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html
2. Once you've done this, you should be able to build with `idy.py build`
3. TODO: Verify flashing configuration once we finally get our modules

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
