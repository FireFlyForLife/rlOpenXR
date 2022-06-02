#pragma once
#include "openxr/openxr.h"
// raylib.h collides with Windows.h, 
// So we avoid the collision by forward declaring the win32 parts we need.

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct HDC__;
typedef HDC__* HDC;
struct HGLRC__;
typedef HGLRC__* HGLRC;
struct IUnknown;
union _LARGE_INTEGER;
typedef _LARGE_INTEGER LARGE_INTEGER;
typedef int BOOL;

// Wrapped Windows.h functions
HDC wrapped_wglGetCurrentDC();
HGLRC wrapped_wglGetCurrentContext();
BOOL wrapped_wglMakeCurrent(HDC hDC, HGLRC hGLRC);
XrTime wrapped_XrTimeFromQueryPerformanceCounter(XrInstance instance, void* xrConvertWin32PerformanceCounterToTimeKHR_funcptr);

#ifdef __cplusplus
}
#endif