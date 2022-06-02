# rlOpenXR
[OpenXR](https://www.khronos.org/openxr/) VR bindings for [Raylib](https://www.raylib.com/).

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

# Design decisions
To stay close to the design of Raylib. A concise yet powerful subset of OpenXR is exposed.
Most of the API is there to wrap the interaction with Raylib (Mostly rendering). And covering very common usage, eg Head/Hands position.
