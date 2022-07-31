# rlOpenXR
[OpenXR](https://www.khronos.org/openxr/) VR bindings for [Raylib](https://www.raylib.com/).

# Design decisions
To stay close to the design of Raylib. A concise yet powerful subset of OpenXR is exposed.
Most of the API is there to wrap the interaction with Raylib (Mostly rendering). And covering very common usage, eg Head/Hands position.

# Minimum Raylib version
We rely on a bug fix for Raylib stereo rendering: [74ca813] (https://github.com/raysan5/raylib/commit/74ca81338e45937c1f36efecdae4c7b4c293431c), Released after 4.0.
So I recommend latest from the `Master` branch.

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
RlOpenXR uses CMake (minimum 3.14) as it's build system. 

| Option | Description | Default |
| ---    | ---         | ---     |
| `RLOPENXR_BUILD_EXAMPLES` | Build RLOpenXR Examples | On |

The Raylib & OpenXR dependencies are currently always fetched using [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html). In the near future this will be replaced by the CMake's flexible [dependency system released in 3.24](https://cmake.org/cmake/help/latest/guide/using-dependencies/index.html#fetchcontent-and-find-package-integration).
