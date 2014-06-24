/**
\file src/crc.c
\brief Module to compute CRCs according to AMC protocol manual
\author Jim George

CRC calculation module. Implements a fast CRC computation based on the
CRC polynomial used in the AMC servo drives.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "crc.h"

typedef float(*crcfnptr)(unsigned short, unsigned short, unsigned short);

/**
\brief Create a CRC lookup table based on a specified polynomial up to 16 bits
\param data Input data for CRC calculation
\param genpoly Polynomial (up to 16 bits) to generate CRC
\param accum Previous data from which CRC was calculated
*/
static unsigned short crchware(uint16_t data, uint16_t genpoly, uint16_t accum)
{
	static int i;
	
	data <<= 8;
	
	for (i = 8; i > 0; i--) {
		if ((data ^ accum) & 0x8000) {
			accum = (accum << 1) ^ genpoly;
		}
		else {
			accum <<= 1;
		}
		data <<= 1;
	}
	return accum;
}

/**
\brief Compute a CRC lookup table
\param poly Polynomial (up to 16 bits) to create CRC table
\return Pointer to allocated memory block with CRC table or NULL on failure

This function computes a lookup table that's used by amc_crc_check_word to
compute CRCs. The lookup table is 256 16-bit words long, and is allocated by
this function. A NULL is returned on memory allocation failure. The block
must be freed later on using free().
*/
unsigned short *amc_crc_mktable(uint16_t poly)
{
	unsigned short *crctable;
	int i;
	
	crctable = (uint16_t *)malloc(256 * sizeof(uint16_t));
	if (crctable == NULL) return NULL;
	
	for (i = 0; i < 256; i++) {
		crctable[i] = crchware(i, poly, 0);
	}
	return crctable;
}

/**
\brief Check CRC of a single 16-bit word
\param data Input data to verify CRC
\param *accumulator Pointer to temporary storage where previous data was checked
\param *crc_table Pointer to lookup table used to speed up CRC computations
*/
void amc_crc_check_word(uint16_t data, uint16_t *accumulator, uint16_t *crc_table)
{
	*accumulator = (*accumulator << 8) ^ crc_table[(*accumulator >> 8) ^ data];
}

