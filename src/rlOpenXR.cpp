#include "rlOpenXR.h"

#include "platform/rlOpenXRWin32Wrapper.h"
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"
#include "external/glad.h"
#include "external/cgltf.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <array>
#include <cassert>
#include <memory>
#include <vector>
#include <cstdarg>


// Types
// ============================================================================

template<typename T>
using Two = std::array<T, 2>;

struct RLExtraHandData
{
	// Data
	cgltf_data* controller_model = nullptr;

	// Construction & Deconstruction
	RLExtraHandData() = default;
	~RLExtraHandData()
	{
		cgltf_free(controller_model);
	}

	// Move only
	RLExtraHandData(const RLExtraHandData& other) = delete;
	RLExtraHandData(RLExtraHandData&& other) noexcept
	{
		std::swap(controller_model, other.controller_model);
	}
	RLExtraHandData& operator=(const RLExtraHandData& other) = delete;
	RLExtraHandData& operator=(RLExtraHandData&& other) noexcept
	{
		std::swap(controller_model, other.controller_model);
	}
};


// Constants
// ============================================================================

constexpr int c_view_count = 2;
constexpr int c_hand_count = 2;
constexpr int c_hand_left_index = 0;
constexpr int c_hand_right_index = 1;

// These should probably be configurable
constexpr XrViewConfigurationType c_view_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
constexpr XrFormFactor c_form_factor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
constexpr XrReferenceSpaceType c_play_space_type = XR_REFERENCE_SPACE_TYPE_STAGE;


// State
// ============================================================================

struct RLOpenXRDataExtensions
{
	// Required extensions
	PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR = nullptr;
	PFN_xrConvertWin32PerformanceCounterToTimeKHR xrConvertWin32PerformanceCounterToTimeKHR = nullptr;

	// Optional extensions
	PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT = nullptr;
	XrDebugUtilsMessengerEXT debug_messenger_handle = XR_NULL_HANDLE;

	bool depth_enabled = false;
};

struct RLOpenXRData
{
	// Data
	XrInstance instance = XR_NULL_HANDLE; // the instance handle can be thought of as the basic connection to the OpenXR runtime
	XrSystemId system_id = XR_NULL_SYSTEM_ID; // the system represents an (opaque) set of XR devices in use, managed by the runtime
	XrSession session = XR_NULL_HANDLE; // the session deals with the renderloop submitting frames to the runtime

	XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;

	RLOpenXRDataExtensions extensions;

	// TODO: Support more than windows
	XrGraphicsBindingOpenGLWin32KHR graphics_binding_gl;

	XrFrameState frame_state{ XR_TYPE_FRAME_STATE };

	XrSpace play_space = XR_NULL_HANDLE;
	XrSpace view_space = XR_NULL_HANDLE;

	XrActionSet internal_actionset = XR_NULL_HANDLE;
	XrAction hand_pose_action = XR_NULL_HANDLE;
	Two<XrPath> hand_paths = { 0, 0 };
	Two<XrSpace> hand_spaces = { XR_NULL_HANDLE, XR_NULL_HANDLE };
	Two<RLHand> hands{};
	Two<RLExtraHandData> hands_internal{};

	bool session_running = false; // to avoid beginning an already running session
	bool run_framecycle = false;  // for some session states skip the frame cycle

	std::vector<XrViewConfigurationView> viewconfig_views; // array of view_count configuration view, contain information like resolution about each view
	std::vector<XrCompositionLayerProjectionView> projection_views; // array of view_count containers for submitting swapchains with rendered VR frames
	std::vector<XrCompositionLayerDepthInfoKHR> depth_infos; // extends projection_views
	
	XrCompositionLayerProjection layer_projection{ XR_TYPE_COMPOSITION_LAYER_PROJECTION }; // Composition layer of all the views
	std::vector<XrCompositionLayerBaseHeader*> layers_pointers; // Composition layers (Will point to `layer_projection`)
	std::vector<XrView> views; // array of view_count views, filled by the runtime with current HMD display pose

	XrSwapchain swapchain = XR_NULL_HANDLE;
	std::vector<XrSwapchainImageOpenGLKHR> swapchain_images;
	XrSwapchain depth_swapchain = XR_NULL_HANDLE;
	std::vector<XrSwapchainImageOpenGLKHR> depth_swapchain_images;

	unsigned int fbo = 0;
	RenderTexture mock_hmd_rt{0};
	unsigned int active_fbo = 0;

	// Construction & Deconstruction
	RLOpenXRData() = default;
	~RLOpenXRData() = default;

	// Not copyable & not moveable
	RLOpenXRData(const RLOpenXRData&) = delete;
	RLOpenXRData(RLOpenXRData&&) = delete;
	RLOpenXRData& operator=(const RLOpenXRData&) = delete;
	RLOpenXRData& operator=(RLOpenXRData&&) = delete;
};

static std::unique_ptr<RLOpenXRData> s_xr;


// Helpers
//=============================================================================

static bool xr_check(XrResult result, const char* format, ...)
{
	if (XR_SUCCEEDED(result))
		return true;

	char resultString[XR_MAX_RESULT_STRING_SIZE];
	if (s_xr != nullptr && s_xr->instance != XR_NULL_HANDLE)
	{
		xrResultToString(s_xr->instance, result, resultString);
	}
	else
	{
		snprintf(resultString, XR_MAX_RESULT_STRING_SIZE, "Error XrResult(%d)", result);
	}

	char formatRes[XR_MAX_RESULT_STRING_SIZE + 1024];
	snprintf(formatRes, XR_MAX_RESULT_STRING_SIZE + 1023, "%s [%s] (%d)\n", format, resultString,
		result);

	va_list args;
	va_start(args, format);
	vprintf(formatRes, args);
	va_end(args);

	return false;
}

// Print helpers
static void print_instance_properties(XrInstance instance)
{
	XrResult result;
	XrInstanceProperties instance_props = {
		.type = XR_TYPE_INSTANCE_PROPERTIES,
		.next = NULL,
	};

	result = xrGetInstanceProperties(instance, &instance_props);
	if (!xr_check(result, "Failed to get instance info"))
		return;

	printf("Runtime Name: %s\n", instance_props.runtimeName);
	printf("Runtime Version: %d.%d.%d\n", XR_VERSION_MAJOR(instance_props.runtimeVersion),
		XR_VERSION_MINOR(instance_props.runtimeVersion),
		XR_VERSION_PATCH(instance_props.runtimeVersion));
}

static void print_system_properties(XrSystemProperties* system_properties)
{
	printf("System properties for system %lu: \"%s\", vendor ID %d\n", system_properties->systemId,
		system_properties->systemName, system_properties->vendorId);
	printf("\tMax layers          : %d\n", system_properties->graphicsProperties.maxLayerCount);
	printf("\tMax swapchain height: %d\n",
		system_properties->graphicsProperties.maxSwapchainImageHeight);
	printf("\tMax swapchain width : %d\n",
		system_properties->graphicsProperties.maxSwapchainImageWidth);
	printf("\tOrientation Tracking: %d\n", system_properties->trackingProperties.orientationTracking);
	printf("\tPosition Tracking   : %d\n", system_properties->trackingProperties.positionTracking);
}

