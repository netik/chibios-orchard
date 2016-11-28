#!/bin/bash

#/usr/local/bin/openocd -f interface/cmsis-dap.cfg \
#/usr/local/bin/openocd -f interface/ftdi/olimex-arm-usb-ocd-h.cfg \
/Applications/GNU\ ARM\ Eclipse/OpenOCD/0.10.0-201610281609-dev/bin/openocd \
 -f interface/ftdi/olimex-arm-usb-ocd-h.cfg \
 -f interface/ftdi/olimex-arm-jtag-swd.cfg \
 -f target/klx.cfg -c "gdb_flash_program enable" -c "gdb_breakpoint_override hard"


