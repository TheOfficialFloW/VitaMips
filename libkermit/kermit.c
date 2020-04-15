#include <string.h>
#include "kermit.h"
#include "sha1.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))
#define sync()      __asm__ __volatile__("sync" ::: "memory")

struct kermit_interrupt {
  uint32_t flag;
  uint32_t pad;
};

static kermit_virtual_interrupt_handler g_virtual_interrupt_handlers[KERMIT_MAX_VIRTUAL_INTR_HANDLERS];

static inline void clear_dcache(void) {
  asm __volatile__(
    "mfc0    $t0, $16\n"
    "li      $t1, 2048\n"
    "ext     $t0, $t0, 6, 3\n"
    "sllv    $t1, $t1, $t0\n"
    "move    $t0, $zero\n"
    "1:cache 0x14, 0($t0)\n"
    "cache   0x14, 0($t0)\n"
    "addiu   $t0, $t0, 64\n"
    "bne     $t0, $t1, 1b\n"
    "nop\n"
    "sync\n"
    "nop\n"
    ::: "t0", "t1", "memory"
  );
}

static inline void kermit_raise_compat_interrupt_mask(uint32_t mask) {
  uint32_t val = REG32(0xBC300050);
  REG32(0xBC300050) = val & ~mask;
  REG32(0xBC300050) = val | mask;
}

static inline void kermit_wait_ready(void) {
  sync();
  REG32(0xBD000004) = 0xf;

  while ((REG32(0xBD000004)) != 0)
    ;
  while ((REG32(0xBD000000)) != 0xf)
    ;
}

static void kermit_special_request(void) {
  static const uint8_t special_request[64] = {
    0x59, 0xAD, 0x29, 0xD3, 0xE6, 0x62, 0x79, 0xF1,
    0xAF, 0x53, 0x2C, 0x62, 0x79, 0x92, 0xDE, 0xCB,
    0x56, 0xA8, 0xB9, 0x9C, 0x68, 0xA5, 0x09, 0x58,
    0x18, 0xF3, 0x52, 0xDC, 0x9B, 0xC7, 0xFB, 0x8F,
    0x3D, 0x43, 0x70, 0x7D, 0x2F, 0xBB, 0x72, 0x3C,
    0x12, 0x36, 0x0C, 0x8E, 0x81, 0xBE, 0x03, 0x1E,
    0x01, 0x2F, 0x20, 0xD2, 0x68, 0xDA, 0x7C, 0xCD,
    0x20, 0x21, 0xD5, 0x56, 0x0F, 0xE6, 0x27, 0x02
  };

  memcpy((void *)0xBFC00FC0, special_request, sizeof(special_request));

  REG32(0xABD78000) = 1;
  while (REG32(0xABD78000) != 2)
    ;

  REG32(0xABD78000) = 3;
  while (REG32(0xABD78000) != 0)
    ;

  memset((void *)0xBFC00FC0, 0, sizeof(special_request));
}

void kermit_init(void) {
  REG32(0xBC000000) = 0xcccccccc;
  REG32(0xBC000004) = 0xcccccccc;
  REG32(0xBC000008) = 0xcccccccc;
  REG32(0xBC00000c) = 0xcccccccc;
  REG32(0xBC000030) = 0;
  REG32(0xBC000034) = 0;
  REG32(0xBC000038) = 0;
  REG32(0xBC00003c) = 0;
  REG32(0xBC000040) = 0;
  REG32(0xBC000044) &= 0xffffff9f;
  REG32(0xBC100040) |= 2;
  sync();
  REG32(0xBC100050) |= 0x4080;

  kermit_special_request();
}

void kermit_register_virtual_interrupt_handler(enum kermit_virtual_interrupt id,
                                               kermit_virtual_interrupt_handler handler) {
  if (id < KERMIT_MAX_VIRTUAL_INTR_HANDLERS)
    g_virtual_interrupt_handlers[id] = handler;
}

void kermit_dispatch_virtual_interrupts(uint16_t interrupt_bits) {
  int i;
  struct kermit_interrupt *interrupt = (struct kermit_interrupt *)0xBFC008C0;

  for (i = 0; i < KERMIT_MAX_VIRTUAL_INTR_HANDLERS; i++) {
    if (interrupt_bits & (1 << i)) {
      if (g_virtual_interrupt_handlers[i])
          g_virtual_interrupt_handlers[i](i);

      interrupt[i].flag = 0;
    }
  }
}

void kermit_raise_compat_interrupt_id(uint32_t compat_interrupt_id) {
  uint32_t mask = 1 << compat_interrupt_id;

  kermit_wait_ready();

  REG32(0xBC300038) |= mask;
  kermit_raise_compat_interrupt_mask(mask);
}

void kermit_interrupts_enable(void) {
  REG32(0xBC300040) = 0xffffffff;
  REG32(0xBC300044) = 0;
  REG32(0xBC300048) = 0xffffffff;
  REG32(0xBC30004C) = 0;
  REG32(0xBC300050) = 0;
  REG32(0xBC300038) = (REG32(0xBC300038) & 0x4002) | 0xffff07f0;
}

void kermit_interrupts_disable(void) {
  REG32(0xBC300038) = REG32(0xBC300038) & 0x4002;
}

void kermit_suspend(kermit_resume_handler handler) {
  SHA1_CTX ctx;
  uint32_t sha1[5];
  static const uint8_t fake_mac[] = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0xde, 0xf0 };

  sha1_init(&ctx);
  sha1_update(&ctx, fake_mac, sizeof(fake_mac));
  sha1_final(&ctx, (uint8_t *)sha1);

  sha1[0] ^= REG32(0xBC100090);
  sha1[1] ^= REG32(0xBC100094);

  sha1_init(&ctx);
  sha1_update(&ctx, (uint8_t *)sha1, sizeof(sha1));
  sha1_final(&ctx, (uint8_t *)sha1);

  REG32(0xBFC001FC) = (uint32_t)handler ^ sha1[0] ^ sha1[1] ^ sha1[2] ^ sha1[3] ^ sha1[4];

  clear_dcache();

  kermit_wait_ready();
  REG32(0xABD78000) = 1;
  kermit_wait_ready();

  while (1)
    ;
}
