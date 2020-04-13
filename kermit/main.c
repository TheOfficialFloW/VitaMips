#include <pspsdk.h>
#include <pspkernel.h>
#include <pspsysevent.h>

#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("sceKermit_Driver", 0x1007, 1, 0);

#define REG32(ADDR) (*(volatile u32 *)(ADDR))

#define ALIGN(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

#define MAX_CONSUMERS 16
#define MAX_RESPONSES 3
#define MAX_INTR_HANDLERS 14

#define KERMIT_MODE_AUDIO 5
#define KERMIT_MODE_ME    6

typedef struct {
  u32 cmd;
  SceUID semaid;
  u64 *resp;
  u32 pad;
  u64 args[0];
} SceKermitRequest;

typedef struct {
  u32 cmd;
  SceKermitRequest *req;
} SceKermitCommand;

typedef struct {
  u64 val;
  SceUID semaid;
  u32 pad1;
  u64 *resp;
  u32 pad2;
  u64 pad3;
} SceKermitResponse;

typedef struct {
  u32 unk_0;
  u32 unk_4;
} SceKermitInterrupt;

int sceKernelPowerLock(int lockType);
int sceKernelPowerUnlock(int lockType);

int sceKermitSystemEventHandler(int ev_id, char *ev_name, void *param,
                                int *result);

static PspSysEventHandler event_handler = {
  sizeof(PspSysEventHandler),
  "SceKermit",
  0x00ffff00,
  sceKermitSystemEventHandler,
};

static SceUID g_consumer_ready[MAX_CONSUMERS];
static SceUID g_response_available[MAX_RESPONSES];

static int g_pipe_id;

static void (* g_virtual_intr_handlers[MAX_INTR_HANDLERS])(void);

static int g_num_pending_reqs;
static int g_running;

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

int sceKermitSystemEventHandler(int ev_id, char *ev_name, void *param,
                                int *result) {
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
      interrupt[i].unk_0 = 0;
    }
  }
}

static int sceKermitInterruptHandler(void) {
  SceKermitResponse *response;
  u16 high_bits, low_bits;
  u32 bits;
  u32 intr_code;
  int intr;
  int i;

  intr = sceKernelCpuSuspendIntr();

  bits = REG32(0xBC300030) & ~0x4002;
  REG32(0xBC300030) = bits;
  __asm__ volatile ("sync");

  high_bits = bits >> 16;
  if (high_bits != 0)
    handleVirtualInterrupt(high_bits);

  low_bits = bits & 0xffff;

  for (i = 0; i < 16; i++) {
    if (low_bits & (1 << i)) {
      if (i >= 4 && i < 7) {
        intr_code = i - 4;
        sceKernelSignalSema(g_response_available[intr_code], 1);
      } else if (i >= 7 && i < 10) {
        intr_code = i - 7;
        response = (SceKermitResponse *)0xBFC00840;

        *response[intr_code].resp = response[intr_code].val;

        if (sceKernelSignalSema(response[intr_code].semaid, 1) == 0) {
          raiseCompatInterrupt(1 << i);
        }
      }
    }
  }

  sceKernelCpuResumeIntr(intr);
  return -1;
}

int sceKermitRegisterVirtualIntrHandler(u32 intr_code, void (* handler)(void)) {
  if (intr_code < MAX_INTR_HANDLERS)
    g_virtual_intr_handlers[intr_code] = handler;

  return 0;
}

int sceKermitInterrupt(u32 intr_code, int is_callback) {
  SceUID semaid;
  u32 val;
  int intr;
  int res;

  res = sceKernelPowerLock(1);
  if (res != 0)
    return res;

  sceKermitWaitReady();

  if (intr_code >= MAX_CONSUMERS) {
    res = 0x80010016;
    goto error_unlock;
  }

  intr = sceKernelCpuSuspendIntr();
  val = 1 << intr_code;
  REG32(0xBC300038) |= val;
  raiseCompatInterrupt(val);
  sceKernelCpuResumeIntr(intr);

  semaid = g_consumer_ready[intr_code];
  if (is_callback)
    res = sceKernelWaitSemaCB(semaid, 1, NULL);
  else
    res = sceKernelWaitSema(semaid, 1, NULL);
  if (res < 0)
    goto error_unlock;

  res = sceKernelPowerUnlock(1);
  if (res < 0)
    goto error_exit;

  res = 0;
  goto error_exit;

error_unlock:
  sceKernelPowerUnlock(1);
error_exit:
  return res;
}

static void *sceKermitGetReqPtr(void *req, int size) {
  void *new_req;
  u32 base, off;
  u32 req_addr;

  req_addr = ((u32)req & 0x1fffffff);

  if (req_addr >= 0x00010000 && req_addr < 0x00014000) {  // scratchpad
    base = 0x01d00000;
    off = ((u32)req & 0x03ffffff);
  } else if (req_addr >= 0x04000000 && req_addr < 0x04800000) {  // vram
    base = 0x01e00000;
    off = ((u32)req & 0x03ffffff);
  } else {
    return req;
  }

  new_req = (void *)((0xaa000000 + base + off) & 0x9fffffff);
  memcpy(new_req, req, size);
  sceKernelDcacheWritebackInvalidateRange(new_req, size);

  return new_req;
}

