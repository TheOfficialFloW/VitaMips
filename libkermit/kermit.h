#ifndef KERMIT_H
#define KERMIT_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KERMIT_MAX_VIRTUAL_INTR_HANDLERS	14

enum kermit_virtual_interrupt {
	KERMIT_VIRTUAL_INTR_NONE,
	KERMIT_VIRTUAL_INTR_AUDIO_CH1,
	KERMIT_VIRTUAL_INTR_AUDIO_CH2,
	KERMIT_VIRTUAL_INTR_AUDIO_CH3,
	KERMIT_VIRTUAL_INTR_ME_DMA_CH1,
	KERMIT_VIRTUAL_INTR_ME_DMA_CH2,
	KERMIT_VIRTUAL_INTR_ME_DMA_CH3,
	KERMIT_VIRTUAL_INTR_WLAN_CH1,
	KERMIT_VIRTUAL_INTR_WLAN_CH2,
	KERMIT_VIRTUAL_INTR_IMPOSE_CH1,
	KERMIT_VIRTUAL_INTR_POWER_CH1,
	KERMIT_VIRTUAL_INTR_UNKNOWN_CH1,
	KERMIT_VIRTUAL_INTR_USBGPS_CH1,
	KERMIT_VIRTUAL_INTR_USBPSPCM_CH1,
};

typedef void (*kermit_virtual_interrupt_handler)(enum kermit_virtual_interrupt id);
typedef void (*kermit_resume_handler)(void *context);

void kermit_init(void);
void kermit_register_virtual_interrupt_handler(enum kermit_virtual_interrupt id,
                                               kermit_virtual_interrupt_handler handler);
void kermit_dispatch_virtual_interrupts(uint16_t interrupt_bits);
void kermit_raise_compat_interrupt(uint32_t compat_interrupt_id);
void kermit_interrupts_enable(void);
void kermit_interrupts_disable(void);
void kermit_suspend(kermit_resume_handler handler) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
