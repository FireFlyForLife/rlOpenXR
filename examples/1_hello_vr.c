#include "rlOpenXR.h"

#include "raylib.h"
#include "raymath.h"

#include <stdio.h>

int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 1200;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "rlOpenXR - Hello Vr");
    
    // Define the camera to look into our 3d world
    Camera camera = { 0 };
    camera.position = (Vector3){10.0f, 10.0f, 10.0f}; // Camera position
    camera.target = (Vector3){0.0f, 3.0f, 0.0f};      // Camera looking at point
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};          // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                       // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;    // Camera mode type

    SetCameraMode(camera, CAMERA_FREE);

    SetTargetFPS(-1); // OpenXR is responsible for waiting in rlOpenXRUpdate()
                      // Having raylib also do it's VSync causes noticeable input lag

    const bool initialised_rlopenxr = rlOpenXRSetup();
    if (!initialised_rlopenxr)
    {
        printf("Failed to initialise rlOpenXR!");
        return 1;
    }
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------

        rlOpenXRUpdate(); // Update OpenXR State.
                          // Should be called at the start of each frame before other rlOpenXR calls.

        UpdateCamera(&camera); // Use mouse control as a debug option when no HMD is available
        rlOpenXRUpdateCamera(&camera); // If the HMD is available, set the camera position to the HMD position

        // Draw
        //----------------------------------------------------------------------------------

        ClearBackground(RAYWHITE); // Clear window, in case rlOpenXR skips rendering the frame

        // rlOpenXRBegin() returns false when OpenXR reports to skip the frame (The HMD is inactive).
        // Optionally rlOpenXRBeginMockHMD() can be chained to always render. It will render into a "Mock" backbuffer.
        if (rlOpenXRBegin() || rlOpenXRBeginMockHMD()) // Render to OpenXR backbuffer
        {
            ClearBackground(BLUE);

            BeginMode3D(camera);

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

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}