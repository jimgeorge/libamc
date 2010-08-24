/**
\file serial.c
\brief Module for serial port routines
\author Jim George
*/

#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>
#include <linux/serial.h>
#include <errno.h>
#include "serial.h"

#define SERIAL_PORT_SPD_TBL_MAX 18
static struct {
	int baud;
	speed_t ident;
} serial_port_speed_table[SERIAL_PORT_SPD_TBL_MAX] = {
{50, B50},
{75, B75},
{110, B110},
{134, B134},
{150, B150},
{200, B200},
{300, B300},
{600, B600},
{1200, B1200},
{1800, B1800},
{2400, B2400},
{4800, B4800},
{9600, B9600},
{19200, B19200},
{38400, B38400},
{57600, B57600},
{115200, B115200},
{230400, B230400}
};

/**
\brief Get interface speed macro from integer speed
\param speed Input speed, in baud
\return One of the interface speed constants from termios library,
or -1 on error

Converts an integer baud rate into one of the termios speed
macros. If the baud rate specified is not found, an error is
returned.
*/
static speed_t serial_port_get_speed(const unsigned int speed)
{
	int ctr;
	
	for (ctr = 0; ctr < SERIAL_PORT_SPD_TBL_MAX; ctr++) {
		if (speed == serial_port_speed_table[ctr].baud) {
			return serial_port_speed_table[ctr].ident;
		}
	}
	return -1;
}

/**
\brief Initialize serial port
\param *device_name Unix device name for the serial port to open
\param speed Speed at which to open port
\param *port Pointer to the file descriptor for the serial port
\return 0 on success, -1 on error

The port is opened with 8N1 settings (8-bit, no parity, 1 stop bit)
*/
int serial_port_init(const char *device_name,
	unsigned int speed,
	int *port)
{
	struct termios term_st;
	
	assert(device_name != NULL);
	assert(port != NULL);
	
	*port = open(device_name, O_RDWR | O_NOCTTY);
	if (*port < 0) {
		return -1;
	}
	
	bzero(&term_st, sizeof(struct termios));

	if (tcgetattr(*port, &term_st)) {
		perror("tcgetattr");
		return -1;
	}

	/* Set interface speed */
	{
		int spd_macro = serial_port_get_speed(speed);
		if (spd_macro == -1) return -1;
		cfsetispeed(&term_st, spd_macro);
		cfsetospeed(&term_st, spd_macro);
	}
	/* Enable raw mode output */
	cfmakeraw(&term_st);
	term_st.c_oflag &= ~OPOST;
	
	/* Don't own the terminal (^C won't kill us), Enable rcv, Drop DTR on close */
	term_st.c_cflag |= (CREAD | HUPCL);
	
	/* Set 8-bit port size */
	term_st.c_cflag &= ~CSIZE; term_st.c_cflag |= CS8;

	/* No parity, 1 stop bit */
	term_st.c_cflag &= ~PARENB; term_st.c_cflag &= ~CSTOPB;
	
	/* Turn off flow control, RS-485 does not use it */
	term_st.c_cflag &= ~CRTSCTS;
	term_st.c_iflag &= ~(IXON | IXOFF | IXANY);
	
	/* Min. 1 character for read to unblock */
	term_st.c_cc[VMIN] = 1;
	/* Set timeout to 1 second */
	term_st.c_cc[VTIME] = 10;
	
	if (tcflush(*port, TCIOFLUSH)) {
		perror("tcflush");
		return -1;
	}
	if (tcsetattr(*port, TCSANOW, &term_st)) {
		perror("tcgetattr");
		return -1;
	}
	
	return 0;
}

void serial_port_flush(int fd)
{
	tcflush(fd, TCIOFLUSH);
}

