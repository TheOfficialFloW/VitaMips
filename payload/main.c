#include <pspsdk.h>

void ClearICache(void) {
  __asm__ volatile (
     ".word 0x40088000\n" // mfc0    $t0, Config
     ".word 0x24091000\n" // li      $t1, 0x1000
     ".word 0x7D081240\n" // ext     $t0, 9, 3
     ".word 0x01094804\n" // sllv    $t1, $t0
     ".word 0x4080E000\n" // mtc0    $0, TagLo
     ".word 0x4080E800\n" // mtc0    $0, TagHi
     ".word 0x00004021\n" // move    $t0, $0
     ".word 0xBD010000\n" // cache   1, 0($t0) #loop
     ".word 0xBD030000\n" // cache   3, 0($t0)
     ".word 0x25080040\n" // addiu   $t0, 0x40
     ".word 0x1509FFFC\n" // bne     $t0, $t1, loop
     ".word 0x00000000\n" // nop
     ".word 0x03E00008\n" // jr      $ra
     ".word 0x00000000\n" // nop
   );
}

void ClearDCache(void) {
  __asm__ volatile (
     ".word 0x40088000\n" // mfc0    $t0, Config
     ".word 0x24090800\n" // li      $t1, 0x800
     ".word 0x7D081180\n" // ext     $t0, 6, 3
     ".word 0x01094804\n" // sllv    $t1, $t0
     ".word 0x00004021\n" // move    $t0, $0
     ".word 0xBD140000\n" // cache   0x14, 0($t0) #loop
     ".word 0xBD140000\n" // cache   0x14, 0($t0)
     ".word 0x25080040\n" // addiu   $t0, 0x40
     ".word 0x1509FFFC\n" // bne     $t0, $t1, loop
     ".word 0x00000000\n" // nop
     ".word 0x03E00008\n" // jr      $ra
     ".word 0x0000000F\n" // sync
   );
}

int _start(void) __attribute__((section(".text.start")));
int _start(void) {
  *(u32 *)0x88000000 = 0x1337;

  while (1) {
    (*(u32 *)0x88000000)++; // atm it only increases once wtf
    ClearDCache();
  }

  return 0;
}
