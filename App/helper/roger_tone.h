#ifndef HELPER_ROGER_TONE_H
#define HELPER_ROGER_TONE_H

#include <stdint.h>

/* Custom Roger tone notes @ SPI 0x01D000 (see roger_tone.c). */
#define ROGER_TONE_MAX_NOTES 128u

/* Null-terminated (0,0) pair sequence for BK4819_PlaySequence, or NULL if empty. */
const uint16_t *ROGER_TONE_Sequence(void);

#endif
