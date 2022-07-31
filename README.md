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
 - [x] Windows

## Planned
 - [ ] Controller state rendering
 - [ ] Linux
 - [ ] Android (Targeting standalone HMDs like Quest 2)
 - [ ] WebXR

## No plans
 - [ ] Varjo 4 screen HMDs

# CMake options
RlOpenXR uses CMake as it's build system. 
