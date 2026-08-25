#ifndef HELPER_BOOT_SOUND_H
#define HELPER_BOOT_SOUND_H

#include <stdbool.h>

/* Custom boot PCM @ SPI 0x013000 (see boot_sound.c). Returns true if custom played. */
bool BOOT_SOUND_PlayIfPresent(void);

#endif
