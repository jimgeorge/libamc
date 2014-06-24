/**
\file tests/test-amc.c
\brief Test program for the AMC drive communication library
\author Jim George

This module implements a test program for the AMC drive communications
library. Uses command line options to send one or more packets to the 
drive, and reads back the drive response. Some packets are parsed and
the output is displayed in greater detail.
*/

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#ifdef MOXA
#include <moxadevice.h>
#endif

#include "src/amc.h"

#ifdef MOXA
#warning Using MOXA-specific ioctls to set RS422 mode
#endif

char serial_device[256] = "/dev/ttyM0";
int baudrate = 115200;

int open_drive(struct amc_drive *drv, char *serial_device, int baudrate, int serial_mode, int *serial_fd)
{
	*serial_fd = amc_serial_open(serial_device, baudrate);

	if (*serial_fd == -1) {
		return -1;
	}

#ifdef MOXA
	ioctl(*serial_fd, MOXA_SET_OP_MODE, &serial_mode);
#endif

	if (0 != amc_drive_new(drv, 0x3F, *serial_fd)) {
		return -1;
	}

	amc_get_access_control(drv);

	return 0;
}

#define KP 30.0
#define KI 1.0
#define KS 20000.0

#define COUNTS_PER_REV 4096.0

#define SCALE_DC1 (8192.0/KP)
#define SCALE_DS1 (131072.0/(KI * KS))

enum {
	OPT_GETID = 256,
	OPT_DEBUG,
	OPT_PORT,
	OPT_ENABLEBRIDGE,
	OPT_QUICKSTOP,
	OPT_RESETEVENTS,
	OPT_BRIDGESTATUS,
	OPT_GETIFACE,
	OPT_SETIFACE,
	OPT_GETMOTORSTATUS,
	OPT_SETSPEED,
	OPT_REG16,
	OPT_REG32,
	OPT_WDT,
};

char *usage_string = 
"Retrieve/control modbus registers on AMC servo drives\n"
"Usage:\n"
"--getid: Retrieve drive ID string and version numbers\n"
"--port=<dev>: Set serial port device to dev\n"
"--debug: Show serial comms debug messages\n"
"--bridgestatus: Retrieve power bridge status\n"
"--enablebridge[=n]: Enable the power bridge, n=0 disables, n=1 enables\n"
"--quickstop[=n]: Perform quick stop, n=0 disables, n=1 enables\n"
"--resetevents: Reset latched events, if any\n"
"--getinterfaceinput=<n>: Retrieve value at interface input n\n"
"--setinterfaceinput=<n,val>: Set value at interface input n to specified value\n"
"--getmotorstatus: Get the motor status\n"
"--setspeed=<n>: Set motor speed in rpm\n"
"--reg16=<reg[,val]>: Get or set a 16-bit register. reg is a 16-bit hex number\n"
"        If specified, val is a 16-bit hex number to write.\n"
"--reg32=<reg[,val]>: Get or set a 32-bit register. reg is a 16-bit hex number\n"
"        If specified, val is a 32-bit hex number to write.\n"
"--wdt[=n]: Get/set the Watchdog Timer. Set to 0 to disable\n"
;

static struct option opt_lst[] = {
	{"debug", no_argument, 0, OPT_DEBUG},
	{"port", required_argument, 0, OPT_PORT},

	{"getid", no_argument, 0, OPT_GETID},
	{"bridgestatus", no_argument, 0, OPT_BRIDGESTATUS},
	{"enablebridge", optional_argument, 0, OPT_ENABLEBRIDGE},
	{"quickstop", optional_argument, 0, OPT_QUICKSTOP},
	{"resetevents", no_argument, 0, OPT_RESETEVENTS},
	{"getinterfaceinput", required_argument, 0, OPT_GETIFACE},
	{"setinterfaceinput", required_argument, 0, OPT_SETIFACE},
	{"getmotorstatus", no_argument, 0, OPT_GETMOTORSTATUS},
	{"setspeed", required_argument, 0, OPT_SETSPEED},
	{"reg16", required_argument, 0, OPT_REG16},
	{"reg32", required_argument, 0, OPT_REG32},
	{"wdt", optional_argument, 0, OPT_WDT},

	{NULL, 0, 0, 0}
};