static void print_viewconfig_view_info(uint32_t view_count, XrViewConfigurationView* viewconfig_views)
{
	for (uint32_t i = 0; i < view_count; i++) {
		printf("View Configuration View %d:\n", i);
		printf("\tResolution       : Recommended %dx%d, Max: %dx%d\n",
			viewconfig_views[0].recommendedImageRectWidth,
			viewconfig_views[0].recommendedImageRectHeight, viewconfig_views[0].maxImageRectWidth,
			viewconfig_views[0].maxImageRectHeight);
		printf("\tSwapchain Samples: Recommended: %d, Max: %d)\n",
			viewconfig_views[0].recommendedSwapchainSampleCount,
			viewconfig_views[0].maxSwapchainSampleCount);
	}
}

// Adapted from openxr-simple-example @ https://gitlab.freedesktop.org/monado/demos/openxr-simple-example/-/blob/master/main.c
static Matrix xr_projection_matrix(const XrFovf& fov)
{
	static_assert(RL_CULL_DISTANCE_FAR > RL_CULL_DISTANCE_NEAR, "rlOpenXR doesn't support infinite far plane distances");

	Matrix matrix{};

	const float tanAngleLeft = tanf(fov.angleLeft);
	const float tanAngleRight = tanf(fov.angleRight);

	const float tanAngleDown = tanf(fov.angleDown);
	const float tanAngleUp = tanf(fov.angleUp);

	const float tanAngleWidth = tanAngleRight - tanAngleLeft;
	const float tanAngleHeight = tanAngleUp - tanAngleDown;

	matrix.m0 = 2 / tanAngleWidth;
	matrix.m4 = 0;
	matrix.m8 = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
	matrix.m12 = 0;

	matrix.m1 = 0;
	matrix.m5 = 2 / tanAngleHeight;
	matrix.m9 = (tanAngleUp + tanAngleDown) / tanAngleHeight;
	matrix.m13 = 0;

	matrix.m2 = 0;
	matrix.m6 = 0;
	matrix.m10 = -(RL_CULL_DISTANCE_FAR + RL_CULL_DISTANCE_NEAR) / (RL_CULL_DISTANCE_FAR - RL_CULL_DISTANCE_NEAR);
	matrix.m14 = -(RL_CULL_DISTANCE_FAR * (RL_CULL_DISTANCE_NEAR + RL_CULL_DISTANCE_NEAR)) / (RL_CULL_DISTANCE_FAR - RL_CULL_DISTANCE_NEAR);

	matrix.m3 = 0;
	matrix.m7 = 0;
	matrix.m11 = -1;
	matrix.m15 = 0;

	return matrix;
}

static Matrix xr_matrix(const XrPosef& pose)
{
	Matrix translation = MatrixTranslate(pose.position.x, pose.position.y, pose.position.z);
	Matrix rotation = QuaternionToMatrix(Quaternion{pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w});
	return MatrixMultiply(rotation, translation);
}

// we need an identity pose for creating spaces without offsets
static XrPosef identity_pose = { .orientation = {.x = 0, .y = 0, .z = 0, .w = 1.0},
								.position = {.x = 0, .y = 0, .z = 0} };

// Temp thingy
static XrBool32 my_xrDebugUtilsMessengerCallback(
	XrDebugUtilsMessageSeverityFlagsEXT              messageSeverity,
	XrDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData)
{
	printf("xrDebugUtilsMessengerCallback: %s\n", callbackData->message);
	int i = 3;

	return XR_FALSE;
}


// Functions
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

