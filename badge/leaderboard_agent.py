#!/usr/local/bin/python
#
# leaderboard_agent.py
#
# This agent will pick up data from a badge running with
# LEADERBOARD_AGENT enabled over the serial port, then forward that
# badge's data off to the leaderboard. You must have a valid API key
# to post data to the leaderboard.
#
# There is very little verification of the inbound JSON here. We
# expect the incoming JSON to be in good shape and sanitized by the
# badge that is acting as LEADERBOARD_AGENT.
#
# Configuration
# -------------
# In the file "leaderboard_agent_config.py",
# set the variables APIURL, APIKEY, and DEBUG.
#
#
# John Adams <jna@retina.net> 6/19/2017

import os
import sys
import serial
import time
import datetime
import requests
import json
from leaderboard_agent_config import APIKEY,APIURL,DEBUG
from math import floor

def build_rfc3339_phrase(datetime_obj):
    datetime_phrase = datetime_obj.strftime('%Y-%m-%dT%H:%M:%S')
    us = datetime_obj.strftime('%f')
    seconds = None
    try:
        seconds = datetime_obj.utcoffset().total_seconds()
    except AttributeError:
        pass
    
    if seconds is None:
        datetime_phrase += 'Z'
    else:
        # Append: decimal, 6-digit uS, -/+, hours, minutes
        datetime_phrase += ('.%.6s%s%02d:%02d' % (
            us,
            ('-' if seconds < 0 else '+'),
            abs(int(floor(seconds / 3600))),
            abs(seconds % 3600)
        ))
        
    return datetime_phrase

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
    dt = datetime.datetime.now()

    if b.startswith("PING:"):
        (junk, packet) = b.rstrip().split("PING: ")

        if DEBUG:
            print packet

        parsedjson = {}
        try:
            parsedjson = json.loads(packet)
        except ValueError:
            print "[%s] Unable to parse inbound json: %s" % (build_rfc3339_phrase(dt), packet)
            continue
            
        parsedjson["apikey"] = APIKEY
        if DEBUG:
            print parsedjson

        r = requests.post(APIURL, json=parsedjson)
        if DEBUG:
            print r.status_code
            print r.json()
        
        if r.status_code == 200:
            print "[%s] Update OK: %s (netid %s)" % (build_rfc3339_phrase(dt), parsedjson[u'name'], parsedjson[u'badgeid'])
        else:
            print "[%s] Update FAILED: %s (netid %s)" % (build_rfc3339_phrase(dt), parsedjson[u'name'], parsedjson[u'badgeid'])
