#include <psp2/compat.h>
#include <psp2/ctrl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>

#include <stdio.h>
#include <string.h>

#include "pspdebug.h"

#define printf psvDebugScreenPrintf

#define FAKE_LICENSE_NAME "AA0000-AAAA00000_00-0000000000000000"

extern uint8_t _binary_res_payload_bin_start;
extern uint8_t _binary_res_payload_bin_size;
extern uint8_t _binary_res_license_rif_start;
extern uint8_t _binary_res_license_rif_size;

int WriteFile(char *file, void *buf, int size) {
  SceUID fd = sceIoOpen(file, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
  if (fd < 0)
    return fd;

  int written = sceIoWrite(fd, buf, size);

  sceIoClose(fd);
  return written;
}

int main() {
  int res;
	
  psvDebugScreenInit();

  SceCompatCdram cdram;
  res = sceCompatAllocCdramWithHole(&cdram);
  printf("sceCompatAllocCdramWithHole: %x\n", res);

  res = sceCompatInitEx(0);
  printf("sceCompatInitEx: %x\n", res);

  memcpy(cdram.uncached_cdram + 0x600000,
         (void *)&_binary_res_payload_bin_start,
         (int)&_binary_res_payload_bin_size);

  sceIoMkdir("ux0:pspemu", 0777);
  sceIoMkdir("ux0:pspemu/PSP", 0777);
  sceIoMkdir("ux0:pspemu/PSP/LICENSE", 0777);
  WriteFile("ux0:pspemu/PSP/LICENSE/" FAKE_LICENSE_NAME ".rif",
            (void *)&_binary_res_license_rif_start,
            (int)&_binary_res_license_rif_size);
  res = sceCompatSetRif(FAKE_LICENSE_NAME);
  printf("sceCompatSetRif: %x\n", res);

  res = sceCompatStart();
  printf("sceCompatStart: %x\n", res);

  while (1) {
    psvDebugScreenSetXY(0, 5);
    printf("Press START to exit.\n");
    printf("Value: %x\n", *(int *)cdram.cached_cdram);

    SceCtrlData pad;
    sceCtrlReadBufferPositive(0, &pad, 1);
    if (pad.buttons & SCE_CTRL_START) {
      break;
    }
  }

  sceCompatStop();
  sceCompatUninit();
	
  return 0;
}