bool rlOpenXRSetup()
{
	assert(s_xr == nullptr);
	s_xr = std::make_unique<RLOpenXRData>();

	XrResult result = XR_SUCCESS;

	//print_api_layers();

	// xrEnumerate*() functions are usually called once with CapacityInput = 0.
	// The function will write the required amount into CountOutput. We then have
	// to allocate an array to hold CountOutput elements and call the function
	// with CountOutput as CapacityInput.
	uint32_t ext_count = 0;
	result = xrEnumerateInstanceExtensionProperties(NULL, 0, &ext_count, NULL);
	if (XR_FAILED(result))
	{
		printf("Failed to enumerate number of extension properties. error code: %d\n", result);
		return false;
	}

	std::vector<XrExtensionProperties> ext_props{ ext_count, { XR_TYPE_EXTENSION_PROPERTIES } };

	result = xrEnumerateInstanceExtensionProperties(NULL, ext_count, &ext_count, &ext_props[0]);
	if (XR_FAILED(result))
	{
		printf("Failed to enumerate number of extension properties. error code: %d\n", result);
		return false;
	}

	bool opengl_supported = false;
	std::vector enabled_exts{ XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_EXT_DEBUG_UTILS_EXTENSION_NAME, XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME };

	printf("Runtime supports %d extensions\n", ext_count);
	for (uint32_t i = 0; i < ext_count; i++) {
		printf("\t%s v%d\n", ext_props[i].extensionName, ext_props[i].extensionVersion);

		if (strcmp(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, ext_props[i].extensionName) == 0) {
			opengl_supported = true;
		}

		if (strcmp(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME, ext_props[i].extensionName) == 0) {
			s_xr->extensions.depth_enabled = true;
			enabled_exts.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
		}

		if (strcmp(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME, ext_props[i].extensionName) == 0) {
			enabled_exts.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
		}
	}

	if (!opengl_supported) 
	{
		printf("Runtime does not support OpenGL extension!\n");
		return false;
	}

	// --- Create XrInstance
	// same can be done for API layers, but API layers can also be enabled by env var

	XrInstanceCreateInfo instance_create_info = {
		.type = XR_TYPE_INSTANCE_CREATE_INFO,
		.next = NULL,
		.createFlags = 0,
		.applicationInfo = {
			.applicationVersion = 1,
			.engineVersion = 0,
			.apiVersion = XR_CURRENT_API_VERSION,
		},
		.enabledApiLayerCount = 0,
		.enabledApiLayerNames = NULL,
		.enabledExtensionCount = (uint32_t)enabled_exts.size(),
		.enabledExtensionNames = enabled_exts.data(),
	};
	strcpy_s(instance_create_info.applicationInfo.applicationName, "rlOpenXR Application"); // TODO: Do we want this to be exposed? Does it have any purpose?
	strcpy_s(instance_create_info.applicationInfo.engineName, "Raylib (rlOpenXR)");

	result = xrCreateInstance(&instance_create_info, &s_xr->instance);
	if (!xr_check(result, "Failed to create XR instance."))
		return false;

	result = xrGetInstanceProcAddr(s_xr->instance, "xrGetOpenGLGraphicsRequirementsKHR",
			(PFN_xrVoidFunction*)&s_xr->extensions.xrGetOpenGLGraphicsRequirementsKHR);
	if (!xr_check(result, "Failed to get OpenGL graphics requirements function!"))
		return false;

	result = xrGetInstanceProcAddr(s_xr->instance, "xrConvertWin32PerformanceCounterToTimeKHR",
		(PFN_xrVoidFunction*)&s_xr->extensions.xrConvertWin32PerformanceCounterToTimeKHR);
	if (!xr_check(result, "Failed to get xrConvertWin32PerformanceCounterToTimeKHR function!"))
		return false;

	result = xrGetInstanceProcAddr(s_xr->instance, "xrCreateDebugUtilsMessengerEXT",
			(PFN_xrVoidFunction*)&s_xr->extensions.xrCreateDebugUtilsMessengerEXT);
	if (!xr_check(result, "Failed to get xrCreateDebugUtilsMessengerEXT function!"))
		return false;

	XrDebugUtilsMessengerCreateInfoEXT debug_message_create_info{
		.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.next = nullptr,
		.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT,
		.userCallback = &my_xrDebugUtilsMessengerCallback,
		.userData = nullptr
	};

	// TODO: Only enable on RLGL_ENABLE_OPENGL_DEBUG_CONTEXT
	// TODO: This leaks the handle, should be destroyed too
	result = s_xr->extensions.xrCreateDebugUtilsMessengerEXT(s_xr->instance, &debug_message_create_info, &s_xr->extensions.debug_messenger_handle);
	if (!xr_check(result, "Failed create debug messenger!"))
		return false;

	// Optionally get runtime name and version
	print_instance_properties(s_xr->instance);

	// --- Get XrSystemId
	XrSystemGetInfo system_get_info = {
		.type = XR_TYPE_SYSTEM_GET_INFO, .next = NULL, .formFactor = c_form_factor };

	result = xrGetSystem(s_xr->instance, &system_get_info, &s_xr->system_id);
	if (!xr_check(result, "Failed to get system for HMD form factor."))
		return false;

	printf("Successfully got XrSystem with id %llu for HMD form factor\n", s_xr->system_id);

	{
		XrSystemProperties system_props = {
			.type = XR_TYPE_SYSTEM_PROPERTIES,
			.next = NULL,
		};

		result = xrGetSystemProperties(s_xr->instance, s_xr->system_id, &system_props);
		if (!xr_check(result, "Failed to get System properties"))
			return false;

		print_system_properties(&system_props);
	}

	uint32_t view_count;
	result = xrEnumerateViewConfigurationViews(s_xr->instance, s_xr->system_id, c_view_type, 0, &view_count, NULL);
	if (!xr_check(result, "Failed to get view configuration view count!"))
		return 1;

	s_xr->viewconfig_views.resize(view_count, XrViewConfigurationView{ .type = XR_TYPE_VIEW_CONFIGURATION_VIEW , .next = nullptr });
	
	result = xrEnumerateViewConfigurationViews(s_xr->instance, s_xr->system_id, c_view_type, view_count,
		&view_count, &s_xr->viewconfig_views[0]);
	if (!xr_check(result, "Failed to enumerate view configuration views!"))
		return 1;
	print_viewconfig_view_info(view_count, &s_xr->viewconfig_views[0]);


	// this function pointer was loaded with xrGetInstanceProcAddr
	// OpenXR requires checking graphics requirements before creating a session.
	XrGraphicsRequirementsOpenGLKHR opengl_reqs = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR,
												   .next = NULL };


	result = s_xr->extensions.xrGetOpenGLGraphicsRequirementsKHR(s_xr->instance, s_xr->system_id, &opengl_reqs);
	if (!xr_check(result, "Failed to get OpenGL graphics requirements!"))
		return false;

	auto min_major = XR_VERSION_MAJOR(opengl_reqs.minApiVersionSupported);
	auto min_minor = XR_VERSION_MINOR(opengl_reqs.minApiVersionSupported);
	auto min_patch = XR_VERSION_PATCH(opengl_reqs.minApiVersionSupported);

	auto max_major = XR_VERSION_MAJOR(opengl_reqs.maxApiVersionSupported);
	auto max_minor = XR_VERSION_MINOR(opengl_reqs.maxApiVersionSupported);
	auto max_patch = XR_VERSION_PATCH(opengl_reqs.maxApiVersionSupported);

	int major, minor;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);

	printf("OpenXR OpenGL requirements, min: %d.%d.%d, max: %d.%d.%d, got: %d.%d\n", 
		min_major, min_minor, min_patch,
		max_major, max_minor, max_patch,
		major, minor
		);

	// --- Create session
	// Assume the calling thread is the one initialised by raylib
	auto graphics_binding_gl = XrGraphicsBindingOpenGLWin32KHR{
		.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,
		.next = nullptr,
		.hDC = wrapped_wglGetCurrentDC(),
		.hGLRC = wrapped_wglGetCurrentContext()
	};

	assert(graphics_binding_gl.hDC != NULL);
	assert(graphics_binding_gl.hGLRC != NULL);

	XrSessionCreateInfo session_create_info = {
		.type = XR_TYPE_SESSION_CREATE_INFO, .next = &graphics_binding_gl, .systemId = s_xr->system_id };

	result = xrCreateSession(s_xr->instance, &session_create_info, &s_xr->session);
	if (!xr_check(result, "Failed to create session"))
		return 1;

	printf("Successfully created a session with OpenGL!\n");

	/* Many runtimes support at least STAGE and LOCAL but not all do.
	 * Sophisticated apps might check with xrEnumerateReferenceSpaces() if the
	 * chosen one is supported and try another one if not.
	 * Here we will get an error from xrCreateReferenceSpace() and exit. */
	XrReferenceSpaceCreateInfo play_space_create_info = { .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
														 .next = NULL,
														 .referenceSpaceType = c_play_space_type,
														 .poseInReferenceSpace = identity_pose };

	result = xrCreateReferenceSpace(s_xr->session, &play_space_create_info, &s_xr->play_space);
	if (!xr_check(result, "Failed to create play space!"))
		return false;

	XrReferenceSpaceCreateInfo view_space_create_info = { .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
													 .next = NULL,
													 .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW,
													 .poseInReferenceSpace = identity_pose };

	result = xrCreateReferenceSpace(s_xr->session, &view_space_create_info, &s_xr->view_space);
	if (!xr_check(result, "Failed to create view space!"))
		return false;

	// --- Create Swapchains
	uint32_t supported_gl_internal_format_count;
	result = xrEnumerateSwapchainFormats(s_xr->session, 0, &supported_gl_internal_format_count, NULL);
	if (!xr_check(result, "Failed to get number of supported swapchain formats"))
		return false;

	printf("Runtime supports %d swapchain formats\n", supported_gl_internal_format_count);

	std::vector<int64_t> supported_gl_internal_formats;
	supported_gl_internal_formats.resize(supported_gl_internal_format_count);
	result = xrEnumerateSwapchainFormats(s_xr->session, supported_gl_internal_format_count, &supported_gl_internal_format_count,
		&supported_gl_internal_formats[0]);
	if (!xr_check(result, "Failed to enumerate swapchain formats"))
		return false;

	uint32_t swapchain_width = 0;
	for (uint32_t i = 0; i < view_count; i++)
	{
		swapchain_width += s_xr->viewconfig_views[i].recommendedImageRectWidth;

		// TODO: assert recommendedSwapchainSampleCounts & recommendedImageRectHeights are the same for each view
	}

	s_xr->fbo = rlLoadFramebuffer(swapchain_width, s_xr->viewconfig_views[0].recommendedImageRectHeight);
	
	// TODO: Better way to choose swapchain format than hardcoding it
	const int color_gl_internal_format = GL_SRGB8_ALPHA8;
	const auto color_format_name = "GL_SRGB8_ALPHA8";

	if (std::find(supported_gl_internal_formats.begin(), supported_gl_internal_formats.end(), color_gl_internal_format)
		== supported_gl_internal_formats.end())
	{
		printf("rlOpenXR render texture has color format '%s' which is not supported by this OpenXR driver.\n", color_format_name);
		return false;
	}

	// TODO: Higher res depth buffer
	int depth_gl_internal_format = GL_DEPTH_COMPONENT16;
	const auto depth_format_name = "GL_DEPTH_COMPONENT16";
	
	if (std::find(supported_gl_internal_formats.begin(), supported_gl_internal_formats.end(), depth_gl_internal_format)
		== supported_gl_internal_formats.end())
	{
		printf("rlOpenXR render texture has depth format '%s' which is not supported by this OpenXR driver. Disabling depth\n", depth_format_name);
		s_xr->extensions.depth_enabled = false;
	}

	// --- Create swapchain for main VR rendering
	{
		// In the frame loop we render into OpenGL textures we receive from the runtime here.
		XrSwapchainCreateInfo swapchain_create_info = {
			.type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
			.next = NULL,
			.createFlags = 0,
			.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
			.format = color_gl_internal_format,
			//TODO: Get multisampling enabled from the Raylib hint
			.sampleCount = s_xr->viewconfig_views[0].recommendedSwapchainSampleCount,
			.width = swapchain_width,
			.height = s_xr->viewconfig_views[0].recommendedImageRectHeight,
			.faceCount = 1,
			.arraySize = 1,
			.mipCount = 1,
		};

		result = xrCreateSwapchain(s_xr->session, &swapchain_create_info, &s_xr->swapchain);
		if (!xr_check(result, "Failed to create swapchain!"))
			return 1;

		// The runtime controls how many textures we have to be able to render to
		// (e.g. "triple buffering")
		uint32_t swapchain_image_count;
		result = xrEnumerateSwapchainImages(s_xr->swapchain, 0, &swapchain_image_count, NULL);
		if (!xr_check(result, "Failed to enumerate swapchains"))
			return false;

		s_xr->swapchain_images.resize(swapchain_image_count, { .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, .next = nullptr });
		result = xrEnumerateSwapchainImages(s_xr->swapchain, swapchain_image_count, &swapchain_image_count,
				(XrSwapchainImageBaseHeader*)s_xr->swapchain_images.data());
		if (!xr_check(result, "Failed to enumerate swapchain images"))
			return false;

		printf("Succesfully created OpenXR color swapchain with format: %s. Dimensions: %d, %d\n", 
			color_format_name, swapchain_create_info.width, swapchain_create_info.height);
	}

	// --- Create swapchain for depth buffers if supported
	{
		if (s_xr->extensions.depth_enabled) {

			uint32_t depth_swapchain_width = 0;
			for (uint32_t i = 0; i < view_count; i++)
			{
				depth_swapchain_width += s_xr->viewconfig_views[i].recommendedImageRectWidth;

				// TODO: assert recommendedSwapchainSampleCounts & recommendedImageRectHeights are the same for each view
			}

			XrSwapchainCreateInfo swapchain_create_info = {
				.type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
				.next = NULL,
				.createFlags = 0,
				.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				.format = depth_gl_internal_format,
				.sampleCount = s_xr->viewconfig_views[0].recommendedSwapchainSampleCount,
				.width = depth_swapchain_width,
				.height = s_xr->viewconfig_views[0].recommendedImageRectHeight,
				.faceCount = 1,
				.arraySize = 1,
				.mipCount = 1,
			};

			result = xrCreateSwapchain(s_xr->session, &swapchain_create_info, &s_xr->depth_swapchain);
			if (!xr_check(result, "Failed to create swapchain!"))
				return false;

			uint32_t depth_swapchain_image_count;
			result = xrEnumerateSwapchainImages(s_xr->depth_swapchain, 0, &depth_swapchain_image_count, NULL);
			if (!xr_check(result, "Failed to enumerate swapchains"))
				return false;

			// these are wrappers for the actual OpenGL texture id
			s_xr->depth_swapchain_images.resize(depth_swapchain_image_count, {.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, .next = nullptr});
			result = xrEnumerateSwapchainImages(s_xr->depth_swapchain, depth_swapchain_image_count, &depth_swapchain_image_count,
				(XrSwapchainImageBaseHeader*)s_xr->depth_swapchain_images.data());
			if (!xr_check(result, "Failed to enumerate swapchain images"))
				return false;

			printf("Succesfully created OpenXR depth swapchain with format: %s. Dimensions: %d, %d\n",
				depth_format_name, swapchain_create_info.width, swapchain_create_info.height);
		}
	}


	// Do not allocate these every frame to save some resources
	s_xr->views.resize(view_count, { .type = XR_TYPE_VIEW, .next = nullptr });

	s_xr->projection_views.resize(view_count);
	for (uint32_t view = 0; view < view_count; view++) {
		s_xr->projection_views[view].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		s_xr->projection_views[view].next = NULL;

		s_xr->projection_views[view].subImage.swapchain = s_xr->swapchain;
		s_xr->projection_views[view].subImage.imageArrayIndex = 0;
		s_xr->projection_views[view].subImage.imageRect.offset.x = view * s_xr->viewconfig_views[view].recommendedImageRectWidth;
		s_xr->projection_views[view].subImage.imageRect.offset.y = 0;
		s_xr->projection_views[view].subImage.imageRect.extent.width = s_xr->viewconfig_views[view].recommendedImageRectWidth;
		s_xr->projection_views[view].subImage.imageRect.extent.height = s_xr->viewconfig_views[view].recommendedImageRectHeight;

		// projection_views[i].{pose, fov} have to be filled every frame in frame loop
	};


	if (s_xr->extensions.depth_enabled) {
		s_xr->depth_infos.resize(view_count);
		for (uint32_t view = 0; view < view_count; view++) {
			s_xr->depth_infos[view].type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
			s_xr->depth_infos[view].next = NULL;
			s_xr->depth_infos[view].minDepth = 0.f;
			s_xr->depth_infos[view].maxDepth = 1.f;
			s_xr->depth_infos[view].nearZ = RL_CULL_DISTANCE_NEAR;
			s_xr->depth_infos[view].farZ = RL_CULL_DISTANCE_FAR;

			s_xr->depth_infos[view].subImage.swapchain = s_xr->depth_swapchain;
			s_xr->depth_infos[view].subImage.imageArrayIndex = 0;
			s_xr->depth_infos[view].subImage.imageRect.offset.x = view * s_xr->viewconfig_views[view].recommendedImageRectWidth;
			s_xr->depth_infos[view].subImage.imageRect.offset.y = 0;
			s_xr->depth_infos[view].subImage.imageRect.extent.width = s_xr->viewconfig_views[view].recommendedImageRectWidth;
			s_xr->depth_infos[view].subImage.imageRect.extent.height = s_xr->viewconfig_views[view].recommendedImageRectHeight;

			// depth is chained to projection, not submitted as separate layer
			s_xr->projection_views[view].next = &s_xr->depth_infos[view];
		};
	}

	s_xr->layer_projection.layerFlags = 0;
	s_xr->layer_projection.space = s_xr->play_space;
	s_xr->layer_projection.viewCount = view_count;
	s_xr->layer_projection.views = s_xr->projection_views.data();
	s_xr->layers_pointers.push_back((XrCompositionLayerBaseHeader*)&s_xr->layer_projection);

	result = xrStringToPath(s_xr->instance, "/user/hand/left", &s_xr->hand_paths[c_hand_left_index]);
	if (!xr_check(result, "Could not convert Left hand string to path.")) { return false; }
	result = xrStringToPath(s_xr->instance, "/user/hand/right", &s_xr->hand_paths[c_hand_right_index]);
	if (!xr_check(result, "Could not convert Right hand string to path.")) { return false; }

	XrActionSetCreateInfo internal_actionset_info = { .type = XR_TYPE_ACTION_SET_CREATE_INFO, .next = NULL, .priority = 0 };
	strcpy(internal_actionset_info.actionSetName, "rlopenxr_actionset");
	strcpy(internal_actionset_info.localizedActionSetName, "OpenXR Internal Actions");

	result = xrCreateActionSet(s_xr->instance, &internal_actionset_info, &s_xr->internal_actionset);
	if (!xr_check(result, "failed to create actionset"))
		return false;

	{
		XrActionCreateInfo action_info = { .type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_POSE_INPUT,
										  .countSubactionPaths = c_hand_count,
										  .subactionPaths = s_xr->hand_paths.data() };
		strcpy(action_info.actionName, "handpose");
		strcpy(action_info.localizedActionName, "Hand Pose");

		result = xrCreateAction(s_xr->internal_actionset, &action_info, &s_xr->hand_pose_action);
		if (!xr_check(result, "failed to create hand pose action"))
			return false;
	}
	// poses can't be queried directly, we need to create a space for each
	for (int hand = 0; hand < c_hand_count; hand++) {
		XrActionSpaceCreateInfo action_space_info = { .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
													 .next = NULL,
													 .action = s_xr->hand_pose_action,
													 .subactionPath = s_xr->hand_paths[hand],
													 .poseInActionSpace = identity_pose };

		result = xrCreateActionSpace(s_xr->session, &action_space_info, &s_xr->hand_spaces[hand]);
		if (!xr_check(result, "failed to create hand %d pose space", hand))
			return false;
	}

	Two<XrPath> grip_pose_path;
	xrStringToPath(s_xr->instance, "/user/hand/left/input/grip/pose", &grip_pose_path[c_hand_left_index]);
	xrStringToPath(s_xr->instance, "/user/hand/right/input/grip/pose", &grip_pose_path[c_hand_right_index]);

	{
		XrPath interaction_profile_path;
		result = xrStringToPath(s_xr->instance, "/interaction_profiles/khr/simple_controller",
			&interaction_profile_path);
		if (!xr_check(result, "failed to get interaction profile"))
			return false;

		const XrActionSuggestedBinding bindings[]{
			{.action = s_xr->hand_pose_action, .binding = grip_pose_path[c_hand_left_index]},
			{.action = s_xr->hand_pose_action, .binding = grip_pose_path[c_hand_right_index]},
		};

		const XrInteractionProfileSuggestedBinding suggested_bindings = {
			.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
			.next = NULL,
			.interactionProfile = interaction_profile_path,
			.countSuggestedBindings = 2,
			.suggestedBindings = bindings };

		result = xrSuggestInteractionProfileBindings(s_xr->instance, &suggested_bindings);
		if (!xr_check(result, "failed to suggest bindings"))
			return false;
	}

	{
		XrPath interaction_profile_path;
		result = xrStringToPath(s_xr->instance, "/interaction_profiles/oculus/touch_controller",
			&interaction_profile_path);
		if (!xr_check(result, "failed to get interaction profile"))
			return false;

		const XrActionSuggestedBinding bindings[]{
			{.action = s_xr->hand_pose_action, .binding = grip_pose_path[c_hand_left_index]},
			{.action = s_xr->hand_pose_action, .binding = grip_pose_path[c_hand_right_index]},
		};

		const XrInteractionProfileSuggestedBinding suggested_bindings = {
			.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
			.next = NULL,
			.interactionProfile = interaction_profile_path,
			.countSuggestedBindings = 2,
			.suggestedBindings = bindings };

		result = xrSuggestInteractionProfileBindings(s_xr->instance, &suggested_bindings);
		if (!xr_check(result, "failed to suggest bindings"))
			return false;
	}

	XrSessionActionSetsAttachInfo actionset_attach_info = {
		.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
		.next = NULL,
		.countActionSets = 1,
		.actionSets = &s_xr->internal_actionset };
	result = xrAttachSessionActionSets(s_xr->session, &actionset_attach_info);
	if (!xr_check(result, "failed to attach action set"))
		return false;

	return true;
}

