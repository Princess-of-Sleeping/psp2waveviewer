
#include <psp2/kernel/clib.h>
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/paf.h>
#include <psp2/sysmodule.h>
#include <psp2/usbserial.h>
#include <psp2/shellutil.h>
#include <taihen.h>
#include "paf.h"
#include "../lib/libdecoder.h"


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


static void *fw;
static void *g_rootPage;
static SceUID g_thid = -1;
static SceInt32 g_alive_usbserial;
static SceUInt32 ctrl_press, ctrl_hold, ctrl_release;

static SceUID bootlogo_base_memid = -1;
extern const SceUInt8 wave_bootlogo[];


int setup_bootlogo(void){

	int res;
	SceDisplayFrameBuf fb_param;

	do {
		fb_param.size        = sizeof(fb_param);
		fb_param.base        = NULL;
		fb_param.pitch       = 960;
		fb_param.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
		fb_param.width       = 960;
		fb_param.height      = 544;

		res = sceKernelAllocMemBlock("Bootlogo", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 0x200000, NULL);
		if(res < 0){
			return res;
		}

		bootlogo_base_memid = res;

		do {
			res = sceKernelGetMemBlockBase(bootlogo_base_memid, &(fb_param.base));
			if(res < 0){
				break;
			}

			res = logo_decode(fb_param.base, 0x1FE000, wave_bootlogo + 4, NULL);
			if(res < 0){
				break;
			}

			res = sceDisplaySetFrameBuf(&fb_param, SCE_DISPLAY_SETBUF_NEXTFRAME);
			if(res < 0){
				break;
			}

			return 0;
		} while(0);

		sceKernelFreeMemBlock(bootlogo_base_memid);
		bootlogo_base_memid = -1;
	} while(0);

	return res;
}

int wave_change_thread(SceSize arg_len, void *argp){

	int res;
	SceUInt32 buttons, ctrl_cache;
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
				ScePafGraphics_2E30F1B5(1.0f, windex & 0x1F);
			}

			if((ctrl_press & SCE_CTRL_LEFT) != 0){
				windex--;
				ScePafGraphics_2E30F1B5(1.0f, windex & 0x1F);
			}

			if((ctrl_press & SCE_CTRL_LTRIGGER) != 0){
				sceKernelAtomicClearAndGet32(&g_alive_usbserial, 1);
			}
		}

		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
		sceKernelDelayThread(16 * 1000);
	}

	return 0;
}

