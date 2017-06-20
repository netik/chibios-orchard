#!/usr/local/bin/python
#
# leaderboard_agent.py
#
# This agent will pick up data from a badge running with
# LEADERBOARD_AGENT enabled over the serial port, then forward that
# badge's data off to the leaderboard. You must have a valid API key
# to post data to the leaderboard.
#
# John Adams <jna@retina.net> 6/19/2017

import os
import sys
import serial
import time
import datetime
import requests
import json

APIURL="https://dc25spqr.com/update.json"
APIKEY="keykeykeykey"

ser = serial.Serial(timeout=1,
                    xonxoff=False,
                    dsrdtr=True,
                    rtscts=True)
ser.baudrate = 115200
ser.port = '/dev/cu.usbserial-A505O8VO'

ser.open()
if not ser.is_open:
    print "failed to open port."
    sys.exit(1)

ser.flush()

while 1 == 1:
    b = ser.readline()

    if b.startswith("PING:"):
        (junk, packet) = b.rstrip().split("PING: ")
        print packet
        parsedjson = json.loads(packet)
        parsedjson["apikey"] = APIKEY
        print parsedjson
        r = requests.post(APIURL, json=parsedjson)
        print r.status_code
        print r.json()
        
        
