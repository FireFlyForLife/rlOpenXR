#include "rlOpenXR.h"

#include "raylib.h"
#include "raymath.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	XrActionSet actionset;

	XrAction hand_pose_action;
	XrPath hand_paths[2];
	XrSpace hand_spaces[2];
} XRInputBindings;

void setup_input_bindings(XRInputBindings* bindings);
void assign_hand_input_bindings(XRInputBindings* bindings, RLHand* left, RLHand* right);

int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 1200;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "rlOpenXR - Hello Hands");
    
	if (!rlOpenXRSetup())
	{
		printf("Failed to initialise rlOpenXR!");
		return 1;
	}

    Camera camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 3.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

	XRInputBindings bindings = { 0 };
	setup_input_bindings(&bindings);

    RLHand left_hand = { 0 };
	left_hand.handedness = RLOPENXR_HAND_LEFT;
	RLHand right_hand = { 0 };
    right_hand.handedness = RLOPENXR_HAND_RIGHT;
	assign_hand_input_bindings(&bindings, &left_hand, &right_hand);

    Model hand_model = LoadModelFromMesh(GenMeshCube(0.2f, 0.2f, 0.2f));

    SetCameraMode(camera, CAMERA_FREE);

    SetTargetFPS(-1); // OpenXR is responsible for waiting in rlOpenXRUpdate()
                      // Having raylib also do it's VSync causes noticeable input lag

    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------

        rlOpenXRUpdate(); // Update OpenXR State
						  // Should be called at the start of each frame before other rlOpenXR calls.

		rlOpenXRSyncSingleActionSet(bindings.actionset); // Simple utility function for simple apps with 1 action set.
														 // xrSyncAction will activate action sets for use.

        rlOpenXRUpdateHands(&left_hand, &right_hand);

        UpdateCamera(&camera); // Use mouse control as a debug option when no HMD is available
        rlOpenXRUpdateCamera(&camera); // If the HMD is available, set the camera position to the HMD position

        // Draw
        //----------------------------------------------------------------------------------
        ClearBackground(RAYWHITE); // Clear window, in case rlOpenXR skips rendering the frame, we don't have garbage data in the backbuffer

        // rlOpenXRBegin() returns false when OpenXR reports to skip the frame (The HMD is inactive).
        // Optionally rlOpenXRBeginMockHMD() can be chained to always render. It will render into a "Mock" backbuffer.
        if (rlOpenXRBegin() || rlOpenXRBeginMockHMD()) // Render to OpenXR backbuffer
        {
            ClearBackground(BLUE);

            BeginMode3D(camera);

                // Draw Hands
                Vector3 left_hand_axis;
                float left_hand_angle;
                QuaternionToAxisAngle(left_hand.orientation, &left_hand_axis, &left_hand_angle);

                Vector3 right_hand_axis;
                float right_hand_angle;
                QuaternionToAxisAngle(right_hand.orientation, &right_hand_axis, &right_hand_angle);

                DrawModelEx(hand_model, left_hand.position, left_hand_axis, left_hand_angle * RAD2DEG, Vector3One(), ORANGE);
                DrawModelEx(hand_model, right_hand.position, right_hand_axis, right_hand_angle * RAD2DEG, Vector3One(), PINK);

				// Draw Scene
				DrawCube((Vector3) { -3, 0, 0 }, 2.0f, 2.0f, 2.0f, RED);
                DrawGrid(10, 1.0f);

            EndMode3D();

            const bool keep_aspect_ratio = true;
            rlOpenXRBlitToWindow(RLOPENXR_EYE_BOTH, keep_aspect_ratio); // Copy OpenXR backbuffer to window backbuffer
																		// Useful for viewing the image on a flatscreen
		}
        rlOpenXREnd();


        BeginDrawing(); // Draw to the window, eg, debug overlays

            DrawFPS(10, 10);
    
        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    rlOpenXRShutdown();

    UnloadModel(hand_model);

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

