# rlOpenXR
[OpenXR](https://www.khronos.org/openxr/) VR bindings for [Raylib](https://www.raylib.com/).

## Design decisions
To stay close to the design of Raylib. A concise yet powerful subset of OpenXR is exposed.
Most of the API is there to wrap the interaction with Raylib (Mostly rendering). And covering very common usage, eg Head/Hands position.

## Minimum Raylib version
Raylib 4.2

# Features
## Completed
 - [x] Rlgl rendering backend
 - [x] Builtin Head pose state
 - [x] Hand interface abstraction

## Planned
 - [ ] Controller state rendering
 - [ ] More convenient Input API
 - [ ] Virtual hand system and rendering

# Platforms
## Supported
 - [x] Windows

## Planned
 - [ ] Linux
 - [ ] Android (Targeting standalone HMDs like Quest 2)
 - [ ] WebXR
 
 ## No plans
 - [ ] Varjo 4 screen HMDs
 
# CMake options
RlOpenXR uses CMake (minimum 3.15) as it's build system. 

| Option | Description | Default |
| ---    | ---         | ---     |
| `RLOPENXR_BUILD_EXAMPLES` | Build RLOpenXR Examples | On |

## Using the rlOpenXR as a dependency
Out of the box rlOpenXR only supports CMake. There are a few options on how to add a library as a dependency in CMake:

### Subdirectory
Download the source for rlOpenXR into a subfolder in your project, for example: "third_party/rlOpenXR". 

This can be done via git: `git clone https://github.com/FireFlyForLife/rlOpenXR.git`. 

Or by manually going to [the github repo](https://github.com/FireFlyForLife/rlOpenXR) and going to "Code" -> "Download Zip".

Then in your CMakeLists.txt file for your project, add the following lines: 
```cmake
add_subdirectory(third_party/rlOpenXR)

# ...

target_link_libraries(YourProject PUBLIC rlOpenXR) # Add rlOpenXR as a dependency to your project
```

### FetchContent
In your CMakeLists.txt file, add the following lines: 
```cmake
FetchContent_Declare(
	rlOpenXR
	GIT_REPOSITORY https://github.com/FireFlyForLife/rlOpenXR.git
	GIT_TAG "1.0"
)

FetchContent_MakeAvailable(rlOpenXR)

# ...

target_link_libraries(YourProject PUBLIC rlOpenXR) # Add rlOpenXR as a dependency to your project
```
In the `FetchContent_MakeAvailable()` call, CMake will download the repository via Git, and add it parse it's CMake file.

