/* Syrup custom boot sound — u8 mono PCM @ 8 kHz in PY25Q16 @ 0x013000 */

#include "helper/boot_sound.h"

#include "audio.h"
#include "driver/keyboard.h"
#include "driver/py25q16.h"
#include "driver/systick.h"
#include "driver/system.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_gpio.h"

#define BOOT_SOUND_ADDR   0x013000u
#define BOOT_SOUND_PCM_MAX 40944u

bool BOOT_SOUND_PlayIfPresent(void)
{
	uint8_t hdr[16];
	uint8_t chunk[32];
	uint32_t left;
	uint32_t addr;

	PY25Q16_ReadBuffer(BOOT_SOUND_ADDR, hdr, 16);
	/* magic "SYRS" */
	if (hdr[0] != 0x53u || hdr[1] != 0x59u || hdr[2] != 0x52u || hdr[3] != 0x53u)
		return false;

	left = (uint32_t)hdr[8]
	     | ((uint32_t)hdr[9] << 8)
	     | ((uint32_t)hdr[10] << 16)
	     | ((uint32_t)hdr[11] << 24);
	if (left == 0u || left > BOOT_SOUND_PCM_MAX)
		return false;

	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_ANALOG);
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);
	/* EN1 | TEN1 | TSEL1 = software (111b) */
	DAC1->CR = DAC_CR_EN1 | DAC_CR_TEN1 | DAC_CR_TSEL1_0 | DAC_CR_TSEL1_1 | DAC_CR_TSEL1_2;
	SYSTICK_DelayUs(15);

	AUDIO_AudioPathOn();
	SYSTEM_DelayMs(20);

	addr = BOOT_SOUND_ADDR + 16u;
	while (left != 0u) {
		uint32_t n;
		uint32_t i;

		if (KEYBOARD_Poll() != KEY_INVALID)
			break;

		n = (left > 32u) ? 32u : left;
		PY25Q16_ReadBuffer(addr, chunk, (uint16_t)n);
		for (i = 0; i < n; i++) {
			DAC1->DHR8R1 = chunk[i];
			DAC1->SWTRIGR = DAC_SWTRIGR_SWTRIG1;
			SYSTICK_DelayUs(125);
		}
		addr += n;
		left -= n;
	}

	DAC1->DHR8R1 = 128u;
	DAC1->SWTRIGR = DAC_SWTRIGR_SWTRIG1;
	AUDIO_AudioPathOff();
	DAC1->CR = 0u;
	return true;
}
