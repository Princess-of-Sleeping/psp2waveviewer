
#ifndef _MY_PSP2_PAF_H_
#define _MY_PSP2_PAF_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <psp2/types.h>


typedef struct ScePafInit { // size is 0x18
	SceSize global_heap_size;
	int a2;
	int a3;
	int use_gxm;
	int heap_opt_param1;
	int heap_opt_param2;
} ScePafInit;

typedef struct ScePafString {
	char *s;
	unsigned int length;
} ScePafString;

typedef struct ScePafWidgetMainParam {
	ScePafString name;
	unsigned int unk_8;
	unsigned int id;
} ScePafWidgetMainParam;


// setup wave
int ScePafGraphics_3EB90427(void);

// setting current wave
int ScePafGraphics_2E30F1B5(SceFloat32 interval, SceUInt32 index);

void **ScePafGraphics_4E038C05(void);

// init fwParam
void *ScePafToplevel_A5560E60(void *param);

// create fw
void *ScePafToplevel_E2860A99(void *fw, void *fwParam);

// some fw settings?
void ScePafToplevel_3F0DB1BF(void *fw, int a2);

void maybe_EnterRenderingLoop_ScePafToplevel_12E33958(void *fw);

// setup wave something?
int ScePafToplevel_397DD062(void *fw, const char *path, int pathlen);

// init pluginParam
void *ScePafToplevel_400F84CE(void *pluginParam);

// enter loop
int maybe_load_plugin_ScePafToplevel_F702E40A(void *pluginParam, int a2, int a3);

// exit fw loop
void ScePafToplevel_9A4B0DC4(void *fw);

// init pageeOpenParam
void *ScePafToplevel_34FE1331(void *param);

void *paf_Plugin_PageOpen(void *plugin, ScePafWidgetMainParam *a2, void *param);

// get widget hash
int scePafResourceSearchIdByName(ScePafWidgetMainParam *widgetToSearchFor, ScePafWidgetMainParam *widgetName);

// settings waveparam 0x1F
int ScePafGraphics_45A01FA1(void *pParam);


#ifdef __cplusplus
}
#endif

#endif /* _MY_PSP2_PAF_H_ */