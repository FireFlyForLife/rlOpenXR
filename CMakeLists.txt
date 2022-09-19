cmake_minimum_required (VERSION 3.15)
include(FetchContent)

project (rlOpenXR)


# User Options
option(RLOPENXR_BUILD_EXAMPLES "Build RLOpenXR Examples" ON)


# Third party
FetchContent_Declare(
	raylib
	GIT_REPOSITORY https://github.com/raysan5/raylib.git
	GIT_TAG "4.2.0"
	# FIND_PACKAGE_ARGS # When CMake 3.24 is widely available, we can use this and remove our own "MakeAvailable_WithFindPackageCheck"
)

FetchContent_Declare(
	openxr
	GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
	GIT_TAG "release-1.0.25"
	# FIND_PACKAGE_ARGS # When CMake 3.24 is widely available, we can use this and remove our own "MakeAvailable_WithFindPackageCheck"
)

function(MakeAvailable_WithFindPackageCheck PACKAGE TARGET_NAME) # Temp function, until CMake 3.24 is more widely available
	message(TRACE "[rlOpenXR] Resolving package '${PACKAGE}' with required target: '${TARGET_NAME}'")

	if(TARGET ${TARGET_NAME})
		message(TRACE "[rlOpenXR] Succesfully found target: '${TARGET_NAME}'. Not trying to use find_package or FetchContent for '${PACKAGE}'.")
		return()
	endif()

	find_package(${PACKAGE} QUIET CONFIG)
	
	if(${${PACKAGE}_FOUND})
		message(TRACE "[rlOpenXR] Found package ${PACKAGE} with find_package! It won't be fetched with FetchContent.")
		return()
	endif()

	message(TRACE "[rlOpenXR] Package '${PACKAGE}' will be downloaded with FetchContent_MakeAvailable.")
	FetchContent_MakeAvailable(${PACKAGE})
endfunction()

message(STATUS "[rlOpenXR] Fetching third party dependencies with FetchContent_MakeAvailable. This might take a few minutes...")
MakeAvailable_WithFindPackageCheck(raylib raylib)
MakeAvailable_WithFindPackageCheck(openxr openxr_loader)


# Executable
add_library (${PROJECT_NAME} "include/rlOpenXR.h" "src/rlOpenXR.cpp" "include/platform/rlOpenXRWin32Wrapper.h" "src/platform/rlOpenXRWin32Wrapper.cpp" )
target_link_libraries(${PROJECT_NAME} PUBLIC raylib openxr_loader)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20) # TODO: Aim for 17 in the future
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)


# Examples
if(${RLOPENXR_BUILD_EXAMPLES})
	add_executable(rlOpenXR_hello_vr "examples/1_hello_vr.c")
	target_link_libraries(rlOpenXR_hello_vr PUBLIC ${PROJECT_NAME})

	add_executable(rlOpenXR_hello_hands "examples/2_hello_hands.c")
	target_link_libraries(rlOpenXR_hello_hands PUBLIC ${PROJECT_NAME})

	add_executable(rlOpenXR_hello_clicky_hands "examples/3_hello_clicky_hands.c")
	target_link_libraries(rlOpenXR_hello_clicky_hands PUBLIC ${PROJECT_NAME})

	add_executable(rlOpenXR_hello_teleport "examples/4_hello_teleport.c")
	target_link_libraries(rlOpenXR_hello_teleport PUBLIC ${PROJECT_NAME})
endif()