int main(int argc, char *argv[])
{
	struct amc_drive *drv;
	int opt_idx, opt_errors = 0, opt;
	int serial_mode = 0;
	int serial_fd;

#ifdef MOXA
	serial_mode = RS422_MODE;
#endif

	if (argc == 1) {
		puts(usage_string);
		return -1;
	}
	
	drv = malloc(sizeof(struct amc_drive));

	if (-1 == open_drive(drv, serial_device, baudrate, serial_mode, &serial_fd)) {
		printf("Could not open %s\n", serial_device);
		return -1;
	}

	while (-1 != (opt = getopt_long(argc, argv, "dv", opt_lst, &opt_idx))) {
		switch(opt) {
		case OPT_PORT:
			strncpy(serial_device, optarg, 256);
			close(serial_fd);
			if (-1 == open_drive(drv, serial_device, baudrate, serial_mode, &serial_fd)) {
					printf("Could not open %s\n", serial_device);
					return -1;
			}
			break;
		case OPT_GETID:
			{
			char buffer[256];
			amc_get_string(drv, 0x0B, 0x00, buffer, 256);
			printf("Drive name: %s\n", buffer);

			struct amc_product_info pi;
			if (0 == amc_get_product_info(drv, &pi)) {
				printf("Control Board Name: %s [%s]\n", pi.control_board_name, pi.control_board_version);
				printf("Product Part Number: %s [%s]\n", pi.product_part_number, pi.product_version);
			}
			else {
				printf("Could not retrieve product info\n");
				return -1;
			}
			}
			break;
		case OPT_DEBUG:
			drv->debug = 1;
			break;		
		case OPT_ENABLEBRIDGE:
			{
				uint16_t bridge_status;
				if (0 > amc_get_uint16(drv, 0x01, 0x00, &bridge_status)) {
					printf("Could not read bridge status\n");
					return -1;
				}
				if ((optarg == NULL) || strtol(optarg, NULL, 10)) {
					bridge_status &= ~AMC_BC_INHIBIT;
				}
				else {
					bridge_status |= AMC_BC_INHIBIT;
				}
				if (0 > amc_write_uint16(drv, 0x01, 0x00, bridge_status)) {
					printf("Could not write bridge status\n");
					return -1;
				}
			}
			break;
		case OPT_QUICKSTOP:
			{
				uint16_t bridge_status;
				if (0 > amc_get_uint16(drv, 0x01, 0x00, &bridge_status)) {
					printf("Could not read bridge status\n");
					return -1;
				}
				if ((optarg == NULL) || strtol(optarg, NULL, 10)) {
					bridge_status |= AMC_BC_QUICKSTOP;
				}
				else {
					bridge_status &= ~AMC_BC_QUICKSTOP;
				}
				if (0 > amc_write_uint16(drv, 0x01, 0x00, bridge_status)) {
					printf("Could not write bridge status\n");
					return -1;
				}
			}
			break;
		case OPT_RESETEVENTS:
			{
				uint16_t bridge_status;
				if (0 > amc_get_uint16(drv, 0x01, 0x00, &bridge_status)) {
					printf("Could not read bridge status\n");
					return -1;
				}
				bridge_status |= AMC_BC_RESETEVENTS;
				if (0 > amc_write_uint16(drv, 0x01, 0x00, bridge_status)) {
					printf("Could not write bridge status\n");
					return -1;
				}
				bridge_status &= ~AMC_BC_RESETEVENTS;
				if (0 > amc_write_uint16(drv, 0x01, 0x00, bridge_status)) {
					printf("Could not write bridge status\n");
					return -1;
				}
			}
			break;
		case OPT_BRIDGESTATUS:
			{
				uint16_t bridge_status;
				if (0 > amc_get_uint16(drv, 0x01, 0x00, &bridge_status)) {
					printf("Could not read bridge control\n");
					return -1;
				}
				printf("Bridge control: 0x%04X, Bridge: %s, Brake: %s, QuickStop: %s\n",
					bridge_status,
					(bridge_status & AMC_BC_INHIBIT) ? "Inhibited" : "Enabled",
					(bridge_status & AMC_BC_BRAKE) ? "Enabled" : "Disabled",
					(bridge_status & AMC_BC_QUICKSTOP) ? "Active" : "Inactive");

				if (0 > amc_get_uint16(drv, 0x02, 0x00, &bridge_status)) {
					printf("Could not read bridge status\n");
					return -1;
				}
				printf("Bridge status: 0x%04X \t[%c] Bridge Enabled\t[%c] DynBrake\n"
						"\t\t[%c] Shunt Reg Enabled\t[%c] Positive Stop\t[%c] Negative Stop\n"
						"\t\t[%c] PosTorqueInh\t[%c] NegTorqueInh\t[%c] Ext Brake\n",
					bridge_status,
					(bridge_status & AMC_BS_ENABLED) ? 'X' : ' ',
					(bridge_status & AMC_BS_DYNBRAKE) ? 'X' : ' ',
					(bridge_status & AMC_BS_SHUNT) ? 'X' : ' ',
					(bridge_status & AMC_BS_POSSTOP) ? 'X' : ' ',
					(bridge_status & AMC_BS_NEGSTOP) ? 'X' : ' ',
					(bridge_status & AMC_BS_POSTORQUEINH) ? 'X' : ' ',
					(bridge_status & AMC_BS_NEGTORQUEINH) ? 'X' : ' ',
					(bridge_status & AMC_BS_EXTBRAKE) ? 'X' : ' ');

				if (0 > amc_get_uint16(drv, 0x02, 0x01, &bridge_status)) {
					printf("Could not read drive protection status\n");
					return -1;
				}
				printf("Drive protection status: 0x%04X\t[%c] Reset\t[%c] Internal Error\t[%c] Short Circuit\n"
					"\t[%c] Overcurrent\t[%c] Undervoltage\t[%c] Overvoltage\t\t[%c] Overtemp\n",
					bridge_status,
					(bridge_status & AMC_PS_RESET) ? 'X' : ' ',
					(bridge_status & AMC_PS_INTERROR) ? 'X' : ' ',
					(bridge_status & AMC_PS_SHORTCKT) ? 'X' : ' ',
					(bridge_status & AMC_PS_OVERCURRENT) ? 'X' : ' ',
					(bridge_status & AMC_PS_UNDERVOLTAGE) ? 'X' : ' ',
					(bridge_status & AMC_PS_OVERVOLTAGE) ? 'X' : ' ',
					(bridge_status & AMC_PS_OVERTEMP) ? 'X' : ' ');

				if (0 > amc_get_uint16(drv, 0x02, 0x02, &bridge_status)) {
					printf("Could not read system protection status\n");
					return -1;
				}
				printf("System protection status: 0x%04X\t[%c] Param Restore Error\t[%c] Param Store Error\n"
					"\t[%c] Motor Overtemp\t[%c] Feedback Error\t[%c] Overspeed\t[%c] Comms Error\n",
					bridge_status,
					(bridge_status & AMC_SS_RESTOREERR) ? 'X' : ' ',
					(bridge_status & AMC_SS_STOREERR) ? 'X' : ' ',
					(bridge_status & AMC_SS_MOTOROVERTEMP) ? 'X' : ' ',
					(bridge_status & AMC_SS_FEEDBACKERROR) ? 'X' : ' ',
					(bridge_status & AMC_SS_OVERSPEED) ? 'X' : ' ',
					(bridge_status & AMC_SS_COMMERR) ? 'X' : ' ');

				if (0 > amc_get_uint16(drv, 0x02, 0x03, &bridge_status)) {
					printf("Could not read drive status 1\n");
					return -1;
				}				
				printf("Drive status 1: 0x%04X\t[%c] Log Missed\t[%c] Commanded Inhibit\t[%c] User Inhibit\n"
					"\t[%c] Pos Inhibit\t[%c] Neg Inhibit\t[%c] Current Limit\t[%c] Cont Current Limit\n"
					"\t[%c] Current Loop Sat\t[%c] Cmd Dyn Brk\t[%c] User Dyn Brk\t[%c] Shunt Reg\n",
					bridge_status,
					(bridge_status & AMC_DS_LOGMISSED) ? 'X' : ' ',
					(bridge_status & AMC_DS_CMDINHIBIT) ? 'X' : ' ',
					(bridge_status & AMC_DS_USERINHIBIT) ? 'X' : ' ',
					(bridge_status & AMC_DS_POSINH) ? 'X' : ' ',
					(bridge_status & AMC_DS_NEGINH) ? 'X' : ' ',
					(bridge_status & AMC_DS_CURRENTLIM) ? 'X' : ' ',
					(bridge_status & AMC_DS_CONTCURRENT) ? 'X' : ' ',
					(bridge_status & AMC_DS_CLSAT) ? 'X' : ' ',
					(bridge_status & AMC_DS_CMDDYNBRAKE) ? 'X' : ' ',
					(bridge_status & AMC_DS_USERDYNBRAKE) ? 'X' : ' ',
					(bridge_status & AMC_DS_SHUNTREG) ? 'X' : ' ');

				if (0 > amc_get_uint16(drv, 0x02, 0x04, &bridge_status)) {
					printf("Could not read drive status 1\n");
					return -1;
				}
				printf("Drive status 2: 0x%04X\t[%c] Zero Velocity\t[%c] At Command\t[%c] Vel Following Error\n"
					"\t[%c] Pos Velocity Limit\t[%c] Neg Velocity Limit\t[%c] Cmd Profiler\n",
					bridge_status,
					(bridge_status & AMC_DS_ZEROVEL) ? 'X' : ' ',
					(bridge_status & AMC_DS_ATCMD) ? 'X' : ' ',
					(bridge_status & AMC_DS_VELOCITYERR) ? 'X' : ' ',
					(bridge_status & AMC_DS_POSVELOCITYLIM) ? 'X' : ' ',
					(bridge_status & AMC_DS_NEGVELOCITYLIM) ? 'X' : ' ',
					(bridge_status & AMC_DS_CMDPROFILER) ? 'X' : ' ');
			}
			break;
		case OPT_SETIFACE:
			{
				uint32_t interface_number;
				uint32_t interface_value;
				char *next_ptr;
				
				interface_number = strtol(optarg, &next_ptr, 10);
				interface_value = strtol(next_ptr + 1, NULL, 10);
				
				if (interface_number > 15) {
					printf("Interface number %d > 15\n", interface_number);
					return -1;
				}
				if (0 > amc_write_uint32(drv, 0x45, interface_number, interface_value)) {
					printf("Could not write to interface %d\n", interface_number);
					return -1;
				}
			}
			break;
		case OPT_GETIFACE:
			{				
				uint32_t interface_number;
				uint32_t interface_value;
				
				interface_number = strtol(optarg, NULL, 10);
				
				if (interface_number > 15) {
					printf("Interface number %d > 15\n", interface_number);
					return -1;
				}
				if (0 > amc_get_uint32(drv, 0x45, interface_number, &interface_value)) {
					printf("Could not read number %d\n", interface_number);
					return -1;
				}
				printf("Interface %2d = 0x%08X (%d)\n", interface_number, interface_value, interface_value);
			}
			break;
		case OPT_GETMOTORSTATUS:
			{
				int16_t current_demand, current_measured;
				if (0 > amc_get_uint16(drv, 0x10, 0x02, &current_demand)) {
					printf("Could not read motor current\n");
					return -1;
				}
				if (0 > amc_get_uint16(drv, 0x10, 0x03, &current_measured)) {
					printf("Could not read motor current\n");
					return -1;
				}
				printf("Current demand: %.2f, measured: %.2f\n", current_demand/SCALE_DC1, current_measured/SCALE_DC1);
				int32_t speed_measured;
				amc_get_uint32(drv, 0x11, 0x02, &speed_measured);
				printf("Speed: %.2f rpm (%d)\n", (speed_measured / SCALE_DS1) / COUNTS_PER_REV * 60.0, speed_measured);
			}
			break;
		case OPT_SETSPEED:
			{
				int32_t speed;
				speed = (atoi(optarg) * COUNTS_PER_REV / 60.0) * SCALE_DS1;
				amc_write_uint32(drv, 0x45, 0, speed);
			}
			break;
		case OPT_REG16:
			{
				uint16_t reg_num;
				uint16_t reg_val;
				char *next_ptr;
				
				reg_num = strtol(optarg, &next_ptr, 16);
				if (*next_ptr != 0) {
					reg_val = strtol(next_ptr + 1, NULL, 16);
					amc_write_uint16(drv, reg_num >> 8, reg_num & 0xFF, reg_val);
				}
				amc_get_uint16(drv, reg_num >> 8, reg_num & 0xFF, &reg_val);
				printf("Register %02X:%02X = %04X (%5d)\n", reg_num >> 8, reg_num & 0xFF, reg_val, reg_val);
			}
			break;
		case OPT_REG32:
			{
				uint16_t reg_num;
				uint32_t reg_val;
				char *next_ptr;
				
				reg_num = strtol(optarg, &next_ptr, 16);
				if (*next_ptr != 0) {
					reg_val = strtol(next_ptr + 1, NULL, 16);
					amc_write_uint32(drv, reg_num >> 8, reg_num & 0xFF, reg_val);
				}
				amc_get_uint32(drv, reg_num >> 8, reg_num & 0xFF, &reg_val);
				printf("Register %02X:%02X = %08X (%10d)\n", reg_num >> 8, reg_num & 0xFF, reg_val, reg_val);
			}
			break;
		case OPT_WDT:
			{
				uint16_t timeout_ms;
				if (optarg != NULL) {
					timeout_ms = strtol(optarg, NULL, 10);
					amc_write_uint16(drv, 0x04, 0x01, timeout_ms);	
				}
				amc_get_uint16(drv, 0x04, 0x01, &timeout_ms);
				printf("Watchdog timer timeout: %5d ms\n", timeout_ms);
			}
			break;
		default:
			opt_errors++;
			break;
		}
	}

	if (opt_errors) {
		puts(usage_string);
	}

	return 0;
}