int dump_current_wave_param(const char *name, void *pWaveParam){

	int res;

	tai_module_info_t tai_module_info;
	tai_module_info.size = sizeof(tai_module_info);
	res = taiGetModuleInfo("ScePaf", &tai_module_info);
	if(res < 0){
		return res;
	}

	SceKernelModuleInfo moduleInfo;
	moduleInfo.size = sizeof(moduleInfo);
	res = sceKernelGetModuleInfo(tai_module_info.modid, &moduleInfo);
	if(res < 0){
		return res;
	}

	void **(* FUN_8109e9a0)(void *a1, SceBool doInit) = NULL;
	void *(* FUN_811fe9e8_Material)(void *a1, ScePafString *pString, SceBool a3) = NULL;
	void *(* FUN_811fef58_PointLightSphere)(void *a1, ScePafString *pString, SceBool a3) = NULL;
	void *(* FUN_811ff448_Fog)(void *a1, ScePafString *pString, SceBool a3) = NULL;
	void *(* FUN_811ffb88_Sky)(void *a1, ScePafString *pString, SceBool a3) = NULL;
	void *(* FUN_812000b8_FFTWave)(void *a1, ScePafString *pString, SceBool a3) = NULL;
	void *(* FUN_81200b40_WaveInstance)(void *a1, ScePafString *pString, SceBool a3) = NULL;
	void *(* FUN_8120116c_WaveRenderer)(void *a1, ScePafString *pString, SceBool a3) = NULL;

	SceUIntPtr text_base = (SceUIntPtr)moduleInfo.segments[0].vaddr;

	switch(tai_module_info.module_nid){
	case 0x03182AC6: // 3.200.010 Tool
		FUN_8109e9a0                  = (void *)(text_base + (0x9e9a0 | 1));
		FUN_811fe9e8_Material         = (void *)(text_base + (0x1fe9e8 | 1));
		FUN_811fef58_PointLightSphere = (void *)(text_base + (0x1fef58 | 1));
		FUN_811ff448_Fog              = (void *)(text_base + (0x1ff448 | 1));
		FUN_811ffb88_Sky              = (void *)(text_base + (0x1ffb88 | 1));
		FUN_812000b8_FFTWave          = (void *)(text_base + (0x2000b8 | 1));
		FUN_81200b40_WaveInstance     = (void *)(text_base + (0x200b40 | 1));
		FUN_8120116c_WaveRenderer     = (void *)(text_base + (0x20116c | 1));
		break;
	case 0xCD679177: // 3.600.011 CEX
	case 0x4AB91355: // 3.600.011 DEX
	case 0x73F90499: // 3.650.011 CEX
	case 0xDFE774A1: // 3.650.011 DEX
		FUN_8109e9a0                  = (void *)(text_base + (0x9d40e | 1));
		FUN_811fe9e8_Material         = (void *)(text_base + (0x1f9c3c | 1));
		FUN_811fef58_PointLightSphere = (void *)(text_base + (0x1fa2a8 | 1));
		FUN_811ff448_Fog              = (void *)(text_base + (0x1fa850 | 1));
		FUN_811ffb88_Sky              = (void *)(text_base + (0x1fb030 | 1));
		FUN_812000b8_FFTWave          = (void *)(text_base + (0x1fb560 | 1));
		FUN_81200b40_WaveInstance     = (void *)(text_base + (0x1fc110 | 1));
		FUN_8120116c_WaveRenderer     = (void *)(text_base + (0x1fc7cc | 1));
		break;
	case 0xFA5137D0: // 3.600.011 Tool
	case 0xD6AB6069: // 3.650.011 Tool
		FUN_8109e9a0                  = (void *)(text_base + (0x9defe | 1));
		FUN_811fe9e8_Material         = (void *)(text_base + (0x1fa7f4 | 1));
		FUN_811fef58_PointLightSphere = (void *)(text_base + (0x1fae60 | 1));
		FUN_811ff448_Fog              = (void *)(text_base + (0x1fb408 | 1));
		FUN_811ffb88_Sky              = (void *)(text_base + (0x1fbbe8 | 1));
		FUN_812000b8_FFTWave          = (void *)(text_base + (0x1fc118 | 1));
		FUN_81200b40_WaveInstance     = (void *)(text_base + (0x1fccc8 | 1));
		FUN_8120116c_WaveRenderer     = (void *)(text_base + (0x1fd384 | 1));
		break;
	default:
		sceClibPrintf("Unknown ScePaf fingerprint 0x%08X\n", tai_module_info.module_nid);
		return -1;
		break;
	}

	void **ppCtx = FUN_8109e9a0(*ScePafGraphics_4E038C05(), SCE_FALSE);
	void *pCtx = *ppCtx;

	void *root = pCtx + 0xd60;
	void *curr = *(void **)(pCtx + 0xd64);

	const char *target_wave_name = name;

	int len = sce_paf_strlen(target_wave_name);

	ScePafString target;
	target.s = sce_paf_malloc(len + 1);
	target.length = len;

	sce_paf_memcpy(target.s, target_wave_name, len);
	target.s[len] = 0;

	sce_paf_memset(pWaveParam, 0, 0x2A0);

	int Fog_flag = 0;

	while(root != curr){

		char *m_name = *(char **)(curr + 0x14);

		if(sce_paf_strcmp(m_name, "Material") == 0){

			void *store = FUN_811fe9e8_Material(curr - 4, &target, SCE_FALSE);

			sce_paf_memcpy(pWaveParam + 0x30, store + 0x48, 0x10);
			sce_paf_memcpy(pWaveParam + 0x40, store + 0x64, 4);
			sce_paf_memcpy(pWaveParam + 0x44, store + 0x70, 4);
			sce_paf_memcpy(pWaveParam + 0x48, store + 0x7C, 4);
			sce_paf_memcpy(pWaveParam + 0x4C, store + 0x88, 4);
			sce_paf_memcpy(pWaveParam + 0x50, store + 0x94, 4);
			sce_paf_memcpy(pWaveParam + 0x54, store + 0xA0, 4);
			sce_paf_memcpy(pWaveParam + 0x58, store + 0xAC, 4);
			sce_paf_memcpy(pWaveParam + 0x5C, store + 0xB8, 4);
			sce_paf_memcpy(pWaveParam + 0x60, store + 0xC4, 4);

		}else if(sce_paf_strcmp(m_name, "PointLightSphere") == 0){
			void *store = FUN_811fef58_PointLightSphere(curr - 4, &target, SCE_FALSE);

			sce_paf_memcpy(pWaveParam + 0x70, store + 0x48, 8);
			sce_paf_memset(pWaveParam + 0x78, 0, 8);
			sce_paf_memcpy(pWaveParam + 0x80, store + 0x70, 0x10);
			sce_paf_memcpy(pWaveParam + 0x90, store + 0x90, 0x10);
			sce_paf_memcpy(pWaveParam + 0xA0, store + 0xB0, 0x10);
			sce_paf_memcpy(pWaveParam + 0xB0, store + 0x5C, 4);
			sce_paf_memcpy(pWaveParam + 0xB4, store + 0xCC, 4);

		}else if(sce_paf_strcmp(m_name, "Fog") == 0){

			void *pFog = pWaveParam + 0x100;

			if(Fog_flag == 0){
				Fog_flag = 1;
				pFog = pWaveParam + 0xC0;
			}

			void *store = FUN_811ff448_Fog(curr - 4, &target, SCE_FALSE);

			sce_paf_memcpy(pFog + 0x00, store + 0x48, 0x10);
			sce_paf_memcpy(pFog + 0x10, store + 0x68, 0x10);
			sce_paf_memcpy(pFog + 0x20, store + 0x88, 0x10);
			sce_paf_memcpy(pFog + 0x30, store + 0xA4, 4);
			sce_paf_memcpy(pFog + 0x34, store + 0xB0, 4);

		}else if(sce_paf_strcmp(m_name, "Sky") == 0){

			void *store = FUN_811ffb88_Sky(curr - 4, &target, SCE_FALSE);

			sce_paf_memcpy(pWaveParam + 0x140, store + 0x48, 8);
			sce_paf_memset(pWaveParam + 0x148, 0, 8);
			sce_paf_memcpy(pWaveParam + 0x150, store + 0x70, 0x10);
			sce_paf_memcpy(pWaveParam + 0x160, store + 0xA8, 0x10);
			sce_paf_memcpy(pWaveParam + 0x170, store + 0xC8, 0x10);
			sce_paf_memcpy(pWaveParam + 0x180, store + 0x5C, 4);
			sce_paf_memcpy(pWaveParam + 0x184, store + 0x8C, 4);
			sce_paf_memcpy(pWaveParam + 0x188, store + 0x98, 4);
			sce_paf_memcpy(pWaveParam + 0x18C, store + 0xE4, 4);
			sce_paf_memcpy(pWaveParam + 0x190, store + 0xF0, 4);
			sce_paf_memcpy(pWaveParam + 0x194, store + 0xFC, 4);
			sce_paf_memcpy(pWaveParam + 0x198, store + 0x108, 4);
			sce_paf_memcpy(pWaveParam + 0x19C, store + 0x114, 4);
			sce_paf_memcpy(pWaveParam + 0x1A0, store + 0x120, 4);
			sce_paf_memcpy(pWaveParam + 0x1A4, store + 0x12C, 4);
			sce_paf_memcpy(pWaveParam + 0x1A8, store + 0x138, 4);
			sce_paf_memcpy(pWaveParam + 0x1AC, store + 0x144, 4);
			sce_paf_memcpy(pWaveParam + 0x1B0, store + 0x150, 4);
			sce_paf_memcpy(pWaveParam + 0x1B4, store + 0x15C, 4);
			sce_paf_memcpy(pWaveParam + 0x1B8, store + 0x168, 4);
			sce_paf_memcpy(pWaveParam + 0x1BC, store + 0x16C, 4);

		}else if(sce_paf_strcmp(m_name, "FFTWave") == 0){

			void *store = FUN_812000b8_FFTWave(curr - 4, &target, SCE_FALSE);

			sce_paf_memcpy(pWaveParam + 0x1C0, store + 0x44, 4);
			sce_paf_memcpy(pWaveParam + 0x1C4, store + 0x50, 4);
			sce_paf_memcpy(pWaveParam + 0x1C8, store + 0x5C, 4);
			sce_paf_memcpy(pWaveParam + 0x1CC, store + 0x68, 4);
			sce_paf_memcpy(pWaveParam + 0x1D0, store + 0x74, 4);
			sce_paf_memcpy(pWaveParam + 0x1D4, store + 0x80, 4);
			sce_paf_memcpy(pWaveParam + 0x1D8, store + 0x8C, 4);

		}else if(sce_paf_strcmp(m_name, "WaveInstance") == 0){

			void *store = FUN_81200b40_WaveInstance(curr - 4, &target, SCE_FALSE);

			sce_paf_memcpy(pWaveParam + 0x1E0, store + 0x58, 0x10);
			sce_paf_memcpy(pWaveParam + 0x1F0, store + 0x78, 0x10);
			sce_paf_memcpy(pWaveParam + 0x200, store + 0x98, 0x10);

			sce_paf_memcpy(pWaveParam + 0x210, store + 0xE0, 0x10);
			sce_paf_memcpy(pWaveParam + 0x220, store + 0x110, 0x10);

			sce_paf_memcpy(pWaveParam + 0x230, store + 0x140, 0x10);
			sce_paf_memcpy(pWaveParam + 0x240, store + 0x160, 0x10);
			sce_paf_memcpy(pWaveParam + 0x250, store + 0x44, 4);
			sce_paf_memcpy(pWaveParam + 0x254, store + 0xB4, 4);
			sce_paf_memcpy(pWaveParam + 0x258, store + 0xC0, 4);
			sce_paf_memcpy(pWaveParam + 0x25C, store + 0xCC, 4);
			sce_paf_memcpy(pWaveParam + 0x260, store + 0xFC, 4);
			sce_paf_memcpy(pWaveParam + 0x264, store + 0x12C, 4);
			sce_paf_memcpy(pWaveParam + 0x268, store + 0x17C, 4);
			sce_paf_memcpy(pWaveParam + 0x26C, store + 0x188, 4);

		}else if(sce_paf_strcmp(m_name, "WaveRenderer") == 0){

			void *store = FUN_8120116c_WaveRenderer(curr - 4, &target, SCE_FALSE);

			sce_paf_memcpy(pWaveParam + 0x270, store + 0x48, 8);
			sce_paf_memset(pWaveParam + 0x278, 0, 8);
			sce_paf_memcpy(pWaveParam + 0x284, store + 0x68, 4);
			sce_paf_memcpy(pWaveParam + 0x288, store + 0x74, 4);
			sce_paf_memcpy(pWaveParam + 0x28C, store + 0x80, 4);
			sce_paf_memcpy(pWaveParam + 0x290, store + 0x8C, 4);
			sce_paf_memcpy(pWaveParam + 0x294, store + 0x98, 4);
			sce_paf_memcpy(pWaveParam + 0x298, store + 0xA4, 4);
			sce_paf_memcpy(pWaveParam + 0x29C, store + 0xB0, 4);
		}

		curr = *(void **)(curr + 0x4);
	}

	sce_paf_free(target.s);

	return 0;
}