int sceKermitRequest(SceKermitRequest *request, int mode, int cmd, int argc,
                     int is_callback, u64 *resp) {
  SceKermitCommand *command;
  SceUInt timeout;
  SceUID semaid;
  u32 intr_code;
  int req_size;
  int intr;
  int res;

  if (!g_running) {
    sceKernelDelayThread(10 * 1000);
    *resp = 0;
    return 0;
  }

  intr = sceKernelCpuSuspendIntr();
  g_num_pending_reqs++;
  sceKernelCpuResumeIntr(intr);

  switch (mode) {
    case KERMIT_MODE_AUDIO:
      intr_code = 1;
      break;

    case KERMIT_MODE_ME:
      intr_code = 2;
      break;

    default:
      intr_code = 0;
      break;
  }

  timeout = 5 * 1000 * 1000;
  res = sceKernelWaitSema(g_response_available[intr_code], 1, &timeout);
  if (res != 0)
    goto error_exit;

  res = sceKernelReceiveMsgPipe(g_pipe_id, &semaid, sizeof(SceUID), 0, 0, NULL);
  if (res != 0)
    goto error_exit;

  command = (SceKermitCommand *)0xBFC00800;
  command[intr_code].cmd = (mode << 16) | cmd;

  request->cmd = cmd;
  request->semaid = semaid;
  request->resp = (u64 *)request;

  req_size = ALIGN(sizeof(SceKermitRequest) + argc * sizeof(u64), sizeof(u64));
  command[intr_code].req = sceKermitGetReqPtr(request, req_size);

  sceKermitWaitReady();

  res = sceKernelPowerLock(1);
  if (res != 0)
    goto error_exit;

  intr = sceKernelCpuSuspendIntr();
  raiseCompatInterrupt(1 << (intr_code + 4));
  sceKernelCpuResumeIntr(intr);

  if (is_callback)
    sceKernelWaitSemaCB(semaid, 1, NULL);
  else
    sceKernelWaitSema(semaid, 1, NULL);

  res = sceKernelSendMsgPipe(g_pipe_id, &semaid, sizeof(SceUID), 0, NULL, NULL);
  if (res != 0)
    goto error_unlock;

  *resp = ((u64 *)request)[0];

  res = sceKernelPowerUnlock(1);
  if (res < 0)
    goto error_exit;

  res = 0;
  goto error_exit;

error_unlock:
  sceKernelPowerUnlock(1);
error_exit:
  intr = sceKernelCpuSuspendIntr();
  g_num_pending_reqs--;
  sceKernelCpuResumeIntr(intr);

  return res;
}

int sceKermitDisplaySync(void) {
  int intr;
  u32 start;

  intr = sceKernelCpuSuspendIntr();

  REG32(0xBE140000) &= ~1;

  start = sceKernelGetSystemTimeLow();
  while ((sceKernelGetSystemTimeLow() - start) < 64);

  raiseCompatInterrupt(0x400);

  while ((REG32(0xBC300030) & 0x400) == 0);

  REG32(0xBC300030) = 0x400;
  __asm__ volatile ("sync");

  REG32(0xBE140000) |= 3;

  sceKernelCpuResumeIntr(intr);

  return intr;
}

static int sceKermitIntrInit(void) {
  SceUID semaid;
  int i;

  for (i = 0; i < MAX_CONSUMERS; i++) {
    semaid = sceKernelCreateSema("sceCompat", 0x100, 0, 1, NULL);
    if (semaid <= 0)
      return -1;

    g_consumer_ready[i] = semaid;
  }

  for (i = 0; i < MAX_RESPONSES; i++) {
    semaid = sceKernelCreateSema("sceCompat", 0x100, 1, 1, NULL);
    if (semaid <= 0)
      return -1;

    g_response_available[i] = semaid;
  }

  g_pipe_id = sceKernelCreateMsgPipe("sceCompat", PSP_MEMORY_PARTITION_KERNEL,
                                     0x1000, (void *)sizeof(g_consumer_ready),
                                     NULL);
  if (g_pipe_id < 0)
    return -1;

  if (sceKernelSendMsgPipe(g_pipe_id, g_consumer_ready,
                           sizeof(g_consumer_ready), 0, NULL, NULL) != 0)
    return -1;

  if (sceKernelRegisterIntrHandler(PSP_MECODEC_INT, 2,
                                   sceKermitInterruptHandler, NULL, NULL) != 0)
    return -1;

  REG32(0xBC300040) = 0xffffffff;
  REG32(0xBC300044) = 0;
  REG32(0xBC300048) = 0xffffffff;
  REG32(0xBC30004C) = 0;
  REG32(0xBC300050) = 0;
  REG32(0xBC300038) = (REG32(0xBC300038) & 0x4002) | 0xffff07f0;

  if (sceKernelEnableIntr(PSP_MECODEC_INT) != 0)
    return -1;

  return 0;
}

static int sceKermitIntrEnd(void) {
  int res;
  int i;

  REG32(0xBC300038) = 0;

  res = sceKernelReleaseIntrHandler(PSP_MECODEC_INT);
  if (res != 0)
    return res;

  for (i = 0; i < MAX_CONSUMERS; i++) {
    res = sceKernelDeleteSema(g_consumer_ready[i]);
    if (res != 0)
      return res;
  }

  return 0;
}

int sceKermitStart(void) {
  g_num_pending_reqs = 0;
  g_running = 1;

  return 0;
}

int sceKermitStop(void) {
  g_running = 0;

  while (g_num_pending_reqs > 0)
    sceKernelDelayThread(100);

  return 0;
}

int module_start(SceSize args, void *argp) {
  int res;

  sceKernelUnregisterSysEventHandler(&event_handler);

  res = sceKermitIntrInit();
  if (res < 0)
    return 1;

  sceKermitStart();

  sceKernelRegisterSysEventHandler(&event_handler);

  return 0;
}

int module_reboot_before(SceSize args, void *argp) {
  sceKermitIntrEnd();
  return 0;
}