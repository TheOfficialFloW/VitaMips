	.set noreorder

#include "pspstub.s"

	STUB_START	"sceSuspendForKernel",0x00010011,0x00020005
	STUB_FUNC	0x3AEE7261,sceKernelPowerUnlock
	STUB_FUNC	0xEADB1BD7,sceKernelPowerLock
	STUB_END
