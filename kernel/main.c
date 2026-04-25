#include <vitasdkkern.h>
#include <taihen.h>

static SceUID hook;
static tai_hook_ref_t ref;

typedef struct {
  int reserved0;
  int reserved1;
} sceAppMgrWorkDirMountByIdOpt;

typedef struct {
  char titleId[10];
  char passCode[32];
  char mountDrive[16];
} generic_mount_ctx;

char kbuf[64];
int _sceAppMgrWorkDirMountById_patched(int mountId, generic_mount_ctx *data, char *mount_point, sceAppMgrWorkDirMountByIdOpt *opt) {
	if (mountId == 0xCF) {
		ksceKernelMemcpyUserToKernel(kbuf, data->titleId, 10);
		//ksceDebugPrintf("TID: %s\n", kbuf);
		for (int i = 0; i < 4; i++) {
			if (!(kbuf[i] >= 'A' && kbuf[i] <= 'Z'))
				kbuf[i] = 'A';
		}
		for (int i = 4; i < 9; i++) {
			if (!(kbuf[i] >= '0' && kbuf[i] <= '9'))
				kbuf[i] = '0';
		}
		//ksceDebugPrintf("TID patched to: %s\n", kbuf);
		ksceKernelMemcpyKernelToUser(data->titleId, kbuf, 10);
	}
	return TAI_CONTINUE(int, ref, mountId, data, mount_point, opt);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	hook = taiHookFunctionExportForKernel(KERNEL_PID, &ref, "SceAppMgr", TAI_ANY_LIBRARY, 0x58E4CC90, _sceAppMgrWorkDirMountById_patched);
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
	taiHookReleaseForKernel(hook, ref);
	return SCE_KERNEL_STOP_SUCCESS;
}
