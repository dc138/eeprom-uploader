Uploader
========

A simple dual EEPROM programmer designed to work with an arduino mini and
some breadboards

Protocol Explanation
====================

Initial packet & handshake
--------------------------

The uploader program will start by opening the serial port that the microcontroller
is connected to. Then, it will wait unit the microcontroller sends the initial 
packet with the version it is running. If the versions match, the PC will procede
and send the state flags. It will look like this:

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
 
