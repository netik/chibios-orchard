#!/usr/local/bin/python

import os
import sys
import serial
import time
import datetime

print "Current time:"
now = int(time.mktime(datetime.datetime.now().timetuple()))
print now

ser = serial.Serial(timeout=1,
                    xonxoff=False,
                    dsrdtr=True,
                    rtscts=True)
ser.baudrate = 115200
ser.port = '/dev/cu.usbserial-A505TQUZ'

ser.open()
if not ser.is_open:
    print "failed to open port."
    sys.exit(1)

ser.write(b"\r")
ser.write(b"\r")
ser.flush()

i=0

# UTC To PST
now = now - (60 * 60 * 7)

while 1 == 1:
    i = i + 1
    b = ser.readline()
    print "%d %s" % (i,  b.rstrip())

    if b.rstrip() == "hail!>":
        print ("config set rtc %ld\r" % now)
        ser.write("config set rtc %ld\r" % now)
        ser.flush()
        time.sleep(1)
        sys.exit(0)
