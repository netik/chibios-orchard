# DEFCON 25 SPQR Badge

## Building the code
I am building on a Mac Pro (OSX 10.11) and Bill is using FreeBSD.

Bill has a bunch of docs, datasheets, and scripts here, but you probably won't need them. 
https://people.freebsd.org/~wpaul/w00t/badge/

1\. Update submodules... (ugfx, etc...)

```git submodule update --init --recursive```

Note that we're using our own slightly modified version of ugfx. 

2\. Install the gcc arm compiler.

```
  cd /usr/local
  wget https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-mac.tar.bz2
  tar xjf gcc-arm-none-eabi-5_4-2016q3-20160926-mac.tar.bz2   
  ln -s gcc-arm-none-eabi-5_4-2016q3-20160926-mac.tar.bz2 cortex-m0
```

Or, better, install the gnu arm toolchain. Much less work.
Go to:  https://launchpad.net/gcc-arm-embedded/+download
  
3\. make sure you are using right compilers...

```
  export PATH=/usr/local/cortex-m0/bin:$PATH
  export CFLAGS="-I/usr/local/cortex-m0/include -L/usr/local/cortex-m0/lib"
```

Add this to your .profile or .bashrc or whatever.

4\. Make
```
  cd badge
  make clean; make
```

5\. Start openOCD (assumes you have it installed ...)
( See also: https://people.freebsd.org/~wpaul/w00t/badge/freedom_board.txt )

The version of OpenOCD that I build has support for this device. I used
the following command to connect to it. Run this in one window. 

```
/usr/local/cortex-m0/bin/openocd -f interface/cmsis-dap.cfg -f target/klx.cfg
```

On my Mac Pro I could not get CMSIS-DAP to work over USB AT ALL. It failed miserably.

I am using an Olimex debugger now directly to the SWD headers. See
*Appendix A* at the bottom of this document for help.

The run commnad for on Mac OS with the GNU ARM Toolchain is (run this in it's own window):

```
/usr/local/bin/openocd -f interface/ftdi/olimex-arm-usb-ocd-h.cfg \
  /Applications/GNU\ ARM\ Eclipse/OpenOCD/0.10.0-201610281609-dev/bin/openocd \
    -f interface/ftdi/olimex-arm-usb-ocd-h.cfg \
    -f interface/ftdi/olimex-arm-jtag-swd.cfg \
    -f target/klx.cfg -c "gdb_flash_program enable" -c "gdb_breakpoint_override hard"
```

This can be put into into a script or alias called 'runopenocd', if you like. 

Note: to talk directly to the device via JTAG/SWD header with the Olimex
debugger, use this command:
```
/usr/local/cortex-m0/bin/openocd -f interface/ftdi/olimex-arm-usb-ocd-h.cfg \
				 -f interface/ftdi/olimex-arm-jtag-swd.cfg -f target/klx.cfg
```

6\. Start GDB

OpenOCD provides two interfaces: a telnet server on port 4444 and a remote
GDB server on port 3333. You need to halt the CPU before GDB will connect.
You can use the telnet interface for that:

Open a new window, and type:

```
% telnet localhost 4444

Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
Open On-Chip Debugger
> halt
target state: halted
target halted due to debug-request, current mode: Thread
xPSR: 0x81000000 pc: 0x0000904c psp: 0x1ffffbd0
>
```

7\. Push code to device

```
  cd badge
  (cd build; /usr/local/cortex-m0/bin/arm-none-eabi-gdb -iex "target remote localhost:3333; load" badge.elf  )
  (gdb) load badge.elf
  (gdb) c
```

Code should run!

8\. Get a shell

If you have the Olimex debugger plugged in, and a USB cable plugged
into the development board, you should see a shell at

```sudo cu -s 115200 -l /dev/cu.usbmodem41412```

This device name is not static -- the last few digits of this are randomized, so you may have to
do an ls /dev/cu.* to find the serial port. 

Type 'help'. There's lots of commands and things to play with there.

## Appendix A: Programming tools.

If you are using the freescale KW01 demo board, you'll need the
following to talk to it via SWD

Olimex ARM-USB-OCH-H JTAG debugger ($50):

http://www.mouser.com/ProductDetail/Olimex-Ltd/ARM-USB-OCD-H

Olimex JTAG to SWD (Serial Wire Debug) adapter ($5):

http://www.mouser.com/ProductDetail/Olimex-Ltd/ARM-JTAG-SWD

Embedded Artists 20 pin to 10 pin adapter ($15):

http://www.mouser.com/ProductDetail/Embedded-Artists/EA-ACC-040





