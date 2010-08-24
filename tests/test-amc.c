#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include "src/amc.h"

char serial_device[256] = "/dev/ttyUSB2";
int baudrate = 115200;

enum {
	OPT_GETID = 256,
	OPT_DEBUG,
	OPT_ENABLEBRIDGE,
	OPT_QUICKSTOP,
	OPT_RESETEVENTS,
	OPT_BRIDGESTATUS,
	OPT_GETIFACE,
	OPT_SETIFACE,
};

char *usage_string = 
"Retrieve/control modbus registers on the WiBEX angle converter board\n"
"Usage:\n"
"--getid: Retrieve drive ID string and version numbers\n"
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

	if (argc == 1) {
		puts(usage_string);
		return -1;
	}
	
	drv = malloc(sizeof(struct amc_drive));

	if (0 != amc_drive_new(drv, serial_device, baudrate, 0x3F)) {
		printf("Could not open serial device %s\n", serial_device);
		return -1;
	}
	
	if (0 > amc_get_access_control(drv)) {
		printf("Could not get access to drive\n");
	}

	while (-1 != (opt = getopt_long(argc, argv, "dv", opt_lst, &opt_idx))) {
		switch(opt) {
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
				printf("Bridge status: 0x%04X, Bridge: %s, DynBrake: %s, Shunt: %s\n"
					"\tPosStop: %s, NegStop: %s, PosTorqueInhibit: %s, NegTorqueInhibit: %s\n"
					"\tExtBrake: %s\n",
					bridge_status,
					(bridge_status & AMC_BS_ENABLED) ? "Active" : "Disabled",
					(bridge_status & AMC_BS_DYNBRAKE) ? "Enabled" : "Disabled",
					(bridge_status & AMC_BS_SHUNT) ? "Enabled" : "Disabled",
					(bridge_status & AMC_BS_POSSTOP) ? "Enabled" : "Disabled",
					(bridge_status & AMC_BS_NEGSTOP) ? "Enabled" : "Disabled",
					(bridge_status & AMC_BS_POSTORQUEINH) ? "Enabled" : "Disabled",
					(bridge_status & AMC_BS_NEGTORQUEINH) ? "Enabled" : "Disabled",
					(bridge_status & AMC_BS_EXTBRAKE) ? "Active" : "Inactive");

				if (0 > amc_get_uint16(drv, 0x02, 0x01, &bridge_status)) {
					printf("Could not read drive protection status\n");
					return -1;
				}
				printf("Drive protection status: 0x%04X, Reset: %s, Internal Error: %s, Short Circuit: %s\n"
					"\tOvercurrent: %s, Undervoltage: %s, Overvoltage: %s, Overtemp: %s\n",
					bridge_status,
					(bridge_status & AMC_PS_RESET) ? "Yes" : "No",
					(bridge_status & AMC_PS_INTERROR) ? "Yes" : "No",
					(bridge_status & AMC_PS_SHORTCKT) ? "Yes" : "No",
					(bridge_status & AMC_PS_OVERCURRENT) ? "Yes" : "No",
					(bridge_status & AMC_PS_UNDERVOLTAGE) ? "Yes" : "No",
					(bridge_status & AMC_PS_OVERVOLTAGE) ? "Yes" : "No",
					(bridge_status & AMC_PS_OVERTEMP) ? "Yes" : "No");

				if (0 > amc_get_uint16(drv, 0x02, 0x02, &bridge_status)) {
					printf("Could not read system protection status\n");
					return -1;
				}
				printf("System protection status: 0x%04X, Parameter Restore Error: %s, Parameter Store Error: %s\n"
					"\tMotor Overtemp: %s, Feedback Error: %s, Overspeed: %s, Communications Error: %s\n",
					bridge_status,
					(bridge_status & AMC_SS_RESTOREERR) ? "Yes" : "No",
					(bridge_status & AMC_SS_STOREERR) ? "Yes" : "No",
					(bridge_status & AMC_SS_MOTOROVERTEMP) ? "Yes" : "No",
					(bridge_status & AMC_SS_FEEDBACKERROR) ? "Yes" : "No",
					(bridge_status & AMC_SS_OVERSPEED) ? "Yes" : "No",
					(bridge_status & AMC_SS_COMMERR) ? "Yes" : "No");

				if (0 > amc_get_uint16(drv, 0x02, 0x03, &bridge_status)) {
					printf("Could not read drive status 1\n");
					return -1;
				}
				printf("Drive status 1: 0x%04X, Log Missed: %s, Commanded Inhibit: %s, User Inhibit: %s\n"
					"\tPositive Inhibit: %s, Negative Inhibit: %s, Current Limit: %s, Continuous Current Limit: %s\n"
					"\tCurrent Loop Saturated: %s, Commanded Dynamic Brake: %s, User Dynamic Brake: %s, Shunt Reg: %s\n",
					bridge_status,
					(bridge_status & AMC_DS_LOGMISSED) ? "Yes" : "No",
					(bridge_status & AMC_DS_CMDINHIBIT) ? "Yes" : "No",
					(bridge_status & AMC_DS_USERINHIBIT) ? "Yes" : "No",
					(bridge_status & AMC_DS_POSINH) ? "Yes" : "No",
					(bridge_status & AMC_DS_NEGINH) ? "Yes" : "No",
					(bridge_status & AMC_DS_CURRENTLIM) ? "Yes" : "No",
					(bridge_status & AMC_DS_CONTCURRENT) ? "Yes" : "No",
					(bridge_status & AMC_DS_CLSAT) ? "Yes" : "No",
					(bridge_status & AMC_DS_CMDDYNBRAKE) ? "Yes" : "No",
					(bridge_status & AMC_DS_USERDYNBRAKE) ? "Yes" : "No",
					(bridge_status & AMC_DS_SHUNTREG) ? "Active" : "Inactive");

				if (0 > amc_get_uint16(drv, 0x02, 0x04, &bridge_status)) {
					printf("Could not read drive status 1\n");
					return -1;
				}
				printf("Drive status 2: 0x%04X, Zero Velocity: %s, At Command: %s, Velocity Following Error: %s\n"
					"\tPositive Velocity Limit: %s, Negative Velocity Limit: %s, Command Profiler: %s\n",
					bridge_status,
					(bridge_status & AMC_DS_ZEROVEL) ? "Yes" : "No",
					(bridge_status & AMC_DS_ATCMD) ? "Yes" : "No",
					(bridge_status & AMC_DS_VELOCITYERR) ? "Yes" : "No",
					(bridge_status & AMC_DS_POSVELOCITYLIM) ? "Yes" : "No",
					(bridge_status & AMC_DS_NEGVELOCITYLIM) ? "Yes" : "No",
					(bridge_status & AMC_DS_CMDPROFILER) ? "Active" : "Inactive");
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

