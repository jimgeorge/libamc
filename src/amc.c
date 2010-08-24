#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include <sys/uio.h>
#include <arpa/inet.h>

#include "serial.h"
#include "amc.h"
#include "crc.h"

/**
\brief Initialize a new AMC drive communications structure
\param *drv Pointer to AMC drive structure
\param *dev Name of serial device to use (eg: "/dev/ttyUSB0")
\param *spd Baud rate of the serial port
\param *address Address of the drive
\return 0 on success, -1 on failure

Initialize a new AMC drive structure, open the specified serial device at the
specified speed (with default settings for parity, number of bits, etc).
*/
int amc_drive_new(struct amc_drive *drv, char *dev, int spd, int address)
{
	if (-1 == serial_port_init(dev, spd, &drv->device)) return -1;

	drv->crc_table = amc_crc_mktable(AMC_CRC_POLY);
	drv->seq_ctr = 0;
	drv->address = address;
	drv->timeout_ms = AMC_DEFAULT_TIMEOUT_MS;
	return 0;
}

/**
\brief Write an AMC command packet to the drive
\param *drv AMC drive to write to
\param *cmd Command packet header to write
\param access_type One of AMC_CMDTYPE_* macros, defines read/write access type
\param response_len Expected response length, in bytes
\param *payload Payload to send as a part of command packet, can be NULL if payload_len is zero
\param payload_len Length of payload in bytes to send with this command packet.
Can be zero to indicate no payload
\return Number of bytes written on success, -1 on failure

This function computes CRC and initializes an AMC command header packet
and sends it to the drive. The index and offset fields are not
modified, and the control.bits.cmd must be properly set. The function
also sends the specified payload.

Enabling the debug flag causes every byte sent to be printed in box 
brackets.

This function makes use of writev to implement write combining and
minimize calls to the kernel.
*/
int amc_cmd_write(struct amc_drive *drv, struct amc_command *cmd, int access_type, 
	int response_len, uint16_t *payload, int payload_len)
{
	assert(drv != NULL);
	assert(cmd != NULL);
	
	/* Increment command sequence number */	
	drv->seq_ctr++;
	if (drv->seq_ctr >= 16) drv->seq_ctr = 0;
	
	if (drv->debug) {
		printf("write: seq = %d\n", drv->seq_ctr);
	}
	
	uint8_t *buffer = (uint8_t *)cmd;
	cmd->sof = AMC_SOF_BYTE;
	cmd->control.bits.seq = drv->seq_ctr;
	cmd->control.bits.rsvd = 0;
	cmd->addr = drv->address;
	cmd->control.bits.cmd = access_type;

	switch (access_type) {
	case AMC_CMDTYPE_READ:
		cmd->payload_len = response_len / sizeof(uint16_t);
		break;
	case AMC_CMDTYPE_WRITE:
	case AMC_CMDTYPE_READWRITE:
		cmd->payload_len = payload_len / sizeof(uint16_t);
		break;
	default:
		return -1;
	}

	/* Ensure that the CRC field is zero, since it is used as an accumulator */
	cmd->crc = 0;
	/* Number of words to check is one less than number of words in the header.
	This assumes that the CRC is 16-bits and is always stored at the end of the
	buffer */
	int ctr;
	int bytes_to_check = sizeof(struct amc_command) - sizeof(uint16_t);
	
	for (ctr = 0; ctr < bytes_to_check; ctr++) {
		amc_crc_check_word(*(buffer + ctr), &cmd->crc, drv->crc_table);
	}

	int bytes_to_write = sizeof(struct amc_command);
	/* CRC is always sent in big-endian (network) byte ordering, convert as necessary */	
	cmd->crc = htons(cmd->crc);
	
	/* Ensure that the CRC accumulator is zero */
	uint16_t payload_crc = 0;
	
	struct iovec iov[3];
	iov[0].iov_base = cmd;
	iov[0].iov_len = sizeof(struct amc_command);

	if (payload_len > 0) {
		bytes_to_check = payload_len;
		buffer = (uint8_t *)payload;
	
		for (ctr = 0; ctr < bytes_to_check; ctr++) {
			amc_crc_check_word(*(buffer + ctr), &payload_crc, drv->crc_table);
		}
		payload_crc = htons(payload_crc);	
		iov[1].iov_base = payload;
		iov[1].iov_len = payload_len;
		iov[2].iov_base = &payload_crc;
		iov[2].iov_len = sizeof(uint16_t);
	}

	if (drv->debug) {
		int ctr;
		buffer = (uint8_t *)iov[0].iov_base;
		for (ctr = 0; ctr < iov[0].iov_len; ctr++) {
			printf("[%02X]", *(buffer + ctr));
		}
		if (payload_len > 0) {
			buffer = (uint8_t *)iov[1].iov_base;
			for (ctr = 0; ctr < iov[1].iov_len; ctr++) {
				printf("[%02X]", *(buffer + ctr));
			}
			buffer = (uint8_t *)iov[2].iov_base;
			for (ctr = 0; ctr < iov[2].iov_len; ctr++) {
				printf("[%02X]", *(buffer + ctr));
			}
		}
		printf("\n");
	}
	
	bytes_to_write = 0;
	for (ctr = 0; ctr < ((payload_len > 0) ? 3 : 1); ctr++) {
		bytes_to_write += iov[ctr].iov_len;
	} 
	int bytes_written = writev(drv->device, iov, (payload_len > 0) ? 3 : 1);

	if (bytes_written != bytes_to_write) {
		return -1;
	}

	return bytes_written;
}

