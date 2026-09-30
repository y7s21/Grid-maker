#include <raylib.h>
#include "TileEditor.h"

int main()
{
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Grid Maker");
    SetTargetFPS(60);

    TileEditor editor;

    while (!WindowShouldClose())
    {
        editor.Update();

        BeginDrawing();
        ClearBackground(BLACK);
        editor.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}