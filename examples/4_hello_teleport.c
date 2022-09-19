#include "rlOpenXR.h"

#include "raylib.h"
#include "raymath.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// Constants
const float TELEPORT_ARC_SPEED = 7.0f;
const float TELEPORT_ARC_GRAVITY = 9.81f;

// Data structures
typedef struct
{
	XrActionSet actionset;

	XrAction hand_pose_action;
	XrPath hand_sub_paths[2];
	XrSpace hand_spaces[2];

	XrAction hand_teleport_action;
} XRInputBindings;

// Function prototypes
float calculate_parabola_time_to_floor(Vector3 hand_position, Quaternion hand_orientation);
Vector3 sample_parabola_position(Vector3 hand_position, Quaternion hand_orientation, float t);

void setup_input_bindings(XRInputBindings* bindings);
void assign_hand_input_bindings(XRInputBindings* bindings, RLHand* left, RLHand* right);

bool get_action_clicked_this_frame(XrAction action, XrPath sub_path);

// Implementation
int main()
{
	// Initialization
	//--------------------------------------------------------------------------------------
	const int screenWidth = 1200;
	const int screenHeight = 900;

	InitWindow(screenWidth, screenHeight, "rlOpenXR - Hello Teleport");

	if (!rlOpenXRSetup())
	{
		printf("Failed to initialise rlOpenXR!");
		return 1;
	}

	Vector3 stage_position = Vector3Zero();

	Camera local_camera = { 0 };
	local_camera.position = (Vector3){10.0f, 10.0f, 10.0f};
	local_camera.target = (Vector3){ 0.0f, 3.0f, 0.0f };
	local_camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
	local_camera.fovy = 45.0f;
	local_camera.projection = CAMERA_PERSPECTIVE;

	XRInputBindings bindings = { 0 };
	setup_input_bindings(&bindings);

	RLHand left_local_hand = { 0 };
	left_local_hand.handedness = RLOPENXR_HAND_LEFT;
	RLHand right_local_hand = { 0 };
	right_local_hand.handedness = RLOPENXR_HAND_RIGHT;
	assign_hand_input_bindings(&bindings, &left_local_hand, &right_local_hand);

	Model hand_model = LoadModelFromMesh(GenMeshCube(0.2f, 0.2f, 0.2f));

	SetCameraMode(local_camera, CAMERA_FREE);

	SetTargetFPS(-1);	// OpenXR is responsible for waiting in rlOpenXRBegin()
						// Having raylib also do it's VSync causes noticeable input lag

	//--------------------------------------------------------------------------------------

	// Main game loop
	while (!WindowShouldClose())        // Detect window close button or ESC key
	{
		// Update
		//----------------------------------------------------------------------------------

		rlOpenXRUpdate(); // Update OpenXR State

		rlOpenXRSyncSingleActionSet(bindings.actionset); // Simple utility function for simple apps with 1 action set.
														 // xrSyncAction will activate action sets for use.

		rlOpenXRUpdateHands(&left_local_hand, &right_local_hand);

		UpdateCamera(&local_camera); // Use mouse control as a debug option when no HMD is available
		rlOpenXRUpdateCamera(&local_camera); // If the HMD is available, set the local_camera position to the HMD position

		// Camera & Hand positions we get are local to the "stage" (Area where the physical person is standing)
		// Create world space positions for rendering & gameplay
		Camera world_camera = local_camera;
		world_camera.position = Vector3Add(local_camera.position, stage_position);
		world_camera.target = Vector3Add(local_camera.target, stage_position);

		RLHand left_hand = left_local_hand;
		left_hand.position = Vector3Add(left_local_hand.position, stage_position);
		RLHand right_hand = right_local_hand;
		right_hand.position = Vector3Add(right_local_hand.position, stage_position);

		// Teleportation
		if (get_action_clicked_this_frame(bindings.hand_teleport_action, bindings.hand_sub_paths[RLOPENXR_HAND_LEFT]))
		{
			// Next frame, this will be the new stage position
			stage_position = sample_parabola_position(left_hand.position, left_hand.orientation,
				calculate_parabola_time_to_floor(left_hand.position, left_hand.orientation));
		}

		// Draw
		//----------------------------------------------------------------------------------
		ClearBackground(RAYWHITE); // Clear window, in case rlOpenXR skips rendering the frame, we don't have garbage data in the backbuffer

		// rlOpenXRBegin() returns false when OpenXR reports to skip the frame (The HMD is inactive).
		// Optionally rlOpenXRBeginMockHMD() can be chained to always render. It will render into a "Mock" backbuffer.
		if (rlOpenXRBegin() || rlOpenXRBeginMockHMD()) // Render to OpenXR backbuffer
		{
			ClearBackground(SKYBLUE);

			BeginMode3D(world_camera);

				// Draw Hands
				Vector3 left_hand_axis;
				float left_hand_angle;
				QuaternionToAxisAngle(left_hand.orientation, &left_hand_axis, &left_hand_angle);

				Vector3 right_hand_axis;
				float right_hand_angle;
				QuaternionToAxisAngle(right_hand.orientation, &right_hand_axis, &right_hand_angle);

				DrawModelEx(hand_model, left_hand.position, left_hand_axis, left_hand_angle * RAD2DEG, Vector3One(), ORANGE);
				DrawModelEx(hand_model, right_hand.position, right_hand_axis, right_hand_angle * RAD2DEG, Vector3One(), PINK);

				// Draw Teleportation Arc
				const float t = calculate_parabola_time_to_floor(left_hand.position, left_hand.orientation);

				const int ARC_SEGMENTS = 50;
				for (int i = 1; i <= ARC_SEGMENTS; ++i)
				{
					float interpolation_t_0 = t / ARC_SEGMENTS * (i - 1);
					float interpolation_t_1 = t / ARC_SEGMENTS * i;
					Vector3 arc_position_0 = sample_parabola_position(left_hand.position, left_hand.orientation, interpolation_t_0);
					Vector3 arc_position_1 = sample_parabola_position(left_hand.position, left_hand.orientation, interpolation_t_1);
					DrawCylinderEx(arc_position_0, arc_position_1, 0.05f, 0.05f, 12, DARKBLUE);
				}

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
			DrawText("Controls: \n    Teleport = Left hand 'X' button", 10, 35, 20, BLACK);

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

// Arc Math
float calculate_parabola_time_to_floor(Vector3 hand_position, Quaternion hand_orientation)
{
	// Evaluate t = -(-V0 ± sqrt(2 * g * y0 + V0^2)) / g
	// Where y0: Start height
	//		 V0: Arc initial y velocity
	//		 g: Gravity constant
	//		 t: Time along arc

	const Vector3 hand_forward = Vector3RotateByQuaternion((Vector3) { 0, -1, 0 }, hand_orientation);
	const Vector3 initial_vel = Vector3Scale(hand_forward, TELEPORT_ARC_SPEED);

	const float g = TELEPORT_ARC_GRAVITY;

	const float t_0 =
		-( -initial_vel.y - sqrtf(2 * g * hand_position.y + initial_vel.y * initial_vel.y) )
		/ g;
	const float t_1 =
		-( -initial_vel.y + sqrtf(2 * g * hand_position.y + initial_vel.y * initial_vel.y) )
		/ g;

	return fmaxf(t_0, t_1);
}

Vector3 sample_parabola_position(Vector3 hand_position, Quaternion hand_orientation, float t)
{
	// Evaulate y = y0 + V0*t - 0.5*g*t^2
	// Where y0: Start height
	//		 V0: Arc initial y velocity
	//		 g: Gravity constant
	//		 t: Time along arc

	const Vector3 hand_forward = Vector3RotateByQuaternion((Vector3) { 0, -1, 0 }, hand_orientation);
	const Vector3 initial_vel = Vector3Scale(hand_forward, TELEPORT_ARC_SPEED);

	const float g = TELEPORT_ARC_GRAVITY;

	const float y_at_t = hand_position.y + initial_vel.y * t - 0.5f * g * t * t;

	Vector3 sampled_position = Vector3Add(hand_position, Vector3Scale(initial_vel, t));
	sampled_position.y = y_at_t;
	return sampled_position;
}

// Input
void setup_input_bindings(XRInputBindings* bindings)
{
	const RLOpenXRData* xr = rlOpenXRData();

	XrResult result = xrStringToPath(xr->instance, "/user/hand/left", &bindings->hand_sub_paths[RLOPENXR_HAND_LEFT]);
	assert(XR_SUCCEEDED(result) && "Could not convert Left hand string to path.");
	result = xrStringToPath(xr->instance, "/user/hand/right", &bindings->hand_sub_paths[RLOPENXR_HAND_RIGHT]);
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
		action_info.subactionPaths = bindings->hand_sub_paths;
		strncpy_s(action_info.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, 
			"Hand Pose", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);

		result = xrCreateAction(bindings->actionset, &action_info, &bindings->hand_pose_action);
		assert(XR_SUCCEEDED(result) && "Failed to create hand pose action");
	}

	{
		XrActionCreateInfo action_info = { 0 };
		action_info.type = XR_TYPE_ACTION_CREATE_INFO;
		action_info.next = NULL;
		strncpy_s(action_info.actionName, XR_MAX_ACTION_NAME_SIZE, 
			"activate", XR_MAX_ACTION_NAME_SIZE);
		action_info.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		action_info.countSubactionPaths = RLOPENXR_HAND_COUNT;
		action_info.subactionPaths = bindings->hand_sub_paths;
		strncpy_s(action_info.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, 
			"Activate", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);

		result = xrCreateAction(bindings->actionset, &action_info, &bindings->hand_teleport_action);
		assert(XR_SUCCEEDED(result) && "Failed to create hand activate action");
	}

	// poses can't be queried directly, we need to create a space for each
	for (int hand = 0; hand < RLOPENXR_HAND_COUNT; hand++) {
		XrPosef identity_pose = { { 0, 0, 0, 1}, {0, 0, 0} };

		XrActionSpaceCreateInfo action_space_info = { 0 };
		action_space_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
		action_space_info.next = NULL;
		action_space_info.action = bindings->hand_pose_action;
		action_space_info.subactionPath = bindings->hand_sub_paths[hand];
		action_space_info.poseInActionSpace = identity_pose;

		result = xrCreateActionSpace(xr->session, &action_space_info, &bindings->hand_spaces[hand]);
		assert(XR_SUCCEEDED(result) && "failed to create hand %d pose space");
	}

	XrPath grip_pose_paths[2] = { 0 };
	xrStringToPath(xr->instance, "/user/hand/left/input/grip/pose", &grip_pose_paths[RLOPENXR_HAND_LEFT]);
	xrStringToPath(xr->instance, "/user/hand/right/input/grip/pose", &grip_pose_paths[RLOPENXR_HAND_RIGHT]);

	XrPath teleport_paths[2] = { 0 };
	xrStringToPath(xr->instance, "/user/hand/left/input/x/click", &teleport_paths[RLOPENXR_HAND_LEFT]);
	xrStringToPath(xr->instance, "/user/hand/right/input/a/click", &teleport_paths[RLOPENXR_HAND_RIGHT]);

	// khr/simple_controller Interaction Profile
	{
		XrPath interaction_profile_path;
		result = xrStringToPath(xr->instance, "/interaction_profiles/khr/simple_controller", &interaction_profile_path);
		assert(XR_SUCCEEDED(result) && "failed to get interaction profile");

		XrActionSuggestedBinding action_suggested_bindings[] = {
			{ bindings->hand_pose_action, grip_pose_paths[RLOPENXR_HAND_LEFT] },
			{ bindings->hand_pose_action, grip_pose_paths[RLOPENXR_HAND_RIGHT] },
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

		XrActionSuggestedBinding action_suggested_bindings[] = {
			{ bindings->hand_pose_action, grip_pose_paths[RLOPENXR_HAND_LEFT]},
			{ bindings->hand_pose_action, grip_pose_paths[RLOPENXR_HAND_RIGHT]},
			{ bindings->hand_teleport_action, teleport_paths[RLOPENXR_HAND_LEFT] },
			{ bindings->hand_teleport_action, teleport_paths[RLOPENXR_HAND_RIGHT] },
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
		hands[i]->hand_pose_subpath = bindings->hand_sub_paths[i];
		hands[i]->hand_pose_space = bindings->hand_spaces[i];
	}
}

bool get_action_clicked_this_frame(XrAction action, XrPath sub_path)
{
	XrActionStateGetInfo activate_state_get_info;
	activate_state_get_info.type = XR_TYPE_ACTION_STATE_GET_INFO;
	activate_state_get_info.next = NULL;
	activate_state_get_info.action = action;
	activate_state_get_info.subactionPath = sub_path;

	XrActionStateBoolean activate_state;
	activate_state.type = XR_TYPE_ACTION_STATE_BOOLEAN;
	activate_state.next = NULL;
	XrResult result = xrGetActionStateBoolean(rlOpenXRData()->session, &activate_state_get_info, &activate_state);
	assert(XR_SUCCEEDED(result) && "failed to get action state as a float");

	return activate_state.changedSinceLastSync && activate_state.currentState;
}
