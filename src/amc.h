#ifndef _AMC_H_
#define _AMC_H_

#include <stdint.h>

#define AMC_SOF_BYTE 0xA5
#define AMC_CRC_POLY 0x1021

#define AMC_DEFAULT_TIMEOUT_MS 1000

#define AMCSTRUCT #pragma pack(1)\
struct

#pragma pack(1)

#define AMC_CMDTYPE_READ 1
#define AMC_CMDTYPE_WRITE 2
#define AMC_CMDTYPE_READWRITE 3

#define AMC_CMDRESP_COMPLETE 1
#define AMC_CMDRESP_INCOMPLETE 2
#define AMC_CMDRESP_INVALID 3
#define AMC_CMDRESP_NOACCESS 6
#define AMC_CMDRESP_FRAMEERR 8

/* TODO: make these macros aware of machine endianness */
#define amc_int16_to_le(x) x
#define amc_int16_from_le(x) x
#define amc_int32_to_le(x) x
#define amc_int32_from_le(x) x


/* Bridge control bits */
#define AMC_BC_INHIBIT (1 << 0)
#define AMC_BC_BRAKE (1 << 1)
#define AMC_BC_QUICKSTOP (1 << 6)
#define AMC_BC_RESETEVENTS (1 << 12)

/* Bridge status bits */
#define AMC_BS_ENABLED (1 << 0)
#define AMC_BS_DYNBRAKE (1 << 1)
#define AMC_BS_SHUNT (1 << 2)
#define AMC_BS_POSSTOP (1 << 3)
#define AMC_BS_NEGSTOP (1 << 4)
#define AMC_BS_POSTORQUEINH (1 << 5)
#define AMC_BS_NEGTORQUEINH (1 << 6)
#define AMC_BS_EXTBRAKE (1 << 7)

/* Drive protection status */
#define AMC_PS_RESET (1 << 0)
#define AMC_PS_INTERROR (1 << 1)
#define AMC_PS_SHORTCKT (1 << 2)
#define AMC_PS_OVERCURRENT (1 << 3)
#define AMC_PS_UNDERVOLTAGE (1 << 4)
#define AMC_PS_OVERVOLTAGE (1 << 5)
#define AMC_PS_OVERTEMP (1 << 6)

/* System protection status */
#define AMC_SS_RESTOREERR (1 << 0)
#define AMC_SS_STOREERR (1 << 1)
#define AMC_SS_MOTOROVERTEMP (1 << 4)
#define AMC_SS_FEEDBACKERROR (1 << 6)
#define AMC_SS_OVERSPEED (1 << 7)
#define AMC_SS_COMMERR (1 << 10)

/* Drive System status 1 */
#define AMC_DS_LOGMISSED (1 << 0)
#define AMC_DS_CMDINHIBIT (1 << 1)
#define AMC_DS_USERINHIBIT (1 << 2)
#define AMC_DS_POSINH (1 << 3)
#define AMC_DS_NEGINH (1 << 4)
#define AMC_DS_CURRENTLIM (1 << 5)
#define AMC_DS_CONTCURRENT (1 << 6)
#define AMC_DS_CLSAT (1 << 7)
#define AMC_DS_CMDDYNBRAKE (1 << 12)
#define AMC_DS_USERDYNBRAKE (1 << 13)
#define AMC_DS_SHUNTREG (1 << 14)

/* Drive System status 2 */
#define AMC_DS_ZEROVEL (1 << 0)
#define AMC_DS_ATCMD (1 << 1)
#define AMC_DS_VELOCITYERR (1 << 2)
#define AMC_DS_POSVELOCITYLIM (1 << 3)
#define AMC_DS_NEGVELOCITYLIM (1 << 4)
#define AMC_DS_CMDPROFILER (1 << 5)

union control {
	uint8_t byte;
	struct {
		uint8_t cmd : 2; /**< Command type */
		uint8_t seq : 4; /**< Sequence number, rolls over at 0x0F */
		uint8_t rsvd : 2;
	} bits;
};

/**
\brief Command packet sent to AMC drives

The destination address is set to 0x00 for broadcast, 0x01 - 0x3F are valid
destination addresses, and 0xFF is only valid for slave-to-master messages.

The command type is 0 = not used, 1 = message contains no data but the
response will contain the number of words specified in payload_len, 
2 = message contains number of words specified in payload_len but the response
will not contain any data, 3 = message and response both contain payloads of
length payload_len.
*/
struct amc_command {
	uint8_t sof; /**< Start of Frame (always set to 0xA5) */
	uint8_t addr; /**< Destination address, 00 = broadcast, 01-3F are valid, FF = Master */
	union control control; /**< Control byte */
	uint8_t index; /**< Index into the parameter array within a drive */
	uint8_t offset; /**< Offset within the parameter array addressed by index */
	uint8_t payload_len; /**< Payload length in words */
	uint16_t crc; /**< CRC of the header */
};

/**
\brief Response packet sent to AMC drives

*/
struct amc_response {
	uint8_t sof; /**< Start of Frame (always set to 0xA5) */
	uint8_t addr; /**< Destination address, 00 = broadcast, 01-3F are valid, FF = Master */
	union control control; /**< Control byte */
	uint8_t status1; /**< First status byte */
	uint8_t status2; /**< Second status byte */
	uint8_t payload_len; /**< Payload length in words */
	uint16_t crc; /**< CRC of the header */
};

struct amc_drive {
	uint8_t seq_ctr; /**< Message sequenc counter */
	uint16_t *crc_table; /**< Cached CRC table */
	int device; /**< Device number for the communications port */
	int address; /**< Device address */
	int timeout_ms; /**< Read timeout in milliseconds */
	int debug; /**< Debug flag, set to 1 to enable debug messages */
};

struct amc_product_info {
	uint8_t rsvd1[2];
	uint8_t control_board_name[32];
	uint8_t control_board_version[32];
	uint8_t control_board_serial[32];
	uint8_t control_board_build_date[32];
	uint8_t control_board_build_time[32];
	uint8_t rsvd2[30];
	uint8_t product_part_number[32];
	uint8_t product_version[32];
	uint8_t product_serial_number[32];
	uint8_t product_build_date[32];
	uint8_t product_build_time[32];
};

int amc_drive_new(struct amc_drive *drv, char *dev, int spd, int address);
int amc_cmd_write(struct amc_drive *drv, struct amc_command *cmd, int access_type, 
	int response_len, uint16_t *payload, int payload_len);
int amc_resp_read(struct amc_drive *drv, struct amc_response *rsp, void *payload, int payload_max_size);

int amc_get_string(struct amc_drive *drv, int index, int offset, void *buffer, int bufsize);
int amc_get_uint16(struct amc_drive *drv, int index, int offset, uint16_t *buffer);
int amc_get_uint32(struct amc_drive *drv, int index, int offset, uint32_t *buffer);

int amc_write_string(struct amc_drive *drv, int index, int offset, void *buffer, int bufsize);
int amc_write_uint16(struct amc_drive *drv, int index, int offset, uint16_t value);
int amc_write_uint32(struct amc_drive *drv, int index, int offset, uint32_t value);

int amc_get_access_control(struct amc_drive *drv);
int amc_get_product_info(struct amc_drive *drv, struct amc_product_info *pi);
int amc_get_command_param(struct amc_drive *drv, unsigned int param, uint32_t *buffer);

#endif /* _AMC_H_ */

