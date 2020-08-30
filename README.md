# K64 MCU Project Template

## Requirements
1. Install [Meson](https://mesonbuild.com).

2. Install the following:
    - `avr-gcc`
    - `avr-binutils`
    - `avr-libc` (optional, for libc functions)
    - `avrdude` (recommended, for flashing)

## Building Your Project
1. Customize the `meson.build` file to include the files for your project.
    - C source files should be added separately from ASM source files,
      as denoted in the build file.
    - `local_headers` should be set to the root of the include search tree.
      __Do not list every header file.__
    - Add any project options you require with `add_project_arguments`
2. Set appropriate options in `meson_options.txt`
    - Define the correct F\_CPU for your target. (default: 16000000)
    - Define the programmer for AVRDUDE to use. (default: usbtiny)
3. Set the CPU type in `template_files/avr_files/avr_cross_properties.cfg`.
    - AVRDUDE can use GCC names, but the not the converse. (default: atmega328p)

4. Run `python init.py`. Only Python 3 is supported.  This script will customize
   files in template\_files with the included replacements.  This may be
   extended for your project if necessary, and acts as a way to configure the
   build before handing over control to meson.

5. Run `meson <build dir> --cross-file avr_files/avr_cross_properties.cfg`.
   This will create a Ninja build description in `<build dir>`.

6. Run `ninja -C <build dir>`.  This will create a binary file which
   can be flashed to the K64 in `<build dir>`.

## Flashing the Target
AVRDUDE provides the flashing mechanism and supports a wide variety of
AVR and other programmers.
Simply type `ninja -C <build dir> flash` to flash the target with AVRDUDE.
AVRDUDE can be invoked manually if something goes wrong/the target becomes
locked. 
    `avrdude -p <part> -c <programmer> -U flash:w:<build dir>/main.hex:i`
