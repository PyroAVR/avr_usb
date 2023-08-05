project('avr_project_template', 'c')

proj_queue = subproject('base_queue')
dep_queue = proj_queue.get_variable('dep_queue')

# dependencies
dependencies = [dep_queue]

# Project include path.
local_headers = ['include']

# Do not remove the core files if you are not certain of what you are doing.
# List of C sources to compile. Relative to project root.
c_sources = [
    'src/main.c',
    'src/uart.c',
    'src/usb_descriptors.c',
    'src/monoqueue.c',
    'src/32u4_usb.c',
    'src/usb_std.c',
]

# List of ASM sources to compile.  Relative to project root.
asm_sources = [
]

## project setup

# debug build
if get_option('buildtype') == 'debug'
    add_project_arguments(['-DDEBUG=1', '-ggdb3'], language: 'c')

endif


### DON'T EDIT BELOW THIS LINE ###

objcopy = find_program('objcopy')
avrdude = find_program('avrdude', required: false, disabler: true)

add_project_arguments(['-mmcu=' + host_machine.cpu()], language: 'c')
add_project_link_arguments(['-mmcu=' + host_machine.cpu()], language: 'c')
add_project_link_arguments(['-Wl,-Map=' + 'main.map'], language: 'c')

add_project_arguments(['-DF_CPU=' + get_option('cpu_freq').to_string()], language: 'c')
add_project_link_arguments(['-DF_CPU=' + get_option('cpu_freq').to_string()], language: 'c')

_includes = local_headers
_incl_dirs = include_directories(_includes)

asm_static_object = static_library(
    'asm_static_object',
    asm_sources,
    include_directories: _incl_dirs,
    c_args: get_option('c_args') + ['-x', 'assembler-with-cpp'],
    install: false
  )

main = executable(
    'main.elf',
    c_sources,
    include_directories: _incl_dirs,
    #link_with: asm_static_object,
    dependencies: dependencies
)

main_hex = custom_target(
    'main.hex',
    command: [
        objcopy,
        '-j', '.text', '-j', '.data',
        '-O', 'ihex',
        '@INPUT@', '@OUTPUT@'
    ],
    input: main,
    output: 'main.hex',
    build_by_default: true
)

if avrdude.found()
    # creates an empty file named 'flash' to get around meson's inability
    # to have a target create no outputs.
    flash = custom_target(
        'flash',
        build_always_stale: true,
        command: [
            avrdude, '-p', host_machine.cpu(),
            '-c', get_option('programmer'),
            '-U', 'flash:w:@INPUT@:i', '-B', '1'
        ],
        input: main_hex,
        capture: true,
        output: 'flash'
    )
endif
