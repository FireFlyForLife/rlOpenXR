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
void rlOpenXRUpdateCameraTransform(Transform* transform);

// Drawing
bool rlOpenXRBegin();
bool rlOpenXRBeginMockHMD();
void rlOpenXREnd();

enum RLOpenXREye { RLOPENXR_EYE_LEFT = 0, RLOPENXR_EYE_RIGHT = 1, RLOPENXR_EYE_BOTH = 2 };
void rlOpenXRBlitToWindow(RLOpenXREye eye, bool keep_aspect_ratio);

// State
struct RLOpenXRData
{
	XrInstance instance = XR_NULL_HANDLE; // the instance handle can be thought of as the basic connection to the OpenXR runtime
	XrSystemId system_id = XR_NULL_SYSTEM_ID; // the system represents an (opaque) set of XR devices in use, managed by the runtime
	XrSession session = XR_NULL_HANDLE; // the session deals with the renderloop submitting frames to the runtime

	XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;

	XrSpace play_space = XR_NULL_HANDLE;
	XrSpace view_space = XR_NULL_HANDLE;

	// Constants
	const XrViewConfigurationType view_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	const XrFormFactor form_factor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	const XrReferenceSpaceType play_space_type = XR_REFERENCE_SPACE_TYPE_STAGE;
};

const RLOpenXRData* rlOpenXRData();

// Hands
enum RLOpenXRHandEnum { RLOPENXR_HAND_LEFT, RLOPENXR_HAND_RIGHT, RLOPENXR_HAND_COUNT };

struct RLHand
{
	// OpenXR Ouput Data
	bool valid;
	Vector3 position;
	Quaternion orientation;

	// Input Config
	RLOpenXRHandEnum handedness;

	XrAction hand_pose_action;
	XrPath hand_pose_subpath;
	XrSpace hand_pose_space;
};

void rlOpenXRUpdateHands(RLHand* left, RLHand* right);
void rlOpenXRSyncSingleActionSet(XrActionSet action_set); // Utility function for xrSyncAction with a single action set.

// Misc
XrTime rlOpenXRGetTime(); // Get the current time from OpenXR

#ifdef __cplusplus
}
#endif