const char * const wave_name_list[0x20] = {
	"effects_default_0512_01",
	"iboot",
	"effects_0406_ruby",
	"effects_0407_purple_01",
	"effects_0408_01_orange",
	"effects_0411_purple_02",
	"effects_0415_A5",
	"effects_0518_dalia",
	"effects_0518_gray_FLAT",
	"effects_0518_ruby",
	"effects_A1",
	"effects_black",
	"effects_blue_FLAT",
	"effects_blueTB_0513_01",
	"effects_bluewhite_05",
	"effects_casis",

	"effects_green_0512_01",
	"effects_koiao_0512_01",
	"effects_maintosh",
	"effects_marrygold",
	"effects_peach_0512_001",
	"effects_purple_01",
	"effects_purplered",
	"effects_yellow_0512_01",
	"effects_redTB_0513_01",
	"effects_limegreen_0513_02",
	"effects_turquoise_0513_01",
	"effects_color_vari_2g_01",
	"effects_color_vari_2g_02",
	"effects_color_vari_2g_03",
	"effects_color_vari_2g_04",
	"effects_original_01"
};

#define SCE_USB_SERIAL_ERROR_NOT_STARTED      (0x80244401)
#define SCE_USB_SERIAL_ERROR_INVALID_ARGUMENT (0x80244402)
// 0x80244403
// 0x80244404
#define SCE_USB_SERIAL_ERROR_NOT_CONNECTED    (0x80244405)
// 0x80244406
#define SCE_USB_SERIAL_ERROR_TIMEOUT          (0x80244407)
// 0x80244408


