#if 0

bbv_radiance_injection_apply_cs.hlsl

MainView の HDR radiance + depth から BBV Brick ごとの radiance accumulation へ atomic 加算する.

#endif

#define NGL_SRVS_RADIANCE_ENABLE_SHORT_RAY_FALLBACK 0
#include "bbv_radiance_injection_apply_core.hlsli"