/**
\brief Read back a response from the drive
\param *drv AMC drive to read
\param *cmd Location to store data read back from drive
\param *payload Location to store paylaod read back from drive (if any)
\param payload_max_size Max. size in bytes of buffer pointed to by *payload
\return Number of bytes read on success, -1 on failure

This function reads back a response from the drive. It first reads in a
response header, then uses the contents of the response header to determine
how many words (if any) exist in the payload and reads these words back.
If any reads time out, an error is returned back to the caller.
If either the response header or the payload CRCs do not match, an error
is returned back to the caller.

Enabling the debug flag causes every byte received to be printed out in
angle brackets, and errors to be printed out.
*/
int amc_resp_read(struct amc_drive *drv, struct amc_response *rsp, void *payload, int payload_max_size)
{
	uint8_t *rd_ptr;

	struct pollfd pfd;

	rd_ptr = (uint8_t *)rsp;
	int bytes_to_read = sizeof(struct amc_response);
	int total_bytes_read = 0;

	int ret;
	pfd.fd = drv->device;
	pfd.events = POLLIN;
	pfd.revents = 0;

	do {
		do {
			ret = poll(&pfd, 1, drv->timeout_ms);
		} while ((ret == -1) && (errno == EINTR));

		if (ret == 0) {
			if (drv->debug) {
				printf("Timed out reading response header\n");
			}
			return -1;
		}
	
		int bytes_read = read(drv->device, rd_ptr, bytes_to_read);
		rd_ptr += bytes_read;
		bytes_to_read -= bytes_read;
		total_bytes_read += bytes_read;
	} while (bytes_to_read > 0);
	
	if (drv->debug) {
		printf("read: seq = %d\n", (int)rsp->control.bits.seq);
	}
	
	uint8_t *buffer = (uint8_t *)rsp;
	int bytes_to_check = sizeof(struct amc_response) - sizeof(uint16_t);
	uint16_t crc, ctr;
	
	crc = 0;
	
	if (drv->debug) {
		for (ctr = 0; ctr < total_bytes_read; ctr++) {
			printf("<%02X>", *(buffer + ctr));
		}
	}
	for (ctr = 0; ctr < bytes_to_check; ctr++) {
		amc_crc_check_word(*(buffer + ctr), &crc, drv->crc_table);
	}
	
	/* Convert received CRC back to host byte ordering */
	if (crc != ntohs(rsp->crc)) {
		if (drv->debug) {
			printf("Header CRC failed (expected %04X, got %04X)\n", crc, ntohs(rsp->crc));
		}
		return -1;
	}
	
	if (rsp->status1 != AMC_CMDRESP_COMPLETE) {
		if (drv->debug) {
			switch (rsp->status1) {
			case AMC_CMDRESP_INCOMPLETE:
				printf("Command not completed\n");
				break;
			case AMC_CMDRESP_INVALID:
				printf("Invalid command\n");
				break;
			case AMC_CMDRESP_NOACCESS:
				printf("No access\n");
				break;
			case AMC_CMDRESP_FRAMEERR:
				printf("Frame error\n");
				break;
			}
		}
		return -1;
	}

	if (!(rsp->control.bits.cmd & 0x02)) {
		return total_bytes_read;
	}
	
	assert(payload != NULL);

	rd_ptr = (uint8_t *)payload;
	bytes_to_read = rsp->payload_len * sizeof(uint16_t);
	int payload_bytes_read = 0;

	do {
		do {
			ret = poll(&pfd, 1, drv->timeout_ms);
		} while ((ret == -1) && (errno == EINTR));

		if (ret == 0) {
			if (drv->debug) {
				printf("Timed out reading payload\n");
			}
			return -1;
		}
	
		int bytes_read = read(drv->device, rd_ptr, bytes_to_read);
		rd_ptr += bytes_read;
		bytes_to_read -= bytes_read;
		payload_bytes_read += bytes_read;

		if (payload_bytes_read > payload_max_size) {
			if (drv->debug) {
				printf("Payload received exceeds max size\n");
			}
			return -1;
		}
	} while (bytes_to_read > 0);
	total_bytes_read += payload_bytes_read;

	if (drv->debug) {
		buffer = (uint8_t *)payload;
		for (ctr = 0; ctr < payload_bytes_read; ctr++) {
			printf("<%02X>", *(buffer + ctr));
		}
	}

	uint16_t readback_crc = 0;
	rd_ptr = (uint8_t *)&readback_crc;
	bytes_to_read = sizeof(uint16_t);
	do {
		do {
			ret = poll(&pfd, 1, drv->timeout_ms);
		} while ((ret == -1) && (errno == EINTR));

		if (ret == 0) {
			if (drv->debug) {
				printf("Timed out reading payload CRC\n");
			}
			return -1;
		}
		int bytes_read = read(drv->device, rd_ptr, bytes_to_read);
		rd_ptr += bytes_read;
		bytes_to_read -= bytes_read;
		payload_bytes_read += bytes_read;
	} while (bytes_to_read > 0);
	
	if (drv->debug) {
		buffer = (uint8_t *)&readback_crc;
		for (ctr = 0; ctr < sizeof(uint16_t); ctr++) {
			printf("<%02X>", *(buffer + ctr));
		}
		printf("\n");
	}

	buffer = (uint8_t *)payload;
	bytes_to_check = rsp->payload_len * sizeof(uint16_t);
	crc = 0;

	for (ctr = 0; ctr < bytes_to_check; ctr++) {
		amc_crc_check_word(*(buffer + ctr), &crc, drv->crc_table);
	}
	
	/* Convert received CRC back to host byte ordering */
	if (crc != ntohs(readback_crc)) {
		if (drv->debug) {
			printf("CRC failed (expected %04X, got %04X)\n", crc, ntohs(readback_crc));
		}
		return -1;
	}

	return total_bytes_read;
}

