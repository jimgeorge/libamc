Serial communications library with the Advanced Motion Controls (AMC) Digiflex
Performance (DP) series brushless servo motor controllers. 

These high-performance servo motor controllers can operate in several modes:

+ Torque mode (current feedback)
+ Velocity mode (velocity feedback from resolver)
+ Position mode (position feedback from encoder)

The drives have a serial (RS-422) port through which various registers with
settings and device status may be accessed. The drive can also be controlled
through the command registers. The same serial port may also be used with AMC's
DriveWare software to commission the drive, and configure it.

The serial protocol includes CRC checks and a packet sequence number, these
ensure that electrical noise and other effects do not corrupt the data transfer
to and from the drive. This library will handle CRC generation, and verify the
packet serial numbers in order to validate error-free communication.

This library has been used to control a DPRANIR-030A400B with resolver
feedback. The drive was first set up using DriveWare, this library is only used
operationally to control the internal command registers and monitor the drive
and motor status.

Please note: this library is provided as-is, the author does NOT assume
responsibility for any damage caused by the use of this software library. It
has not been officially endorsed by Advanced Motion Controls, it is an
independent development based on information provided in the Serial
Communication Reference Manual.

First time build
================

	autoreconf -i
	automake -a
	libtoolize
	autoreconf

Usage
=====

The test directory contains a program that can be used to query a drive and
optionally to send it commands. This program can be used as an example of how
to use the functions in this library.
