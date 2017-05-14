#!/bin/bash

make

#/usr/local/bin/openocd -f interface/cmsis-dap.cfg \
#/usr/local/bin/openocd -f interface/ftdi/olimex-arm-usb-ocd-h.cfg \

# two JTAGS, one programmer.
/Applications/GNU\ ARM\ Eclipse/OpenOCD/0.10.0-201610281609-dev/bin/openocd \
 -f interface/ftdi/olimex-arm-usb-ocd-h-1.cfg \
 -f interface/ftdi/olimex-arm-jtag-swd.cfg \
 -f target/klx.cfg \
 -c "gdb_flash_program enable" -c "gdb_breakpoint_override hard" \
 -c "program /Users/jna/src/chibios-orchard/badge/build/badge.elf" \
 -c "reset" \
 -c "exit" &

/Applications/GNU\ ARM\ Eclipse/OpenOCD/0.10.0-201610281609-dev/bin/openocd \
 -f interface/ftdi/olimex-arm-usb-ocd-h-2.cfg \
 -f interface/ftdi/olimex-arm-jtag-swd.cfg \
 -f target/klx.cfg \
 -c "gdb_flash_program enable" -c "gdb_breakpoint_override hard" \
 -c "program /Users/jna/src/chibios-orchard/badge/build/badge.elf" \
 -c "reset" \
 -c "exit"




