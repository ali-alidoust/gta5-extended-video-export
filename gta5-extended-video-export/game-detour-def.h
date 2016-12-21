#pragma once

static float Detour_GetRenderTimeBase(int64_t choice);
typedef float (*tGetRenderTimeBase)(int64_t choice);

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