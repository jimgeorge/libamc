#ifndef _CRC_H_
#define _CRC_H_

#include <stdint.h>

void amc_crc_check_word(uint16_t data, uint16_t *accumulator, uint16_t *crc_table);
unsigned short *amc_crc_mktable(uint16_t poly);

#endif /* _CRC_H_ */

