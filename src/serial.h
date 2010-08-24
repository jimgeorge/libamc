/**
\file serial.h
\brief Include file serial port routines
\author Jim George
*/

#ifndef _SERIAL_H_

int serial_port_init(const char *device_name,
	unsigned int speed,
	int *port);
void serial_port_flush(int fd);
void serial_port_set_rts(int fd);
void serial_port_clear_rts(int fd);

#define _SERIAL_H_

#endif /* _SERIAL_H_ */

