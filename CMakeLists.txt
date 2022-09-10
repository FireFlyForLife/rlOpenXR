cmake_minimum_required (VERSION 3.14)
include(FetchContent)

project (rlOpenXR)


# User Options
option(RLOPENXR_BUILD_EXAMPLES "Build RLOpenXR Examples" ON)


# Third party
FetchContent_Declare(
	raylib
	GIT_REPOSITORY https://github.com/raysan5/raylib.git
	GIT_TAG "4.2.0"
	# FIND_PACKAGE_ARGS # When CMake 3.24 is widely available, we can use this and remove our own "TryResolveWithFindPackage"
)

FetchContent_Declare(
	openxr
	GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
	GIT_TAG "release-1.0.25"
	# FIND_PACKAGE_ARGS # When CMake 3.24 is widely available, we can use this and remove our own "TryResolveWithFindPackage"
)


function(TryResolveWithFindPackage THIRD_PARTY_PACKAGE_LIST) # Temp function, should be moved to utility
	foreach(THIRD_PARTY_PACKAGE ${${THIRD_PARTY_PACKAGE_LIST}})
		find_package(${THIRD_PARTY_PACKAGE} QUIET)
		if(${${THIRD_PARTY_PACKAGE}_FOUND})
			message(TRACE "Found package ${THIRD_PARTY_PACKAGE} with find_package! It won't be fetched anymore with FetchContent.")
			list(REMOVE_ITEM ${THIRD_PARTY_PACKAGE_LIST} ${THIRD_PARTY_PACKAGE})
		endif()
	endforeach()

	# Propogate the list modifications to the callee
	set(${THIRD_PARTY_PACKAGE_LIST} ${${THIRD_PARTY_PACKAGE_LIST}} PARENT_SCOPE)
endfunction()

set(THIRD_PARTY_DEPENDENCIES raylib openxr)
TryResolveWithFindPackage(THIRD_PARTY_DEPENDENCIES)
message(STATUS "Fetching third party dependencies '${THIRD_PARTY_DEPENDENCIES}' with FetchContent_MakeAvailable. This might take a few minutes...")
FetchContent_MakeAvailable(${THIRD_PARTY_DEPENDENCIES})


# Executable
add_library (${PROJECT_NAME} "include/rlOpenXR.h" "src/rlOpenXR.cpp" "include/platform/rlOpenXRWin32Wrapper.h" "src/platform/rlOpenXRWin32Wrapper.cpp")
target_link_libraries(${PROJECT_NAME} PUBLIC raylib openxr_loader)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20) # TODO: Aim for 17 in the future
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)


# Examples
if(${RLOPENXR_BUILD_EXAMPLES})
	add_executable(rlOpenXR_hello_cube "examples/hello_cube.cpp")
	target_link_libraries(rlOpenXR_hello_cube PUBLIC ${PROJECT_NAME})

	add_executable(rlOpenXR_hello_hands "examples/hello_hands.cpp")
	target_link_libraries(rlOpenXR_hello_hands PUBLIC ${PROJECT_NAME})

	add_executable(rlOpenXR_hello_clicky_hands "examples/hello_clicky_hands.cpp")
	target_link_libraries(rlOpenXR_hello_clicky_hands PUBLIC ${PROJECT_NAME})
endif()
