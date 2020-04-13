#include <pspsdk.h>

int _start(void) __attribute__((section(".text.start")));
int _start(void) {
  *(u32 *)0x88000000 = 0;

  while (1) {
    (*(u32 *)0xA8000000)++;
  }

  return 0;
}
