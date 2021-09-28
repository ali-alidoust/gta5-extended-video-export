#pragma once



static float Detour_GetRenderTimeBase(int64_t choice);
typedef float (*tGetRenderTimeBase)(int64_t choice);
//
//static float Detour_GetGameSpeedMultiplier(void* rcx);
//typedef float (*tGetGameSpeedMultiplier)(void* rcx);
//
//static void Detour_StepAudio(void* rcx);
//typedef void (*tStepAudio)(void* rcx);
//
static HANDLE Detour_CreateThread(void* pFunc, void* pParams, int32_t r8d, int32_t r9d, void* rsp20, int32_t rsp28, char* name);
typedef HANDLE (*tCreateThread)(void* pFunc, void* pParams, int32_t r8d, int32_t r9d, void* rsp20, int32_t rsp28, char* name);

//static void Detour_WaitForSingleObject(void* rcx, int32_t edx);
//typedef void (*tWaitForSingleObject)(void* rcx, int32_t edx);

//static float Detour_GetFrameRate(int32_t choice);
//typedef float (*tGetFrameRate)(int32_t choice);
//
//static uint32_t Detour_GetAudioSamples(int32_t choice);
//typedef uint32_t (*tGetAudioSamples)(int32_t choice);
//
//static void Detour_getFrameRateFraction(float input, uint32_t* num, uint32_t* den);
//typedef void (*tgetFrameRateFraction)(float input, uint32_t* num, uint32_t* den);
//
//static float Detour_Unk01(float x0, float x1, float x2, float x3);
//typedef float (*tUnk01)(float x0, float x1, float x2, float x3);

//static void** Detour_CreateTexture(int32_t a, char *name, void *c, uint32_t width, uint32_t height, int32_t format, void *d, bool e, void* f);
//typedef void** (*tCreateTexture)(int32_t a, char *name, void *c, uint32_t width, uint32_t height, int32_t format, void *d, bool e, void* f);

static void* Detour_CreateTexture(void* rcx, char* name, uint32_t r8d, uint32_t width, uint32_t height, uint32_t format, void* rsp30);
typedef void* (*tCreateTexture)(void* rcx, char* name, uint32_t r8d, uint32_t width, uint32_t height, uint32_t format, void* rsp30);

static void* Detour_LinearizeTexture(void* rcx, void* rdx);
typedef void* (*tLinearizeTexture)(void* rcx, void* rdx);

static uint8_t Detour_AudioUnk01(void* rcx);
typedef uint8_t (*tAudioUnk01)(void* rcx);

//static void* Detour_GetGlobalVariableIndex(char* name, uint32_t edx);
//typedef void* (*tGetGlobalVariableIndex)(char* name, uint32_t edx);
//
//static void* Detour_GetVariable(char* name, void* unknown);
//typedef void* (*tGetVariable)(char* name, void* unknown);
//
//static void* Detour_GetMatrices(void* ecx, bool edx);
//typedef void* (*tGetMatrices)(void* ecx, bool edx);
//
//static uint32_t Detour_GetVarHash(char* name, uint32_t edx);
//typedef uint32_t(*tGetVarHash)(char* name, uint32_t edx);
//
//static void* Detour_GetVarPtrByHash(void* gUnk01, uint32_t hash);
//typedef void* (*tGetVarPtrByHash)(void* gUnk01, uint32_t hash);
//
//static void* Detour_GetVarPtrByHash2(char* hash);
//typedef void* (*tGetVarPtrByHash2)(char* hash);


//static void* Detour_GetTexture(char* name, uint32_t edx, uint32_t r8d, uint32_t r9d, uint32_t rsp8, uint32_t rsp10, void* rsp18);
//typedef void* (*tGetTexture)(char* name, uint32_t edx, uint32_t r8d, uint32_t r9d, uint32_t rsp8, uint32_t rsp10, void* rsp18);