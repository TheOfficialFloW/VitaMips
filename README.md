# VitaMips

This is sample homebrew that demonstrates how to integrate the MIPS processor into homebrews without the need of a kernel plugin. This capability is beneficial for PS1, PS2 and N64 emulators.

### pcbctool

This tool generates a bootcode that can be placed at PSP memory `0x88600000` and executed with `sceCompatStart()`.

### Enable PspEmu Capability

To enable PspEmu Capability in homebrews, you need to use `0x2800000000000013` flags in your fself.

### Fake PSP license

Thanks to the patch [rif_check_psp_patched](https://github.com/yifanlu/taiHEN/blob/master/hen.c#L246) in taiHEN, it is possible to load fake licenses. A license is required in order for `sceCompatStart` to succeed.

### sceCompat API

The API for PSP compatibility is available at [compat.h](https://github.com/vitasdk/vita-headers/blob/master/include/psp2/compat.h).

### TODO

- Currently, 64MB of CDRAM is allocated by `sceCompat`. We don't need that much memory.
- Implement MIPS baremetal code for interrupt handling.
- Implement `sceKermit` code, such that we can use `sceCompatWaitIntr`, `sceCompatInterrupt`, `sceCompatWaitAndGetRequest` and `sceCompatReturnValueEx`.

### Credits

- All folks behind kirk_engine
- Davee
- xerpi

