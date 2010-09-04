#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
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
};

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

	return 0;
}

char *usage_string = 
"Retrieve/control modbus registers on the WiBEX angle converter board\n"
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