/**
\brief Get a specified string from the specified address
\param *drv AMC drive to read from
\param index Index of the parameter
\param offset Offset of the parameter
\param *buffer Location to store the parameter read back
\param bufsize Size of the buffer pointed to by *buffer
\return 0 on success, -1 on failure
*/
int amc_get_string(struct amc_drive *drv, int index, int offset, void *buffer, int bufsize)
{
	assert (drv != NULL);
	assert (buffer != NULL);
	struct amc_command cmd;
	struct amc_response resp;

	cmd.index = index;
	cmd.offset = offset;

	if (0 > amc_cmd_write(drv, &cmd, AMC_CMDTYPE_READ, bufsize, NULL, 0)) {
		if (drv->debug) {
			printf("Could not write command\n");
		}
		return -1;
	}
	
	if (0 > amc_resp_read(drv, &resp, buffer, bufsize)) {
		if (drv->debug) {
			printf("Could not read back data\n");
		}
		return -1;
	}
	return 0;
}

/**
\brief Get a specified 16-bit parameter from the specified address
\param *drv AMC drive to gain access to
\param index Index of the parameter
\param offset Offset of the parameter
\param *buffer Location to store the parameter read back (must be at least 4 bytes)
\return 0 on success, -1 on failure
*/
int amc_get_uint16(struct amc_drive *drv, int index, int offset, uint16_t *buffer)
{
        return amc_get_string(drv, index, offset, buffer, sizeof(uint16_t));
}

