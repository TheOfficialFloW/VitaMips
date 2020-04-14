#include <pspsdk.h>
#include <string.h>

#include "sha1.h"

#define REG32(ADDR) (*(volatile u32 *)(ADDR))

#define MAX_INTR_HANDLERS 14

typedef struct {
  u32 flag;
  u32 pad;
} SceKermitInterrupt;

static void (* g_virtual_intr_handlers[MAX_INTR_HANDLERS])(void);

void ClearIcache(void) {
  __asm__ volatile ("\
    .word 0x40088000;\
    .word 0x24091000;\
    .word 0x7D081240;\
    .word 0x01094804;\
    .word 0x4080E000;\
    .word 0x4080E800;\
    .word 0x00004021;\
    .word 0xBD010000;\
    .word 0xBD030000;\
    .word 0x25080040;\
    .word 0x1509FFFC;\
    .word 0x00000000;\
  "::);
}

void ClearDcache(void) {
  __asm__ volatile ("\
    .word 0x40088000;\
    .word 0x24090800;\
    .word 0x7D081180;\
    .word 0x01094804;\
    .word 0x00004021;\
    .word 0xBD140000;\
    .word 0xBD140000;\
    .word 0x25080040;\
    .word 0x1509FFFC;\
    .word 0x00000000;\
    .word 0x0000000F;\
    .word 0x00000000;\
  "::);
}

inline static void raiseCompatInterrupt(u32 val) {
  u32 old_val = REG32(0xBC300050);
  REG32(0xBC300050) = old_val & ~val;
  REG32(0xBC300050) = old_val | val;
}

static void sceKermitWaitReady(void) {
  __asm__ volatile ("sync");

  REG32(0xBD000004) = 0xf;

  while ((REG32(0xBD000004)) != 0);
  while ((REG32(0xBD000000)) != 0xf);
}

static int sceKermitSystemEventHandler(int ev_id) {
  switch (ev_id) {
    case 0x4000:  // suspend
      REG32(0xBC300038) = REG32(0xBC300038) & 0x4002;
      break;

    case 0x10000:  // resume
      REG32(0xBC300040) = 0xffffffff;
      REG32(0xBC300044) = 0;
      REG32(0xBC300048) = 0xffffffff;
      REG32(0xBC30004C) = 0;
      REG32(0xBC300050) = 0;
      REG32(0xBC300038) = (REG32(0xBC300038) & 0x4002) | 0xffff07f0;
      break;

    default:
      return 0;
  }

  return 0;
}

static void handleVirtualInterrupt(u16 bits) {
  SceKermitInterrupt *interrupt;
  int i;

  for (i = 0; i < 16; i++) {
    if (bits & (1 << i)) {
      if (g_virtual_intr_handlers[i])
        g_virtual_intr_handlers[i]();

      interrupt = (SceKermitInterrupt *)0xBFC008C0;
      interrupt[i].flag = 0;
    }
  }
}

static int sceKermitInterruptHandler(void) {
  u16 high_bits;
  u32 bits;

  bits = REG32(0xBC300030) & ~0x4002;
  REG32(0xBC300030) = bits;
  __asm__ volatile ("sync");

  high_bits = bits >> 16;
  if (high_bits != 0)
    handleVirtualInterrupt(high_bits);

  return -1;
}

int sceKermitRegisterVirtualIntrHandler(u32 intr_code, void (* handler)(void)) {
  if (intr_code < MAX_INTR_HANDLERS)
    g_virtual_intr_handlers[intr_code] = handler;

  return 0;
}

int sceKermitInterrupt(u32 intr_code) {
  u32 val;

  sceKermitWaitReady();

  val = 1 << intr_code;
  REG32(0xBC300038) |= val;
  raiseCompatInterrupt(val);

  return 0;
}

static int sceKermitIntrInit(void) {
  REG32(0xBC300040) = 0xffffffff;
  REG32(0xBC300044) = 0;
  REG32(0xBC300048) = 0xffffffff;
  REG32(0xBC30004C) = 0;
  REG32(0xBC300050) = 0;
  REG32(0xBC300038) = (REG32(0xBC300038) & 0x4002) | 0xffff07f0;

  return 0;
}

static u8 special_request[0x40] = {
  0x59, 0xAD, 0x29, 0xD3, 0xE6, 0x62, 0x79, 0xF1,
  0xAF, 0x53, 0x2C, 0x62, 0x79, 0x92, 0xDE, 0xCB,
  0x56, 0xA8, 0xB9, 0x9C, 0x68, 0xA5, 0x09, 0x58,
  0x18, 0xF3, 0x52, 0xDC, 0x9B, 0xC7, 0xFB, 0x8F,
  0x3D, 0x43, 0x70, 0x7D, 0x2F, 0xBB, 0x72, 0x3C,
  0x12, 0x36, 0x0C, 0x8E, 0x81, 0xBE, 0x03, 0x1E,
  0x01, 0x2F, 0x20, 0xD2, 0x68, 0xDA, 0x7C, 0xCD,
  0x20, 0x21, 0xD5, 0x56, 0x0F, 0xE6, 0x27, 0x02
};

static int sceKermitSpecialRequest(void) {
  memcpy((void *)0xBFC00FC0, special_request, sizeof(special_request));

  REG32(0xABD78000) = 1;
  while (REG32(0xABD78000) != 2);
  REG32(0xABD78000) = 3;
  while (REG32(0xABD78000) != 0);

  memset((void *)0xBFC00FC0, 0, sizeof(special_request));

  return 0;
}

static void interrupt_loop(void) {
  while (1) {
    sceKermitInterruptHandler();
    REG32(0xA8000000)++;
  }
}

static void interrupt_handler(void) {
  REG32(0xA8000004)++;
}

static void power_suspend_handler(void);

static void power_resume_handler(void *param) {
  sceKermitSystemEventHandler(0x10000);
  // TODO: restore registers properly
  interrupt_loop();
}

static void power_suspend_handler(void) {
  SHA1_CTX ctx;
  u32 sha1[5], sha2[5];
  u32 fptr;

  u8 fake_mac[] = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0xde, 0xf0 };

  sceKermitSystemEventHandler(0x4000);

  sha1_init(&ctx);
  sha1_update(&ctx, (u8 *)fake_mac, sizeof(fake_mac));
  sha1_final(&ctx, (u8 *)sha1);

  sha1[0] ^= REG32(0xBC100090);
  sha1[1] ^= REG32(0xBC100094);

  sha1_init(&ctx);
  sha1_update(&ctx, (u8 *)sha1, sizeof(sha1));
  sha1_final(&ctx, (u8 *)sha2);

  fptr = (u32)power_resume_handler;
  REG32(0xBFC001FC) = fptr ^ sha2[0] ^ sha2[1] ^ sha2[2] ^ sha2[3] ^ sha2[4];

  ClearDcache();

  sceKermitWaitReady();
  REG32(0xABD78000) = 1;
  sceKermitWaitReady();
  while (1);
}

int main(void) {
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
  __asm__ volatile ("sync");
  REG32(0xBC100050) |= 0x4080;

  sceKermitSpecialRequest();

  sceKermitIntrInit();
  sceKermitRegisterVirtualIntrHandler(1, interrupt_handler);
  sceKermitRegisterVirtualIntrHandler(10, power_suspend_handler);

  interrupt_loop();

  return 0;
}
