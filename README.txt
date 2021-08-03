Uploader
========

A very simple EEPROM programmer designed to work with the VSA.

Protocol Explanation
====================

The uploader program will start by opening the serial port that the microcontroller
is connected to. Then, it will wait unit the microcontroller sends the initial 
packet with the version it is running. It will look like this:

 * (PC) Open port
 * (MC) Auto reset
        Send bytes 0x01 {version number}
 * (PC) Compare versions
        If no match, give up
        Else, initiate handshake
        Send bytes 0x02 0x00
        Send bytes {args.high} {args.low}
        Send bytes {receiving} {sending}
 * (MC) Update state

[[TODO]]
 