/**
\brief Get a specified 32-bit parameter from the specified address
\param *drv AMC drive to gain access to
\param index Index of the parameter
\param offset Offset of the parameter
\param *buffer Location to store the parameter read back (must be at least 4 bytes)
\return 0 on success, -1 on failure
*/
int amc_get_uint32(struct amc_drive *drv, int index, int offset, uint32_t *buffer)
{
	return amc_get_string(drv, index, offset, buffer, sizeof(uint32_t));
}

/**
\brief Write a specified string to a given address
\param *drv AMC drive to write to
\param index Index of the parameter
\param offset Offset of the parameter
\param *buffer Location of the parameter to write
\param bufsize Size of the parameter string
\return 0 on success, -1 on failure
*/
int amc_write_string(struct amc_drive *drv, int index, int offset, void *buffer, int bufsize)
{
	assert (drv != NULL);
	assert (buffer != NULL);
	struct amc_command cmd;
	struct amc_response resp;

	cmd.index = index;
	cmd.offset = offset;

	if (0 > amc_cmd_write(drv, &cmd, AMC_CMDTYPE_WRITE, 0, buffer, bufsize)) {
		if (drv->debug) {
			printf("Could not write command\n");
		}
		return -1;
	}
	if (0 > amc_resp_read(drv, &resp, NULL, 0)) {
		if (drv->debug) {
			printf("Could not read response\n");
		}
		return -1;
	}
	return 0;
}

/**
\brief Write a 16-bit value to the specified address
\param *drv AMC drive to write to
\param index Index of the parameter
\param offset Offset of the parameter
\param value The value to write
\return 0 on success, -1 on failure
*/
int amc_write_uint16(struct amc_drive *drv, int index, int offset, uint16_t value)
{
	return amc_write_string(drv, index, offset, &value, sizeof(uint16_t));
}

/**
\brief Write a 32-bit value to the specified address
\param *drv AMC drive to write to
\param index Index of the parameter
\param offset Offset of the parameter
\param value The value to write
\return 0 on success, -1 on failure
*/
int amc_write_uint32(struct amc_drive *drv, int index, int offset, uint32_t value)
{
	return amc_write_string(drv, index, offset, &value, sizeof(uint32_t));
}

/**
\brief Get write access to all registers on the specified drive
\param *drv AMC drive to gain access to
\return 0 on success, -1 on failure
*/
int amc_get_access_control(struct amc_drive *drv)
{
	return amc_write_uint16(drv, 0x07, 0x00, amc_int16_to_le(0x000E)); 
}


/**
\brief Read back product information
\param *drv AMC drive to gain access to
\param *buffer Location to store product info read back
\return 0 on success, -1 on failure
*/
int amc_get_product_info(struct amc_drive *drv, struct amc_product_info *pi)
{
	return amc_get_string(drv, 0x8C, 0, pi, sizeof(struct amc_product_info));
}

/**
\brief Read back command parameter register
\param *drv AMC drive to gain access to
\param param Parameter number (ranges from 0 to 15)
\param *buffer Location to store the parameter read back (must be at least 4 bytes)
\return 0 on success, -1 on failure
*/
int amc_get_command_param(struct amc_drive *drv, unsigned int param, uint32_t *buffer)
{
	return amc_get_uint32(drv, 0x45, param, buffer);
}