void *g_fw;

typedef struct _psp2wpp_comm_packet {
	uint32_t sequence;
	uint32_t cmd;
	int result;
	int size;
} psp2wpp_comm_packet;

int start_remote_helper(void){

	int res;
	SceUID moduleId;

	res = taiLoadKernelModule("app0:/usbserial_patch.skprx", 0x1000, NULL);
	if(res < 0){
		sceClibPrintf("taiLoadKernelModule 0x%X\n", res);
		return res;
	}

	moduleId = res;

	res = taiStartKernelModule(moduleId, 0, NULL, 0, NULL, NULL);
	if(res < 0){
		sceClibPrintf("taiStartKernelModule 0x%X\n", res);
		return res;
	}

	return 0;
}

int wave_exec_thread(SceSize arg_len, void *argp){

	int res;
	SceUInt8 waveparam[0x2A0];

	if(sceUsbSerialStatus() == 0x80010058){
		res = start_remote_helper();
		if(res < 0){
			return sceKernelExitDeleteThread(0);
		}
	}

	sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);

	int retry = 3000;

	res = sceUsbSerialStart();
	if(res != SCE_OK){
		sceClibPrintf("sceUsbSerialStart 0x%X\n", res);
	}

	res = sceUsbSerialSetup(1);
	if(res != SCE_OK){
		sceClibPrintf("sceUsbSerialSetup 0x%X\n", res);
	}

	do {
		res = sceUsbSerialStatus();
		// sceClibPrintf("sceUsbSerialStatus 0x%X\n", res);
		sceKernelDelayThread(1000);
		retry--;
	} while(res == 0x80244401 && retry != 0); // not connected

	retry = 1000;

	do {
		res = sceUsbSerialStatus();
		sceKernelDelayThread(10000);
		retry--;
	} while(res == 0 && retry != 0);

	sceClibPrintf("sceUsbSerialStatus 0x%X\n", res);

	sceKernelDelayThread(10000);


	psp2wpp_comm_packet comm_packet;
	SceUInt32 sequence = 0;

	int is_alive = sceKernelAtomicGetAndOr32(&g_alive_usbserial, 2);
	while((is_alive & 1) != 0){

		do {
			comm_packet.sequence = sequence;
			comm_packet.cmd = 1;
			comm_packet.size = 0;

			res = sceUsbSerialSend(&comm_packet, sizeof(comm_packet), 0, 1000);
			if(res < 0){
				if(res != SCE_USB_SERIAL_ERROR_NOT_CONNECTED && res != SCE_USB_SERIAL_ERROR_TIMEOUT){
					sceClibPrintf("sceUsbSerialSend 0x%X\n", res);
				}

				sequence = 0;
				break;
			}

			res = sceUsbSerialRecv(&comm_packet, sizeof(comm_packet), 0, 16600);
			if(res < 0){
				if(res != SCE_USB_SERIAL_ERROR_NOT_CONNECTED && res != SCE_USB_SERIAL_ERROR_TIMEOUT){
					sceClibPrintf("sceUsbSerialRecv 0x%X\n", res);
				}

				sequence = 0;
				break;
			}

			sceKernelDelayThread(16 * 1000);

			sequence += 1;

			if(sequence != comm_packet.sequence){
				sceClibPrintf("Bad sequence. Will be reset the handshake after 3 seconds and try again.\n");
				sceKernelDelayThread(3 * 1000 * 1000);
				sequence = 0;
				break;
			}

			sequence = comm_packet.sequence;

			uint32_t cmd = comm_packet.cmd;

			switch(cmd){
			case 0:
				break;
			case 1:
				res = sceUsbSerialRecv(waveparam, sizeof(waveparam), 0, 16600);
				if(res < 0){
					sceClibPrintf("L%d: sceUsbSerialRecv 0x%X (res)\n", __LINE__, res);
					break;
				}

				res = ScePafGraphics_45A01FA1(waveparam);
				if(res < 0){
					sceUsbSerialSend((SceUInt32[]){res}, 4, 0, 1000);
					break;
				}

				res = sceUsbSerialSend((SceUInt32[]){0}, 4, 0, 1000);
				if(res < 0){
					sceClibPrintf("L%d: sceUsbSerialSend 0x%X (res)\n", __LINE__, res);
					break;
				}

				scePafGraphicsCurrentWave = 0;
				ScePafGraphics_2E30F1B5(1.0f, 0x1F);
				break;
			case 2:
				res = dump_current_wave_param(wave_name_list[scePafGraphicsCurrentWave & 0x1F], waveparam);
				if(res < 0){
					sceUsbSerialSend((SceUInt32[]){res}, 4, 0, 1000);
					break;
				}

				*(SceUInt32 *)(waveparam + 0x0) = 0x61776976;
				*(SceUInt32 *)(waveparam + 0x4) = 1;
				*(SceUInt32 *)(waveparam + 0x8) = scePafGraphicsCurrentWave & 0x1F;
				*(SceUInt32 *)(waveparam + 0xC) = 0;

				res = sceUsbSerialSend((SceUInt32[]){0}, 4, 0, 1000);
				if(res < 0){
					sceClibPrintf("L%d: sceUsbSerialSend 0x%X (res)\n", __LINE__, res);
					break;
				}

				res = sceUsbSerialSend(waveparam, sizeof(waveparam), 0, 1000);
				if(res < 0){
					sceClibPrintf("L%d: sceUsbSerialSend 0x%X (res)\n", __LINE__, res);
					break;
				}

				break;
			case 3:
				res = sceUsbSerialRecv(waveparam, sizeof(waveparam), 0, 16600);
				if(res < 0){
					sceClibPrintf("L%d: sceUsbSerialRecv 0x%X (res)\n", __LINE__, res);
					break;
				}

				do {
					SceUID fd = sceIoOpen("ux0:/data/waveparam.bin", SCE_O_CREAT | SCE_O_TRUNC | SCE_O_WRONLY, 0606);
					if(fd < 0){
						res = fd;
						break;
					}

					res = sceIoWrite(fd, waveparam, sizeof(waveparam));
					sceIoClose(fd);

					if(res == sizeof(waveparam)){
						res = 0;
					}
				} while(0);

				res = sceUsbSerialSend((SceUInt32[]){res}, 4, 0, 1000);
				if(res < 0){
					sceClibPrintf("L%d: sceUsbSerialSend 0x%X (res)\n", __LINE__, res);
					break;
				}

				break;
			default:
				break;
			}
		} while(0);

		sceKernelDelayThread(16600);
		is_alive = sceKernelAtomicGetAndOr32(&g_alive_usbserial, 2);
	}

	sceUsbSerialClose();
	sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);

	sceClibPrintf("Leave psp2wpp remote configuration mode\n");

	ScePafToplevel_9A4B0DC4(g_fw);

	return 0;
}

