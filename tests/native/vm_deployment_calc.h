#pragma once

#if defined(_WIN32)
#if defined(WIO_CALC_BUILD)
#define WIO_CALC_API __declspec(dllexport)
#else
#define WIO_CALC_API __declspec(dllimport)
#endif
#else
#define WIO_CALC_API __attribute__((visibility("default")))
#endif

extern "C" WIO_CALC_API int WioDeploymentAdd(int left, int right);
