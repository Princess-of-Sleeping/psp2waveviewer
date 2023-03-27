
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/io/fcntl.h>
#include <psp2/ctrl.h>
#include <psp2/paf.h>
#include <psp2/sysmodule.h>
#include "paf.h"


const char    sceUserMainThreadName[]          = "SceWaveViewer";
const int     sceUserMainThreadPriority        = 0x10000100;
const int     sceUserMainThreadCpuAffinityMask = 0x70000;
const SceSize sceUserMainThreadStackSize       = 0x4000;

/*
const int sceKernelPreloadModuleInhibit = SCE_KERNEL_PRELOAD_INHIBIT_LIBC
					| SCE_KERNEL_PRELOAD_INHIBIT_LIBDBG
					| SCE_KERNEL_PRELOAD_INHIBIT_APPUTIL
					| SCE_KERNEL_PRELOAD_INHIBIT_LIBSCEFT2
					| SCE_KERNEL_PRELOAD_INHIBIT_LIBPERF;
*/


typedef struct ScePafInit { // size is 0x18
	SceSize global_heap_size;
	int a2;
	int a3;
	int use_gxm;
	int heap_opt_param1;
	int heap_opt_param2;
} ScePafInit;


void *fw;
void *g_rootPage;
SceUID g_thid;


int wave_change_thread(SceSize arg_len, void *argp){

	int res;
	SceUInt32 buttons, ctrl_cache;
	SceUInt32 ctrl_press, ctrl_hold, ctrl_release;
	SceCtrlData pad_data;

	ctrl_press   = 0;
	ctrl_hold    = 0;
	ctrl_release = 0;

	while(1){

		res = sceCtrlReadBufferPositive(0, &pad_data, 1);
		if(res >= 0){
			buttons = pad_data.buttons;
			ctrl_cache = ctrl_press | ctrl_hold;

			ctrl_press   = buttons & ~ctrl_cache;
			ctrl_release = ~buttons & ctrl_cache;
			ctrl_hold    = ctrl_cache ^ ctrl_release;

			SceUInt32 windex = scePafGraphicsCurrentWave;

			if((ctrl_press & SCE_CTRL_RIGHT) != 0){
				windex++;
			}

			if((ctrl_press & SCE_CTRL_LEFT) != 0){
				windex--;
			}

			ScePafGraphics_2E30F1B5(1.0f, windex & 0x1F);
		}

		sceKernelDelayThread(16 * 1000);
	}

	return 0;
}

int loadPluginCB(void *plugin){

	ScePafWidgetMainParam si, res;

	si.name.s = "page_main";
	si.name.length = sce_paf_strlen("page_main");

	res.name.s = "page_main";
	res.name.length = sce_paf_strlen("page_main");

	res.id = scePafResourceSearchIdByName(&res, &si);

	int param[11];

	ScePafToplevel_34FE1331(param);

	param[3] = 1;

	void *result = paf_Plugin_PageOpen(plugin, &res, param);

	g_rootPage = result;

	ScePafGraphics_3EB90427();
	// ScePafGraphics_2E30F1B5(1.0f, 1);

	g_thid = sceKernelCreateThread("WaveChangeThread", wave_change_thread, 0x68, 0x1000, 0, 0, NULL);
	if(g_thid >= 0){
		sceKernelStartThread(g_thid, 0, NULL);
	}

	return 0;
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp){

	int load_res;
	ScePafInit init_param;
	SceSysmoduleOpt sysmodule_opt;

	init_param.global_heap_size = 0x1000000;
	init_param.a2               = 0xEA60;
	init_param.a3               = 0x40000;
	init_param.use_gxm          = 0;
	init_param.heap_opt_param1  = 0;
	init_param.heap_opt_param2  = 0;

	load_res = 0xDEADBEEF;
	sysmodule_opt.flags  = 0;
	sysmodule_opt.result = &load_res;

	sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF, sizeof(init_param), &init_param, &sysmodule_opt);



	SceUInt32 fwParam[0xAC/4];
	SceUInt32 pluginParam[0x94/4];


	ScePafToplevel_A5560E60(fwParam);

	fwParam[28] = 0; // frame buffer option. 0 to Game.

	fw = sce_paf_malloc(0x7C);
	if(fw != NULL){
		fw = ScePafToplevel_E2860A99(fw, fwParam);
	}

	ScePafToplevel_3F0DB1BF(fw, 0);
	// ScePafToplevel_397DD062(fw, "vs0:data/internal/effects", 7);

	ScePafToplevel_400F84CE(pluginParam);
	pluginParam[7] = 0;
	pluginParam[8] = (int)loadPluginCB;
	pluginParam[9] = 0;
	pluginParam[10] = 0;

	maybe_load_plugin_ScePafToplevel_F702E40A(pluginParam, 0, 0);

	maybe_EnterRenderingLoop_ScePafToplevel_12E33958(fw);

	sceKernelExitProcess(0);

	return SCE_KERNEL_START_SUCCESS;
}
