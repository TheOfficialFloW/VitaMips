#include <pspsdk.h>
#include <string.h>

#define REG32(ADDR) (*(volatile u32 *)(ADDR))

#define ALIGN(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

#define MAX_INTR_HANDLERS 14

typedef struct {
  u32 unk_0;
  u32 unk_4;
} SceKermitInterrupt;

static void (* g_virtual_intr_handlers[MAX_INTR_HANDLERS])(void);

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

static void handleVirtualInterrupt(u16 bits) {
  SceKermitInterrupt *interrupt;
  int i;

  for (i = 0; i < 16; i++) {
    if (bits & (1 << i)) {
      if (g_virtual_intr_handlers[i])
        g_virtual_intr_handlers[i]();

      interrupt = (SceKermitInterrupt *)0xBFC008C0;
      interrupt[i].unk_0 = 0;
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

  *(volatile u32 *)0xABD78000 = 1;
  while (*(volatile u32 *)0xABD78000 != 2);
  *(volatile u32 *)0xABD78000 = 3;
  while (*(volatile u32 *)0xABD78000 != 0);

  memset((void *)0xBFC00FC0, 0, sizeof(special_request));

  return 0;
}

static int interrupt_handler(void) {
  (*(volatile u32 *)0xA8000004)++;
  return 0;
}

static int running = 0;

static int exit_handler(void) {
  REG32(0xBC300038) = REG32(0xBC300038) & 0x4002;
  running = 0;
  return 0;
}

int main(void) {
  *(volatile u32 *)0xBC000000 = 0xcccccccc;
  *(volatile u32 *)0xBC000004 = 0xcccccccc;
  *(volatile u32 *)0xBC000008 = 0xcccccccc;
  *(volatile u32 *)0xBC00000c = 0xcccccccc;
  *(volatile u32 *)0xBC000030 = 0;
  *(volatile u32 *)0xBC000034 = 0;
  *(volatile u32 *)0xBC000038 = 0;
  *(volatile u32 *)0xBC00003c = 0;
  *(volatile u32 *)0xBC000040 = 0;
  *(volatile u32 *)0xBC000044 &= 0xffffff9f;
  *(volatile u32 *)0xBC100040 |= 2;
  __asm__ volatile ("sync");
  *(volatile u32 *)0xBC100050 |= 0x4080;

  sceKermitSpecialRequest();

  sceKermitIntrInit();
  sceKermitRegisterVirtualIntrHandler(1, interrupt_handler);
  sceKermitRegisterVirtualIntrHandler(2, exit_handler);

  *(volatile u32 *)0xA8000000 = 0;
  *(volatile u32 *)0xA8000004 = 0;

  running = 1;
  while (running) {
    sceKermitInterruptHandler();
    (*(volatile u32 *)0xA8000000)++;
  }

  return 0;
}