void rlOpenXRShutdown()
{
	if (!s_xr)
	{
		printf("%s", "rlOpenXR it not valid! Aborting openXR shutdown\n");
		return;
	}

	rlUnloadFramebuffer(s_xr->fbo);
	UnloadRenderTexture(s_xr->mock_hmd_rt);

	XrResult result = xrDestroyInstance(s_xr->instance);
	if (XR_SUCCEEDED(result))
	{
		printf("%s", "Succesfully shutdown OpenXR.\n");
	}
	else
	{
		printf("Failed to shutdown OpenXR. error code: %d\n", result);
	}

	s_xr.reset();
}

// ----------------------------------------------------------------------------

void rlOpenXRUpdate()
{
	assert(s_xr && "rlOpenXR is not initialised yet, call rlOpenXRSetup()");

	XrResult result;

	XrEventDataBuffer runtime_event = { .type = XR_TYPE_EVENT_DATA_BUFFER, .next = NULL };
	XrResult poll_result = xrPollEvent(s_xr->instance, &runtime_event);
	while (poll_result == XR_SUCCESS) {
		switch (runtime_event.type) {
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
			XrEventDataInstanceLossPending* event = (XrEventDataInstanceLossPending*)&runtime_event;
			printf("EVENT: instance loss pending at %lu! Destroying instance.\n", event->lossTime);
			continue;
		}
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
			XrEventDataSessionStateChanged* event = (XrEventDataSessionStateChanged*)&runtime_event;
			printf("EVENT: session state changed from %d to %d\n", s_xr->session_state, event->state);
			s_xr->session_state = event->state;

			/*
			 * react to session state changes, see OpenXR spec 9.3 diagram. What we need to react to:
			 *
			 * * READY -> xrBeginSession STOPPING -> xrEndSession (note that the same session can be restarted)
			 * * EXITING -> xrDestroySession (EXITING only happens after we went through STOPPING and called xrEndSession)
			 *
			 * After exiting it is still possible to create a new session but we don't do that here.
			 *
			 * * IDLE -> don't run render loop, but keep polling for events
			 * * SYNCHRONIZED, VISIBLE, FOCUSED -> run render loop
			 */
			switch (s_xr->session_state) {
				// skip render loop, keep polling
			case XR_SESSION_STATE_MAX_ENUM: // must be a bug
			case XR_SESSION_STATE_IDLE:
			case XR_SESSION_STATE_UNKNOWN: {
				s_xr->run_framecycle = false;

				break; // state handling switch
			}

										 // do nothing, run render loop normally
			case XR_SESSION_STATE_FOCUSED:
			case XR_SESSION_STATE_SYNCHRONIZED:
			case XR_SESSION_STATE_VISIBLE: {
				s_xr->run_framecycle = true;

				break; // state handling switch
			}

										 // begin session and then run render loop
			case XR_SESSION_STATE_READY: {
				// start session only if it is not running, i.e. not when we already called xrBeginSession
				// but the runtime did not switch to the next state yet
				if (!s_xr->session_running) {
					XrSessionBeginInfo session_begin_info = { .type = XR_TYPE_SESSION_BEGIN_INFO,
															 .next = NULL,
															 .primaryViewConfigurationType = c_view_type };
					result = xrBeginSession(s_xr->session, &session_begin_info);
					if (!xr_check(result, "Failed to begin session!"))
						return;
					printf("Session started!\n");
					s_xr->session_running = true;
				}
				// after beginning the session, run render loop
				s_xr->run_framecycle = true;

				break; // state handling switch
			}

									   // end session, skip render loop, keep polling for next state change
			case XR_SESSION_STATE_STOPPING: {
				// end session only if it is running, i.e. not when we already called xrEndSession but the
				// runtime did not switch to the next state yet
				if (s_xr->session_running) {
					result = xrEndSession(s_xr->session);
					if (!xr_check(result, "Failed to end session!"))
						return;
					s_xr->session_running = false;
				}
				// after ending the session, don't run render loop
				s_xr->run_framecycle = false;

				break; // state handling switch
			}

										  // destroy session, skip render loop, exit render loop and quit
			case XR_SESSION_STATE_LOSS_PENDING:
			case XR_SESSION_STATE_EXITING:
				result = xrDestroySession(s_xr->session);
				if (!xr_check(result, "Failed to destroy session!"))
					return;
				s_xr->run_framecycle = false;

				break; // state handling switch
			}
			break; // session event handling switch
		}
		case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
			printf("EVENT: interaction profile changed!\n");
			XrEventDataInteractionProfileChanged* event =
				(XrEventDataInteractionProfileChanged*)&runtime_event;
			(void)event;

			XrInteractionProfileState state = { .type = XR_TYPE_INTERACTION_PROFILE_STATE };

			for (int i = 0; i < c_hand_count; i++) {
				XrResult res = xrGetCurrentInteractionProfile(s_xr->session, s_xr->hand_paths[i], &state);
				if (!xr_check(res, "Failed to get interaction profile for %d", i))
					continue;

				XrPath prof = state.interactionProfile;

				uint32_t strl;
				char profile_str[XR_MAX_PATH_LENGTH];
				res = xrPathToString(s_xr->instance, prof, XR_MAX_PATH_LENGTH, &strl, profile_str);
				if (!xr_check(res, "Failed to get interaction profile path str for %d", i))
					continue;

				printf("Event: Interaction profile changed for %d: %s\n", i, profile_str);
			}
			break;
		}
		default: printf("Unhandled event (type %d)\n", runtime_event.type);
		}

		runtime_event.type = XR_TYPE_EVENT_DATA_BUFFER;
		poll_result = xrPollEvent(s_xr->instance, &runtime_event);
	}
	if (poll_result == XR_EVENT_UNAVAILABLE) {
		// processed all events in the queue
	}
	else {
		// TODO: Actually print the poll_result as a string...
		printf("Failed to poll events!\n");
	}

	const XrActiveActionSet active_actionsets[] = {
			{.actionSet = s_xr->internal_actionset, .subactionPath = XR_NULL_PATH} };

	XrActionsSyncInfo actions_sync_info = {
			.type = XR_TYPE_ACTIONS_SYNC_INFO,
			.countActiveActionSets = sizeof(active_actionsets) / sizeof(active_actionsets[0]),
			.activeActionSets = active_actionsets,
	};
	result = xrSyncActions(s_xr->session, &actions_sync_info);
	xr_check(result, "failed to sync actions!");
}

