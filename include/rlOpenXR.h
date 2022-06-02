#pragma once
#include "raylib.h"
#include "openxr/openxr.h"

// TODO: DLL import/export stuff

#ifdef __cplusplus
extern "C" {
#endif

// Setup
bool rlOpenXRSetup();
void rlOpenXRShutdown();

// Update
void rlOpenXRUpdate();
void rlOpenXRUpdateCamera(Camera3D* camera);
void rlOpenXRUpdateHands(Transform* left, Transform* right);

// Drawing
bool rlOpenXRBegin();
bool rlOpenXRBeginMockHMD();
void rlOpenXREnd();

enum RLOpenXREye { RLOPENXR_EYE_LEFT = 1, RLOPENXR_EYE_RIGHT = 2, RLOPENXR_EYE_BOTH = 3 };
void rlOpenXRBlitToWindow(RLOpenXREye eye, bool keep_aspect_ratio);

// Hands
enum RLHandEnum { RLOPENXR_HAND_LEFT, RL_OPENXR_HAND_RIGHT, RL_OPENXR_HAND_COUNT };

struct RLHand
{
	// OpenXR Ouput Data
	bool valid;
	Vector3 position;
	Quaternion orientation;
	RLHandEnum handedness;

	// Input Config
	XrPath click_binding;
};

RLHand* rlOpenXRHand(RLHandEnum handedness);


#ifdef __cplusplus
}
#endif