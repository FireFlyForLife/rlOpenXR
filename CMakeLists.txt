cmake_minimum_required (VERSION 3.14)
include(FetchContent)

project (rlOpenXR)


# User Options
option(RLOPENXR_BUILD_EXAMPLES "Build RLOpenXR Examples" ON)


# Third party
FetchContent_Declare(
	raylib
	GIT_REPOSITORY https://github.com/raysan5/raylib.git
	GIT_TAG df3f64bfd2c103603ae87097d5e8efad0ed54181 # Master at the time of writing
)

FetchContent_Declare(
	openxr
	GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
	GIT_TAG "release-1.0.22"
)

message(STATUS "Fetching third party dependencies with FetchContent_MakeAvailable. This might take a few minutes...")
FetchContent_MakeAvailable(raylib openxr)


# Executable
add_library (${PROJECT_NAME} "include/rlOpenXR.h" "src/rlOpenXR.cpp" "include/platform/rlOpenXRWin32Wrapper.h" "src/platform/rlOpenXRWin32Wrapper.cpp")
target_link_libraries(${PROJECT_NAME} PUBLIC raylib openxr_loader)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)


# Examples
if(${RLOPENXR_BUILD_EXAMPLES})
	add_executable(rlOpenXR_hello_cube "examples/hello_cube.cpp")
	target_link_libraries(rlOpenXR_hello_cube PUBLIC ${PROJECT_NAME})
endif()