void rlOpenXRUpdateCamera(Camera3D* camera)
{
	assert(s_xr && "rlOpenXR is not initialised yet, call rlOpenXRSetup()");
	assert(camera != nullptr);

	const XrTime time = std::max(s_xr->frame_state.predictedDisplayTime,
		wrapped_XrTimeFromQueryPerformanceCounter(s_xr->instance, s_xr->extensions.xrConvertWin32PerformanceCounterToTimeKHR));

	XrSpaceLocation view_location{ XR_TYPE_SPACE_LOCATION };
	XrResult result = xrLocateSpace(s_xr->view_space, s_xr->play_space, time, &view_location);
	if (!xr_check(result, "Could not locate view location"))
	{
		return;
	}

	if (view_location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)
	{
		const auto& pos = view_location.pose.position;
		camera->position = Vector3{ pos.x, pos.y, pos.z };
	}
	if (view_location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
	{
		const auto& rot = view_location.pose.orientation;
		const auto forward = Vector3RotateByQuaternion(Vector3{ 0, 0, -1 }, Quaternion{ rot.x, rot.y, rot.z, rot.w });
		const auto up = Vector3RotateByQuaternion(Vector3{ 0, 1, 0 }, Quaternion{ rot.x, rot.y, rot.z, rot.w });
		camera->target = Vector3Add(camera->position, forward);
		camera->up = up;
	}
}

void rlOpenXRUpdateHands(Transform* left, Transform* right)
{
	assert(s_xr && "rlOpenXR is not initialised yet, call rlOpenXRSetup()");

	const XrTime time = std::max(s_xr->frame_state.predictedDisplayTime,
		wrapped_XrTimeFromQueryPerformanceCounter(s_xr->instance, s_xr->extensions.xrConvertWin32PerformanceCounterToTimeKHR));

	std::array transforms{ left, right };

	for (int hand_index = 0; hand_index < c_hand_count; ++hand_index)
	{
		XrActionStateGetInfo get_info = { .type = XR_TYPE_ACTION_STATE_GET_INFO,
											.next = NULL,
											.action = s_xr->hand_pose_action,
											.subactionPath = s_xr->hand_paths[hand_index] };
		
		XrActionStatePose hand_pose_state{ XR_TYPE_ACTION_STATE_POSE };
		XrResult result = xrGetActionStatePose(s_xr->session, &get_info, &hand_pose_state);
		if (!xr_check(result, "failed to get hand %d action state pose!", hand_index)) 
		{ 
			continue; 
		}
		
		if (hand_pose_state.isActive && transforms[hand_index] != nullptr)
		{
			XrSpaceLocation hand_location{ XR_TYPE_SPACE_LOCATION };
			result = xrLocateSpace(s_xr->hand_spaces[hand_index], s_xr->play_space, time, &hand_location);
			if (!xr_check(result, "Could not retrieve hand %d location", hand_index)) 
			{ 
				continue; 
			}

			auto& pose = hand_location.pose;

			if (hand_location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)
			{
				transforms[hand_index]->translation = Vector3{ pose.position.x, pose.position.y, pose.position.z };
			}
			if (hand_location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
			{
				transforms[hand_index]->rotation = Quaternion{ pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w };
			}
		}
	}
}

// ----------------------------------------------------------------------------

bool rlOpenXRBegin()
{
	assert(s_xr && "rlOpenXR is not initialised yet, call rlOpenXRSetup()");

	if (!s_xr->session_running)
	{
		return false;
	}

	XrFrameWaitInfo frame_wait_info = { .type = XR_TYPE_FRAME_WAIT_INFO, .next = NULL };
	XrResult result = xrWaitFrame(s_xr->session, &frame_wait_info, &s_xr->frame_state);
	if (!xr_check(result, "xrWaitFrame() was not successful, skipping this frame"))
	{
		return false;
	}

	XrViewLocateInfo view_locate_info{ .type = XR_TYPE_VIEW_LOCATE_INFO,
										 .next = NULL,
										 .viewConfigurationType = c_view_type,
										 .displayTime = s_xr->frame_state.predictedDisplayTime,
										 .space = s_xr->play_space };

	XrViewState view_state{ XR_TYPE_VIEW_STATE };

	uint32_t output_view_count;
	result = xrLocateViews(s_xr->session, &view_locate_info, &view_state, c_view_count, &output_view_count, s_xr->views.data());
	if (!xr_check(result, "Could not locate views"))
		return false;

	assert(output_view_count == c_view_count);

	for (int i = 0; i < c_view_count; ++i)
	{
		s_xr->projection_views[i].pose = s_xr->views[i].pose;
		s_xr->projection_views[i].fov = s_xr->views[i].fov;
	}

	XrSpaceLocation view_location{ XR_TYPE_SPACE_LOCATION };
	result = xrLocateSpace(s_xr->view_space, s_xr->play_space, s_xr->frame_state.predictedDisplayTime, &view_location);
	if (!xr_check(result, "Could not locate view location"))
		return false;

	XrFrameBeginInfo frame_begin_info = { XR_TYPE_FRAME_BEGIN_INFO };
	result = xrBeginFrame(s_xr->session, &frame_begin_info);
	if (!xr_check(result, "failed to begin frame!"))
		return false;

	if (!s_xr->run_framecycle)
	{
		return false;
	}

	uint32_t swapchain_image_index = std::numeric_limits<uint32_t>::max();
	XrSwapchainImageAcquireInfo color_swapchain_image_acquire_info{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
	result = xrAcquireSwapchainImage(s_xr->swapchain, &color_swapchain_image_acquire_info, &swapchain_image_index);
	if (!xr_check(result, "failed to aquire swapchain image!"))
		return false;
	XrSwapchainImageWaitInfo wait_info{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	wait_info.timeout = XR_INFINITE_DURATION;
	result = xrWaitSwapchainImage(s_xr->swapchain, &wait_info);
	if (!xr_check(result, "failed to wait for swapchain image!"))
		return false;

	uint32_t color_swapchain_image = s_xr->swapchain_images[swapchain_image_index].image;
	uint32_t depth_swapchain_image = std::numeric_limits<uint32_t>::max();

	rlFramebufferAttach(s_xr->fbo, color_swapchain_image, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);

	if (s_xr->extensions.depth_enabled)
	{
		uint32_t swapchain_depth_image_index = std::numeric_limits<uint32_t>::max();
		XrSwapchainImageAcquireInfo depth_swapchain_image_acquire_info{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
		result = xrAcquireSwapchainImage(s_xr->depth_swapchain, &depth_swapchain_image_acquire_info, &swapchain_depth_image_index);
		if (!xr_check(result, "failed to aquire swapchain depth image!"))
			return false;
		XrSwapchainImageWaitInfo depth_wait_info{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		depth_wait_info.timeout = XR_INFINITE_DURATION;
		result = xrWaitSwapchainImage(s_xr->depth_swapchain, &depth_wait_info);
		if (!xr_check(result, "failed to wait for swapchain depth image!"))
			return false;

		depth_swapchain_image = s_xr->depth_swapchain_images[swapchain_depth_image_index].image;
		rlFramebufferAttach(s_xr->fbo, depth_swapchain_image, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0); //TODO: Stencil
	}

	assert(rlFramebufferComplete(s_xr->fbo));
	
	const int render_texture_width = s_xr->viewconfig_views[0].recommendedImageRectWidth * 2;
	const int render_texture_height = s_xr->viewconfig_views[0].recommendedImageRectHeight;

	RenderTexture2D render_texture{
		s_xr->fbo,
		Texture2D{
			color_swapchain_image,
			render_texture_width,
			render_texture_height,
			1,
			-1 // HACK: I do not know the GL format to RL format, but it doesn't seem to be used.
		},
		Texture2D{
			depth_swapchain_image,
			render_texture_width,
			render_texture_height,
			1,
			-1 // HACK: I do not know the GL format to RL format, but it doesn't seem to be used.
		}
	};

	BeginTextureMode(render_texture);
	s_xr->active_fbo = s_xr->fbo;

	rlEnableStereoRender();
	
	auto proj_left = xr_projection_matrix(s_xr->views[0].fov);
	auto proj_right = xr_projection_matrix(s_xr->views[1].fov);
	std::swap(proj_left, proj_right); // For some reason it doesn't look right unless they are swapped!?
	rlSetMatrixProjectionStereo(proj_right, proj_left); 

	const auto view_matrix = MatrixInvert(xr_matrix(view_location.pose));
	const auto view_offset_left = MatrixMultiply(xr_matrix(s_xr->views[0].pose), view_matrix);
	const auto view_offset_right = MatrixMultiply(xr_matrix(s_xr->views[1].pose), view_matrix);
	rlSetMatrixViewOffsetStereo(view_offset_right, view_offset_left);

	return true;
}

bool rlOpenXRBeginMockHMD()
{
	assert(s_xr && "rlOpenXR is not initialised yet, call rlOpenXRSetup()");

	VrDeviceInfo mock_device = {
		// Oculus Rift CV1 parameters for simulator
		.hResolution = 2160,                 // Horizontal resolution in pixels
		.vResolution = 1200,                 // Vertical resolution in pixels
		.hScreenSize = 0.133793f,            // Horizontal size in meters
		.vScreenSize = 0.0669f,              // Vertical size in meters
		.vScreenCenter = 0.04678f,           // Screen center in meters
		.eyeToScreenDistance = 0.041f,       // Distance between eye and display in meters
		.lensSeparationDistance = 0.07f,     // Lens separation distance in meters
		.interpupillaryDistance = 0.07f,     // IPD (distance between pupils) in meters

		// NOTE: CV1 uses fresnel-hybrid-asymmetric lenses with specific compute shaders
		// Following parameters are just an approximation to CV1 distortion stereo rendering
		.lensDistortionValues = { 1.0f, 0.22f, 0.24f, 0.0f},
		.chromaAbCorrection = { 0.996f,	-0.004f, 1.014f, 0.0f }
	};

	static const VrStereoConfig config = LoadVrStereoConfig(mock_device);

	if (s_xr->mock_hmd_rt.id == 0)
	{
		s_xr->mock_hmd_rt = LoadRenderTexture(mock_device.hResolution, mock_device.vResolution);
	}

	BeginTextureMode(s_xr->mock_hmd_rt);
	s_xr->active_fbo = s_xr->mock_hmd_rt.id;

	BeginVrStereoMode(config);

	return true;
}

void rlOpenXREnd()
{
	assert(s_xr && "rlOpenXR is not initialised yet, call rlOpenXRSetup()");

	if (!s_xr->session_running)
	{
		return;
	}

	if (s_xr->run_framecycle)
	{
		EndTextureMode();
		s_xr->active_fbo = 0;

		rlDisableStereoRender();

		XrSwapchainImageReleaseInfo release_info{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
		XrResult result = xrReleaseSwapchainImage(s_xr->swapchain, &release_info);
		xr_check(result, "failed to release color swapchain image!"); // We still want to continue ending the xr frame

		if (s_xr->extensions.depth_enabled)
		{
			XrSwapchainImageReleaseInfo depth_release_info{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
			result = xrReleaseSwapchainImage(s_xr->depth_swapchain, &depth_release_info);
			xr_check(result, "failed to release depth swapchain image!"); // We still want to continue ending the xr frame
		}
	}

	XrFrameEndInfo frame_end_info = { .type = XR_TYPE_FRAME_END_INFO,
									   .next = NULL,
									   .displayTime = s_xr->frame_state.predictedDisplayTime,
									   .environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
									   .layerCount = (uint32_t)s_xr->layers_pointers.size(),
									   .layers = s_xr->layers_pointers.data() };

	XrResult result = xrEndFrame(s_xr->session, &frame_end_info);
	if (!xr_check(result, "failed to end frame!"))
	{
		return;
	}
}

void rlOpenXRBlitToWindow(RLOpenXREye eye, bool keep_aspect_ratio)
{
	assert(s_xr && "rlOpenXR is not initialised yet, call rlOpenXRSetup()");
	assert(s_xr->active_fbo != 0 && "rlOpenXR is not currently drawing. call after rlOpenXRBegin() or rlOpenXRBeginMockHMD() and before rlOpenXREnd()");

	XrRect2Di src{};
	if (eye == RLOPENXR_EYE_LEFT)
	{
		src.offset = { 0, 0 };
		src.extent.width = (int)s_xr->viewconfig_views[0].recommendedImageRectWidth;
		src.extent.height = (int)s_xr->viewconfig_views[0].recommendedImageRectHeight;
	}
	else if (eye == RLOPENXR_EYE_RIGHT)
	{
		src.offset.x = (int)s_xr->viewconfig_views[0].recommendedImageRectWidth;
		src.offset.y = (int)s_xr->viewconfig_views[0].recommendedImageRectHeight;
		src.extent.width = (int)s_xr->viewconfig_views[1].recommendedImageRectWidth;
		src.extent.height = (int)s_xr->viewconfig_views[1].recommendedImageRectHeight;
	}
	else if (eye == RLOPENXR_EYE_BOTH)
	{
		src.offset = { 0, 0 };
		src.extent.width = (int)(s_xr->viewconfig_views[0].recommendedImageRectWidth + s_xr->viewconfig_views[1].recommendedImageRectWidth);
		src.extent.height = (int)s_xr->viewconfig_views[0].recommendedImageRectHeight;
	}
	else { assert(false && "Unknown value for `eye`"); }

	XrRect2Di dest{ {0, 0}, {rlGetFramebufferWidth(), rlGetFramebufferHeight()} };

	if (keep_aspect_ratio)
	{
		const float src_aspect = (float)src.extent.width / src.extent.height;
		const float dest_aspect = (float)dest.extent.width / dest.extent.height;

		if (src_aspect > dest_aspect)
		{
			dest.extent.height = dest.extent.width / src_aspect;
		}
		else
		{
			dest.extent.width = dest.extent.height * src_aspect;
		}
	}

	rlDisableFramebuffer(); // Return to default frame buffer

	ClearBackground(BLACK);

	glBlitNamedFramebuffer(s_xr->active_fbo, 0,
		src.offset.x, src.offset.y, src.offset.x + src.extent.width, src.offset.y + src.extent.height,
		dest.offset.x, dest.offset.y, dest.offset.x + dest.extent.width, dest.offset.y + dest.extent.height,
		GL_COLOR_BUFFER_BIT, GL_LINEAR);

	rlEnableFramebuffer(s_xr->active_fbo);
}

RLHand* rlOpenXRHand(RLHandEnum handedness)
{
	assert(handedness > 0 && handedness < RL_OPENXR_HAND_COUNT);
	return nullptr;
}

#ifdef __cplusplus
}
#endif
