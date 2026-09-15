/* Syrup custom Roger tone — note pairs (freq_hz, duration_ms) in PY25Q16 @ 0x01D000
 *
 *   0x00  4  Magic "SYRT"
 *   0x04  1  Version = 1
 *   0x05  1  Flags
 *   0x06  2  Note count N (LE uint16), 1..128
 *   0x08  8  Reserved
 *   0x10  4*N  Notes: uint16 freq_hz LE, uint16 duration_ms LE
 *
 * Values are clamped by the web flasher before write.
 */

#include "helper/roger_tone.h"

#include <stddef.h>
#include "driver/py25q16.h"

#define ROGER_TONE_ADDR 0x01D000u

static uint16_t s_seq[(ROGER_TONE_MAX_NOTES + 1u) * 2u];

const uint16_t *ROGER_TONE_Sequence(void)
{
	uint8_t  hdr[8];
	uint8_t  raw[4];
	uint16_t count;
	uint16_t i;

	PY25Q16_ReadBuffer(ROGER_TONE_ADDR, hdr, 8);
	if (hdr[0] != 'S' || hdr[1] != 'Y' || hdr[2] != 'R' || hdr[3] != 'T')
		return NULL;
	if (hdr[4] != 1u)
		return NULL;

	count = (uint16_t)hdr[6] | ((uint16_t)hdr[7] << 8);
	if (count == 0u || count > ROGER_TONE_MAX_NOTES)
		return NULL;

	for (i = 0; i < count; i++) {
		PY25Q16_ReadBuffer(ROGER_TONE_ADDR + 16u + (uint32_t)i * 4u, raw, 4);
		s_seq[i * 2u]     = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
		s_seq[i * 2u + 1u] = (uint16_t)raw[2] | ((uint16_t)raw[3] << 8);
	}

	s_seq[count * 2u]     = 0u;
	s_seq[count * 2u + 1u] = 0u;
	return s_seq;
}
