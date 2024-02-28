
#include <psp2kern/kernel/modulemgr.h>
#include <taihen.h>


#define HookImport(module_name, library_nid, func_nid, func_name) \
	taiHookFunctionImportForKernel(0x10005, &func_name ## _ref, module_name, library_nid, func_nid, func_name ## _patch)

#define HookExport(module_name, library_nid, func_nid, func_name) \
	taiHookFunctionExportForKernel(0x10005, &func_name ## _ref, module_name, library_nid, func_nid, func_name ## _patch)



tai_hook_ref_t sceSblACMgrIsAllowedUsbSerial_ref;
SceBool sceSblACMgrIsAllowedUsbSerial_patch(ScePID target){
	TAI_CONTINUE(SceBool, sceSblACMgrIsAllowedUsbSerial_ref, target);
	return SCE_TRUE;
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp){

	tai_module_info_t module_info;

	module_info.size = sizeof(module_info);
	taiGetModuleInfoForKernel(SCE_KERNEL_PROCESS_ID, "SceUsbSerial", &module_info);

	HookImport("SceUsbSerial", 0x9AD8E213, 0x062CAEB2, sceSblACMgrIsAllowedUsbSerial);

	return SCE_KERNEL_START_SUCCESS;
}