void setup_input_bindings(XRInputBindings* bindings)
{
	const RLOpenXRData* xr = rlOpenXRData();

	XrResult result = xrStringToPath(xr->instance, "/user/hand/left", &bindings->hand_paths[RLOPENXR_HAND_LEFT]);
	assert(XR_SUCCEEDED(result) && "Could not convert Left hand string to path.");
	result = xrStringToPath(xr->instance, "/user/hand/right", &bindings->hand_paths[RLOPENXR_HAND_RIGHT]);
	assert(XR_SUCCEEDED(result) && "Could not convert Right hand string to path.");

	XrActionSetCreateInfo actionset_info = { 0 };
	actionset_info.type = XR_TYPE_ACTION_SET_CREATE_INFO;
	actionset_info.next = NULL;
	strncpy_s(actionset_info.actionSetName, XR_MAX_ACTION_SET_NAME_SIZE, 
		"rlopenxr_hello_hands_actionset", XR_MAX_ACTION_SET_NAME_SIZE);
	strncpy_s(actionset_info.localizedActionSetName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE, 
		"OpenXR Hello Hands ActionSet", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
	actionset_info.priority = 0;

	result = xrCreateActionSet(xr->instance, &actionset_info, &bindings->actionset);
	assert(XR_SUCCEEDED(result) && "Failed to create actionset.");

	{
		XrActionCreateInfo action_info = { 0 };
		action_info.type = XR_TYPE_ACTION_CREATE_INFO;
		action_info.next = NULL;
		strncpy_s(action_info.actionName, XR_MAX_ACTION_NAME_SIZE, 
			"handpose", XR_MAX_ACTION_NAME_SIZE);
		action_info.actionType = XR_ACTION_TYPE_POSE_INPUT;
		action_info.countSubactionPaths = RLOPENXR_HAND_COUNT;
		action_info.subactionPaths = bindings->hand_paths;
		strncpy_s(action_info.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, 
			"Hand Pose", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);

		result = xrCreateAction(bindings->actionset, &action_info, &bindings->hand_pose_action);
		assert(XR_SUCCEEDED(result) && "Failed to create hand pose action");
	}

	// poses can't be queried directly, we need to create a space for each
	for (int hand = 0; hand < RLOPENXR_HAND_COUNT; hand++) {
		XrPosef identity_pose = { { 0, 0, 0, 1}, {0, 0, 0} };

		XrActionSpaceCreateInfo action_space_info = { 0 };
		action_space_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
		action_space_info.next = NULL;
		action_space_info.action = bindings->hand_pose_action;
		action_space_info.subactionPath = bindings->hand_paths[hand];
		action_space_info.poseInActionSpace = identity_pose;

		result = xrCreateActionSpace(xr->session, &action_space_info, &bindings->hand_spaces[hand]);
		assert(XR_SUCCEEDED(result) && "failed to create hand %d pose space");
	}

	XrPath grip_pose_path[2] = { 0 };
	xrStringToPath(xr->instance, "/user/hand/left/input/grip/pose", &grip_pose_path[RLOPENXR_HAND_LEFT]);
	xrStringToPath(xr->instance, "/user/hand/right/input/grip/pose", &grip_pose_path[RLOPENXR_HAND_RIGHT]);

	// khr/simple_controller Interaction Profile
	{
		XrPath interaction_profile_path;
		result = xrStringToPath(xr->instance, "/interaction_profiles/khr/simple_controller", &interaction_profile_path);
		assert(XR_SUCCEEDED(result) && "failed to get interaction profile");

		XrActionSuggestedBinding action_suggested_bindings[] = {
			{ bindings->hand_pose_action, grip_pose_path[RLOPENXR_HAND_LEFT]},
			{ bindings->hand_pose_action, grip_pose_path[RLOPENXR_HAND_RIGHT]}
		};
		const int action_suggested_bindings_count = sizeof(action_suggested_bindings) / sizeof(action_suggested_bindings[0]);

		XrInteractionProfileSuggestedBinding suggested_bindings = { 0 };
		suggested_bindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggested_bindings.next = NULL;
		suggested_bindings.interactionProfile = interaction_profile_path;
		suggested_bindings.countSuggestedBindings = action_suggested_bindings_count;
		suggested_bindings.suggestedBindings = action_suggested_bindings;

		result = xrSuggestInteractionProfileBindings(xr->instance, &suggested_bindings);
		assert(XR_SUCCEEDED(result) && "failed to suggest bindings for khr/simple_controller");
	}

	// oculus/touch_controller Interaction Profile
	{
		XrPath interaction_profile_path;
		result = xrStringToPath(xr->instance, "/interaction_profiles/oculus/touch_controller", &interaction_profile_path);
		assert(XR_SUCCEEDED(result) && "failed to get interaction profile");

		XrActionSuggestedBinding action_suggested_bindings[2] = {
			{ bindings->hand_pose_action, grip_pose_path[RLOPENXR_HAND_LEFT]},
			{ bindings->hand_pose_action, grip_pose_path[RLOPENXR_HAND_RIGHT]},
		};
		const int action_suggested_bindings_count = sizeof(action_suggested_bindings) / sizeof(action_suggested_bindings[0]);

		XrInteractionProfileSuggestedBinding suggested_bindings = { 0 };
		suggested_bindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggested_bindings.next = NULL;
		suggested_bindings.interactionProfile = interaction_profile_path;
		suggested_bindings.countSuggestedBindings = action_suggested_bindings_count;
		suggested_bindings.suggestedBindings = action_suggested_bindings;

		result = xrSuggestInteractionProfileBindings(xr->instance, &suggested_bindings);
		assert(XR_SUCCEEDED(result) && "failed to suggest bindings for oculus/touch_controller");
	}

	XrSessionActionSetsAttachInfo actionset_attach_info = { 0 };
	actionset_attach_info.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
	actionset_attach_info.next = NULL;
	actionset_attach_info.countActionSets = 1;
	actionset_attach_info.actionSets = &bindings->actionset;
	result = xrAttachSessionActionSets(xr->session, &actionset_attach_info);
	assert(XR_SUCCEEDED(result) && "failed to attach action set");
}

void assign_hand_input_bindings(XRInputBindings* bindings, RLHand* left, RLHand* right)
{
	RLHand* hands[2] = { left, right };

	for (int i = 0; i < RLOPENXR_HAND_COUNT; ++i)
	{
		hands[i]->hand_pose_action = bindings->hand_pose_action;
		hands[i]->hand_pose_subpath = bindings->hand_paths[i];
		hands[i]->hand_pose_space = bindings->hand_spaces[i];
	}
}