int load_waveparam(const char *path){

	int res;
	SceUID fd;
	SceUInt8 waveparam[0x2A0];

	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if(fd < 0){
		return fd;
	}

	do {
		res = sceIoRead(fd, waveparam, sizeof(waveparam));
		if(res < 0){
			break;
		}

		if(res != 0x2A0){
			res = -1;
			break;
		}

		res = ScePafGraphics_45A01FA1(waveparam);
	} while(0);

	sceIoClose(fd);

	return res;
}

SceUInt64 boot_start_time, boot_end_time;

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

	sceDisplaySetFrameBuf(NULL, SCE_DISPLAY_SETBUF_NEXTFRAME);
	sceKernelFreeMemBlock(bootlogo_base_memid);
	bootlogo_base_memid = -1;

	// wait for user input.
	sceKernelDelayThread(2 * 1000 * 1000);

	if((ctrl_hold & SCE_CTRL_LTRIGGER) != 0){

		sceShellUtilInitEvents(0);

		sceClibPrintf("Enter psp2wpp remote configuration mode\n");

		sceKernelAtomicGetAndOr32(&g_alive_usbserial, 1);

		SceUID thid = sceKernelCreateThread("WaveExecThread", wave_exec_thread, 0x68, 0x4000, 0, 0, NULL);
		if(thid >= 0){
			sceKernelStartThread(thid, 0, NULL);
		}
	}

	ScePafGraphics_3EB90427();
	// ScePafGraphics_2E30F1B5(1.0f, 11); // black wave

	load_waveparam("ux0:/data/waveparam.bin");

	if(0){
		int res;

		res = load_waveparam("host0:data/waveparam.bin");

		if(res < 0){
			res = load_waveparam("sd0:/data/waveparam.bin");
		}

		if(res < 0){
			res = load_waveparam("ux0:/data/waveparam.bin");
		}

		if(res < 0){
			res = load_waveparam("pd0:/wave/waveparam.bin");
		}
	}

	boot_end_time = sceKernelGetSystemTimeWide();

	sceClibPrintf("Boot take %lld [usec]\n", boot_end_time - boot_start_time);

	return 0;
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp){

	boot_start_time = sceKernelGetSystemTimeWide();
	setup_bootlogo();

	int load_res;
	ScePafInit init_param;
	SceSysmoduleOpt sysmodule_opt;

	init_param.global_heap_size = 0x100000;
	init_param.a2               = 0xEA60;
	init_param.a3               = 0x40000;
	init_param.use_gxm          = 0;
	init_param.heap_opt_param1  = 0;
	init_param.heap_opt_param2  = 0;

	load_res = 0xDEADBEEF;
	sysmodule_opt.flags  = 0;
	sysmodule_opt.result = &load_res;

	sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF, sizeof(init_param), &init_param, &sysmodule_opt);

	g_thid = sceKernelCreateThread("WaveChangeThread", wave_change_thread, 0x68, 0x1000, 0, 0, NULL);
	if(g_thid >= 0){
		sceKernelStartThread(g_thid, 0, NULL);
	}

	SceUInt32 fwParam[0xAC/4];
	SceUInt32 pluginParam[0x94/4];


	ScePafToplevel_A5560E60(fwParam);

	fwParam[28] = 0; // frame buffer option. 0 to Game.

	fw = sce_paf_malloc(0x7C);
	if(fw != NULL){
		fw = ScePafToplevel_E2860A99(fw, fwParam);
	}

	g_fw = fw;

	ScePafToplevel_3F0DB1BF(fw, 0);
	// ScePafToplevel_397DD062(fw, "vs0:data/internal/effects", 7);

	ScePafToplevel_400F84CE(pluginParam);
	pluginParam[7] = 0;
	pluginParam[8] = (uintptr_t)loadPluginCB;
	pluginParam[9] = 0;
	pluginParam[10] = 0;

	maybe_load_plugin_ScePafToplevel_F702E40A(pluginParam, 0, 0);

	maybe_EnterRenderingLoop_ScePafToplevel_12E33958(fw);

	sceKernelExitProcess(0);

	return SCE_KERNEL_START_SUCCESS;
}
