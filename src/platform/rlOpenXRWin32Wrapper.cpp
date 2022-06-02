#include "platform/rlOpenXRWin32Wrapper.h"

#define XR_USE_PLATFORM_WIN32
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"
#include <windows.h>

#include <cassert>

static_assert(STRICT == 1, "rlOpenXR only supports Windows in STRICT mode, check that NO_STRICT is not defined. This is required as we forward declare HDC and HGLRC.");

#ifdef __cplusplus
extern "C" {
#endif

HDC wrapped_wglGetCurrentDC()
{
	return wglGetCurrentDC();
}

HGLRC wrapped_wglGetCurrentContext()
{
	return wglGetCurrentContext();
}

BOOL wrapped_wglMakeCurrent(HDC hDC, HGLRC hGLRC)
{
	return wglMakeCurrent(hDC, hGLRC);
}

XrTime wrapped_XrTimeFromQueryPerformanceCounter(XrInstance instance, void* xrConvertWin32PerformanceCounterToTimeKHR_funcptr)
{
	LARGE_INTEGER time_win32{};
	const BOOL success_win32 = QueryPerformanceCounter(&time_win32);
	assert(success_win32);

	auto xrConvertWin32PerformanceCounterToTimeKHR = (PFN_xrConvertWin32PerformanceCounterToTimeKHR)xrConvertWin32PerformanceCounterToTimeKHR_funcptr;
	XrTime time_xr = 0;
	XrResult result_xr = xrConvertWin32PerformanceCounterToTimeKHR(instance, &time_win32, &time_xr);
	assert(XR_SUCCEEDED(result_xr));

	return time_xr;
}

#ifdef __cplusplus
}
#endif