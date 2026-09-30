#include "TileEditor.h"

#include <algorithm>
#include <cmath>
#include <direct.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>

TileEditor::TileEditor()
{
    zoom = 1.0f;
    cellSize = 256; // keep as fallback/base value
    cameraOffset = { 140.0f, 60.0f };

    panelOpen = true;
    panelScrollX = 0.0f;

    selectedAssetId = "";
    selectedLayerIndex = 0;

    editingDisplayName = false;
    editingType = false;
    editBuffer = "";

    hasSelectedPlacedTile = false;
    selectedPlacedCell = { 0, 0 };

    EnsureAssetsFolderExists();

    layers.clear();

    Layer ground256;
    ground256.name = "Ground 256";
    ground256.visible = true;
    ground256.gridSize = 256;
    layers.push_back(ground256);

    Layer ground128;
    ground128.name = "Ground 128";
    ground128.visible = true;
    ground128.gridSize = 128;
    layers.push_back(ground128);

    Layer ground64;
    ground64.name = "Ground 64";
    ground64.visible = true;
    ground64.gridSize = 64;
    layers.push_back(ground64);

    Layer ground32;
    ground32.name = "Ground 32";
    ground32.visible = true;
    ground32.gridSize = 32;
    layers.push_back(ground32);

    Layer props;
    props.name = "Props";
    props.visible = true;
    props.gridSize = 256;
    layers.push_back(props);

    currentTool = EditorTool::Tiles;
    selectedColliderIndex = -1;
    draggingCollider = false;
    colliderDragOffset = { 0.0f, 0.0f };

    resizingCollider = false;
    activeResizeHandle = -1;
}

TileEditor::~TileEditor()
{
    for (TileAsset& asset : assets)
    {
        if (asset.texture.id != 0)
        {
            UnloadTexture(asset.texture);
        }
    }
}

void TileEditor::Update()
{
    HandleDroppedFiles();

    if (IsKeyPressed(KEY_TAB))
    {
        panelOpen = !panelOpen;
    }

    HandleShortcuts();
    HandleCamera();
    HandleZoom();
    HandleToolPanelInput();
    HandlePanelInput();
    HandleLayerPanelInput();
    HandleEditingInput();

    if (currentTool == EditorTool::Tiles)
    {
        HandlePlacedTileSelection();
        HandlePlacedTileMovement();

        if (!editingDisplayName && !editingType)
        {
            HandleTilePainting();
        }
    }
    else if (currentTool == EditorTool::Collision)
    {
        HandleCollisionModeInput();
    }
}

void TileEditor::Draw()
{
    DrawGrid();
    DrawColliders();
    DrawToolPanel();
    DrawLayerPanel();
    DrawBottomPanel();
    DrawInspector();
    DrawUI();
}

void TileEditor::HandleShortcuts()
{
    TileAsset* selected = FindAssetById(selectedAssetId);

    if (currentTool == EditorTool::Tiles && selected != nullptr && !editingDisplayName && !editingType)
    {
        if (IsKeyPressed(KEY_F2))
        {
            editingDisplayName = true;
            editBuffer = selected->displayName;
        }

        if (IsKeyPressed(KEY_F3))
        {
            editingType = true;
            editBuffer = selected->type;
        }

        Layer* layer = GetSelectedLayer();
        PlacedTile* placed = nullptr;

        if (layer != nullptr && hasSelectedPlacedTile)
        {
            auto it = layer->tiles.find(selectedPlacedCell);
            if (it != layer->tiles.end())
            {
                placed = &it->second;
            }
        }

        if (IsKeyPressed(KEY_W) && placed != nullptr)
        {
            placed->walkable = !placed->walkable;
        }

        if (IsKeyPressed(KEY_F) && placed != nullptr)
        {
            placed->fitToCell = !placed->fitToCell;
        }

        if (IsKeyPressed(KEY_LEFT_BRACKET) && placed != nullptr)
        {
            placed->drawWidth -= 8.0f;
            placed->drawHeight -= 8.0f;

            if (placed->drawWidth < 8.0f) placed->drawWidth = 8.0f;
            if (placed->drawHeight < 8.0f) placed->drawHeight = 8.0f;
        }

        if (IsKeyPressed(KEY_RIGHT_BRACKET) && placed != nullptr)
        {
            placed->drawWidth += 8.0f;
            placed->drawHeight += 8.0f;
        }

        if (IsKeyPressed(KEY_DELETE))
        {
            RemoveSelectedAsset();
        }
    }

    if (currentTool == EditorTool::Collision)
    {
        if (IsKeyPressed(KEY_DELETE))
        {
            if (selectedColliderIndex >= 0 && selectedColliderIndex < (int)colliders.size())
            {
                colliders.erase(colliders.begin() + selectedColliderIndex);
                selectedColliderIndex = -1;
                draggingCollider = false;
            }
        }
    }

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
    {
        AddLayer();
    }

    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
    {
        RemoveSelectedLayer();
    }

    if (IsKeyPressed(KEY_ONE) && (int)layers.size() > 0) selectedLayerIndex = 0;
    if (IsKeyPressed(KEY_TWO) && (int)layers.size() > 1) selectedLayerIndex = 1;
    if (IsKeyPressed(KEY_THREE) && (int)layers.size() > 2) selectedLayerIndex = 2;
    if (IsKeyPressed(KEY_FOUR) && (int)layers.size() > 3) selectedLayerIndex = 3;
    if (IsKeyPressed(KEY_FIVE) && (int)layers.size() > 4) selectedLayerIndex = 4;

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z))
    {
        Undo();
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y))
    {
        Redo();
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
    {
        SaveToFile("map_data.txt");
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_L))
    {
        LoadFromFile("map_data.txt");
    }
}

void TileEditor::HandleEditingInput()
{
    if (!editingDisplayName && !editingType)
    {
        return;
    }

    int key = GetCharPressed();
    while (key > 0)
    {
        if (key >= 32 && key <= 125)
        {
            editBuffer += (char)key;
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !editBuffer.empty())
    {
        editBuffer.pop_back();
    }

    if (IsKeyPressed(KEY_ENTER))
    {
        TileAsset* selected = FindAssetById(selectedAssetId);
        if (selected != nullptr)
        {
            if (editingDisplayName)
            {
                selected->displayName = editBuffer;
            }
            else if (editingType)
            {
                selected->type = editBuffer;
            }
        }

        editingDisplayName = false;
        editingType = false;
        editBuffer.clear();
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        editingDisplayName = false;
        editingType = false;
        editBuffer.clear();
    }
}

void TileEditor::HandleDroppedFiles()
{
    if (!IsFileDropped())
    {
        return;
    }

    FilePathList droppedFiles = LoadDroppedFiles();

    for (unsigned int i = 0; i < droppedFiles.count; i++)
    {
        const char* filePath = droppedFiles.paths[i];

        if (IsFileExtension(filePath, ".png"))
        {
            AddDroppedTexture(filePath);
        }
    }

    UnloadDroppedFiles(droppedFiles);
}

bool TileEditor::EnsureAssetsFolderExists()
{
    int result = _mkdir("Assets");
    return (result == 0 || errno == EEXIST);
}

bool TileEditor::CopyFileBinary(const std::string& sourcePath, const std::string& destinationPath)
{
    std::ifstream src(sourcePath, std::ios::binary);
    if (!src.is_open()) return false;

    std::ofstream dst(destinationPath, std::ios::binary);
    if (!dst.is_open()) return false;

    dst << src.rdbuf();
    return true;
}

void TileEditor::AddDroppedTexture(const char* filePath)
{
    EnsureAssetsFolderExists();

    std::string originalPath = filePath;
    std::string fileName = GetFileNameOnly(originalPath);
    std::string destinationPath = "Assets/" + fileName;

    if (!CopyFileBinary(originalPath, destinationPath))
    {
        std::cout << "Failed to copy file: " << originalPath << "\n";
        return;
    }

    Texture2D tex = LoadTexture(destinationPath.c_str());
    if (tex.id == 0)
    {
        std::cout << "Failed to load texture: " << destinationPath << "\n";
        return;
    }

    SetTextureFilter(tex, TEXTURE_FILTER_POINT);

    std::string displayName = GetFileNameWithoutExtension(fileName);
    std::string uniqueId = MakeUniqueAssetId(displayName);

    TileAsset asset;
    asset.id = uniqueId;
    asset.displayName = displayName;
    asset.type = MakeDefaultType(displayName);
    asset.sourcePath = destinationPath;
    asset.texture = tex;

    assets.push_back(asset);
    selectedAssetId = asset.id;
}

void TileEditor::HandleCamera()
{
    static Vector2 lastMousePos = { 0.0f, 0.0f };

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
    {
        lastMousePos = GetMousePosition();
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
        Vector2 mousePos = GetMousePosition();

        Vector2 delta = {
            mousePos.x - lastMousePos.x,
            mousePos.y - lastMousePos.y
        };

        cameraOffset.x += delta.x;
        cameraOffset.y += delta.y;

        lastMousePos = mousePos;
    }
}

void TileEditor::HandleZoom()
{
    if (IsMouseOverAnyUI())
    {
        return;
    }

    Vector2 mouse = GetMousePosition();
    float wheel = GetMouseWheelMove();

    if (wheel == 0.0f)
    {
        return;
    }

    float oldZoom = zoom;
    zoom += wheel * 0.1f;

    if (zoom < 0.2f) zoom = 0.2f;
    if (zoom > 3.0f) zoom = 3.0f;

    float scale = zoom / oldZoom;

    cameraOffset.x = mouse.x - (mouse.x - cameraOffset.x) * scale;
    cameraOffset.y = mouse.y - (mouse.y - cameraOffset.y) * scale;
}

void TileEditor::HandlePanelInput()
{
    if (!panelOpen)
    {
        return;
    }

    if (IsMouseOverPanel())
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
        {
            panelScrollX -= wheel * 40.0f;
            if (panelScrollX < 0.0f) panelScrollX = 0.0f;
        }
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    if (!IsMouseOverPanel())
    {
        return;
    }

    for (int i = 0; i < (int)assets.size(); i++)
    {
        Rectangle slot = GetAssetSlotRect(i);

        if (CheckCollisionPointRec(GetMousePosition(), slot))
        {
            selectedAssetId = assets[i].id;
            return;
        }
    }
}

void TileEditor::HandleLayerPanelInput()
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    if (!IsMouseOverLayerPanel())
    {
        return;
    }

    Rectangle panel = GetLayerPanelRect();

    Rectangle addButton = { panel.x + 10, panel.y + panel.height - 36, 40, 26 };
    Rectangle removeButton = { panel.x + 60, panel.y + panel.height - 36, 40, 26 };

    if (CheckCollisionPointRec(GetMousePosition(), addButton))
    {
        AddLayer();
        return;
    }

    if (CheckCollisionPointRec(GetMousePosition(), removeButton))
    {
        RemoveSelectedLayer();
        return;
    }

    for (int i = 0; i < (int)layers.size(); i++)
    {
        Rectangle rowRect = GetLayerRowRect(i);

        if (CheckCollisionPointRec(GetMousePosition(), rowRect))
        {
            Rectangle eyeRect = { rowRect.x + 6, rowRect.y + 6, 24, 20 };

            if (CheckCollisionPointRec(GetMousePosition(), eyeRect))
            {
                layers[i].visible = !layers[i].visible;
            }
            else
            {
                selectedLayerIndex = i;
            }

            return;
        }
    }
}

void TileEditor::HandleTilePainting()
{
    if (IsMouseOverAnyUI())
    {
        return;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
        return;
    }

    Layer* layer = GetSelectedLayer();
    if (layer == nullptr)
    {
        return;
    }

    int gridSize = layer->gridSize;

    float worldX = (GetMouseX() - cameraOffset.x) / (gridSize * zoom);
    float worldY = (GetMouseY() - cameraOffset.y) / (gridSize * zoom);

    int col = (int)std::floor(worldX);
    int row = (int)std::floor(worldY);

    std::pair<int, int> cell = { row, col };

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (!selectedAssetId.empty())
        {
            const TileAsset* asset = FindAssetById(selectedAssetId);
            if (asset == nullptr)
            {
                return;
            }

            TileEditAction action;
            action.layerIndex = selectedLayerIndex;
            action.cell = cell;

            auto it = layer->tiles.find(cell);
            action.hadOldTile = (it != layer->tiles.end());
            if (action.hadOldTile)
            {
                action.oldTile = it->second;
            }

            PlacedTile placed;
            placed.assetId = selectedAssetId;
            placed.offsetX = 0.0f;
            placed.offsetY = 0.0f;
            placed.rotation = 0.0f;
            placed.walkable = true;
            placed.fitToCell = false;
            placed.drawWidth = (float)asset->texture.width;
            placed.drawHeight = (float)asset->texture.height;

            action.hasNewTile = true;
            action.newTile = placed;

            bool changed = true;
            if (action.hadOldTile)
            {
                const PlacedTile& old = action.oldTile;
                changed = !(old.assetId == placed.assetId &&
                            old.offsetX == placed.offsetX &&
                            old.offsetY == placed.offsetY &&
                            old.rotation == placed.rotation &&
                            old.walkable == placed.walkable &&
                            old.fitToCell == placed.fitToCell &&
                            old.drawWidth == placed.drawWidth &&
                            old.drawHeight == placed.drawHeight);
            }

            if (changed)
            {
                layer->tiles[cell] = placed;
                PushTileEditAction(action);
            }
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        auto it = layer->tiles.find(cell);
        if (it != layer->tiles.end())
        {
            TileEditAction action;
            action.layerIndex = selectedLayerIndex;
            action.cell = cell;
            action.hadOldTile = true;
            action.oldTile = it->second;
            action.hasNewTile = false;

            layer->tiles.erase(cell);
            PushTileEditAction(action);

            if (hasSelectedPlacedTile &&
                selectedPlacedCell.first == row &&
                selectedPlacedCell.second == col)
            {
                hasSelectedPlacedTile = false;
            }
        }
    }
}

void TileEditor::HandlePlacedTileSelection()
{
    if (IsMouseOverAnyUI())
    {
        return;
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
    {
        return;
    }

    Layer* layer = GetSelectedLayer();
    if (layer == nullptr)
    {
        return;
    }

    int gridSize = layer->gridSize;

    float worldX = (GetMouseX() - cameraOffset.x) / (gridSize * zoom);
    float worldY = (GetMouseY() - cameraOffset.y) / (gridSize * zoom);

    int col = (int)std::floor(worldX);
    int row = (int)std::floor(worldY);

    auto it = layer->tiles.find({row, col});
    if (it != layer->tiles.end())
    {
        hasSelectedPlacedTile = true;
        selectedPlacedCell = { row, col };
        selectedAssetId = it->second.assetId;
    }
    else
    {
        hasSelectedPlacedTile = false;
    }
}

void TileEditor::HandlePlacedTileMovement()
{
    if (!hasSelectedPlacedTile)
    {
        return;
    }

    // Ground layer should never move placed tiles
    if (selectedLayerIndex == 0)
    {
        return;
    }

    Layer* layer = GetSelectedLayer();
    if (layer == nullptr)
    {
        return;
    }

    auto it = layer->tiles.find(selectedPlacedCell);
    if (it == layer->tiles.end())
    {
        hasSelectedPlacedTile = false;
        return;
    }

    float step = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 8.0f : 1.0f;

    if (IsKeyDown(KEY_LEFT))
    {
        it->second.offsetX -= step;
    }
    if (IsKeyDown(KEY_RIGHT))
    {
        it->second.offsetX += step;
    }
    if (IsKeyDown(KEY_UP))
    {
        it->second.offsetY -= step;
    }
    if (IsKeyDown(KEY_DOWN))
    {
        it->second.offsetY += step;
    }
}

void TileEditor::DrawGrid()
{
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    for (int layerIndex = 0; layerIndex < (int)layers.size(); layerIndex++)
    {
        const Layer& layer = layers[layerIndex];
        if (!layer.visible) continue;

        int gridSize = layer.gridSize;
        float tileSize = gridSize * zoom;

        int startCol = (int)std::floor(-cameraOffset.x / tileSize) - 2;
        int endCol = startCol + (int)(screenWidth / tileSize) + 5;

        int startRow = (int)std::floor(-cameraOffset.y / tileSize) - 2;
        int endRow = startRow + (int)(screenHeight / tileSize) + 5;

        for (int row = startRow; row < endRow; row++)
        {
            for (int col = startCol; col < endCol; col++)
            {
                auto it = layer.tiles.find({row, col});
                if (it == layer.tiles.end()) continue;

                const PlacedTile& placed = it->second;
                const TileAsset* asset = FindAssetById(placed.assetId);
                if (asset == nullptr) continue;

                float x = col * tileSize + cameraOffset.x;
                float y = row * tileSize + cameraOffset.y;

                Rectangle source = {
                    0.0f,
                    0.0f,
                    (float)asset->texture.width,
                    (float)asset->texture.height
                };

                Rectangle dest;

                if (placed.fitToCell)
                {
                    dest = {
                        x + placed.offsetX * zoom,
                        y + placed.offsetY * zoom,
                        tileSize,
                        tileSize
                    };
                }
                else
                {
                    float drawW = placed.drawWidth * zoom;
                    float drawH = placed.drawHeight * zoom;

                    float drawX = x + (tileSize - drawW) * 0.5f + placed.offsetX * zoom;
                    float drawY = y + (tileSize - drawH) + placed.offsetY * zoom;

                    dest = { drawX, drawY, drawW, drawH };
                }

                DrawTexturePro(asset->texture, source, dest, {0.0f, 0.0f}, 0.0f, WHITE);
            }
        }
    }

    Layer* selectedLayer = GetSelectedLayer();
    if (selectedLayer == nullptr)
    {
        return;
    }

    int gridSize = selectedLayer->gridSize;
    float tileSize = gridSize * zoom;

    int startCol = (int)std::floor(-cameraOffset.x / tileSize) - 2;
    int endCol = startCol + (int)(screenWidth / tileSize) + 5;

    int startRow = (int)std::floor(-cameraOffset.y / tileSize) - 2;
    int endRow = startRow + (int)(screenHeight / tileSize) + 5;

    for (int row = startRow; row < endRow; row++)
    {
        for (int col = startCol; col < endCol; col++)
        {
            float x = col * tileSize + cameraOffset.x;
            float y = row * tileSize + cameraOffset.y;

            DrawRectangleLinesEx({ x, y, tileSize, tileSize }, 1.0f, Fade(WHITE, 0.20f));

            if (hasSelectedPlacedTile &&
                selectedPlacedCell.first == row &&
                selectedPlacedCell.second == col)
            {
                DrawRectangleLinesEx({ x + 3.0f, y + 3.0f, tileSize - 6.0f, tileSize - 6.0f }, 3.0f, RED);
            }

            if (zoom >= 0.35f)
            {
                std::string cellText = std::to_string(row) + "," + std::to_string(col);

                int fontSize = (int)(16 * zoom);
                if (fontSize < 10) fontSize = 10;
                if (fontSize > 24) fontSize = 24;

                int textWidth = MeasureText(cellText.c_str(), fontSize);

                float textX = x + 6.0f;
                float textY = y + 6.0f;

                DrawRectangle(
                    (int)textX - 2,
                    (int)textY - 2,
                    textWidth + 4,
                    fontSize + 4,
                    Fade(BLACK, 0.60f)
                );

                DrawText(
                    cellText.c_str(),
                    (int)textX,
                    (int)textY,
                    fontSize,
                    YELLOW
                );
            }
        }
    }
}

void TileEditor::DrawBottomPanel()
{
    if (!panelOpen)
    {
        return;
    }

    Rectangle panel = GetPanelRect();
    DrawRectangleRec(panel, Color{18, 18, 18, 245});
    DrawRectangleLinesEx(panel, 2.0f, GRAY);

    DrawText("Assets Panel", (int)panel.x + 12, (int)panel.y + 8, 22, WHITE);
    DrawText("Drop PNG files into the window", (int)panel.x + 150, (int)panel.y + 10, 18, LIGHTGRAY);

    BeginScissorMode((int)panel.x + 8, (int)panel.y + 30, (int)panel.width - 16, (int)panel.height - 38);

    for (int i = 0; i < (int)assets.size(); i++)
    {
        const TileAsset& asset = assets[i];
        Rectangle slot = GetAssetSlotRect(i);

        if (slot.x + slot.width < panel.x || slot.x > panel.x + panel.width)
        {
            continue;
        }

        bool selected = (asset.id == selectedAssetId);

        DrawRectangleRec(slot, selected ? Color{60, 60, 20, 255} : Color{38, 38, 38, 255});
        DrawRectangleLinesEx(slot, 2.0f, selected ? YELLOW : DARKGRAY);

        Rectangle preview = {
            slot.x + 8.0f,
            slot.y + 8.0f,
            slot.width - 16.0f,
            64.0f
        };

        Rectangle source = {
            0.0f,
            0.0f,
            (float)asset.texture.width,
            (float)asset.texture.height
        };

        DrawTexturePro(asset.texture, source, preview, {0.0f, 0.0f}, 0.0f, WHITE);

        DrawText(asset.displayName.c_str(), (int)slot.x + 6, (int)slot.y + 78, 12, WHITE);
        DrawText(asset.type.c_str(), (int)slot.x + 6, (int)slot.y + 94, 12, SKYBLUE);
    }

    EndScissorMode();
}

void TileEditor::DrawLayerPanel()
{
    Rectangle panel = GetLayerPanelRect();
    DrawRectangleRec(panel, Color{18, 18, 18, 245});
    DrawRectangleLinesEx(panel, 2.0f, GRAY);

    DrawText("Layers", (int)panel.x + 10, (int)panel.y + 10, 22, WHITE);

    for (int i = 0; i < (int)layers.size(); i++)
    {
        Rectangle row = GetLayerRowRect(i);
        bool selected = (i == selectedLayerIndex);

        DrawRectangleRec(row, selected ? Color{55, 55, 20, 255} : Color{36, 36, 36, 255});
        DrawRectangleLinesEx(row, 1.0f, selected ? YELLOW : DARKGRAY);

        Rectangle eyeRect = { row.x + 6, row.y + 4, 24, 20 };
        DrawRectangleRec(eyeRect, layers[i].visible ? DARKGREEN : MAROON);
        DrawText(layers[i].visible ? "V" : "X", (int)eyeRect.x + 7, (int)eyeRect.y + 1, 18, WHITE);

        DrawText(layers[i].name.c_str(), (int)row.x + 38, (int)row.y + 3, 16, WHITE);
        DrawText(TextFormat("%dpx", layers[i].gridSize), (int)row.x + 38, (int)row.y + 15, 12, SKYBLUE);
        DrawText(TextFormat("%d", (int)layers[i].tiles.size()), (int)row.x + (int)row.width - 28, (int)row.y + 5, 16, LIGHTGRAY);
    }

    Rectangle addButton = { panel.x + 10, panel.y + panel.height - 36, 40, 26 };
    Rectangle removeButton = { panel.x + 60, panel.y + panel.height - 36, 40, 26 };

    DrawRectangleRec(addButton, DARKGREEN);
    DrawRectangleRec(removeButton, MAROON);

    DrawText("+", (int)addButton.x + 13, (int)addButton.y + 2, 20, WHITE);
    DrawText("-", (int)removeButton.x + 15, (int)removeButton.y + 2, 20, WHITE);
}

static std::string FitTextToWidth(const std::string& text, int maxWidth, int fontSize)
{
    if (MeasureText(text.c_str(), fontSize) <= maxWidth)
    {
        return text;
    }

    std::string result = text;
    while (!result.empty() && MeasureText((result + "...").c_str(), fontSize) > maxWidth)
    {
        result.pop_back();
    }

    return result + "...";
}

void TileEditor::DrawInspector()
{
    float rightReserved = 200.0f;
    float leftReserved = 170.0f;
    float boxW = 470.0f;
    float boxH = 210.0f;

    float boxX = (GetScreenWidth() - rightReserved - boxW) * 0.5f + 40.0f;
    if (boxX < leftReserved) boxX = leftReserved;

    float maxBoxX = GetScreenWidth() - rightReserved - boxW - 10.0f;
    if (boxX > maxBoxX) boxX = maxBoxX;

    float boxY = 10.0f;

    Rectangle box = { boxX, boxY, boxW, boxH };

    DrawRectangleRec(box, Color{20, 20, 20, 220});
    DrawRectangleLinesEx(box, 2.0f, GRAY);

    TileAsset* selected = FindAssetById(selectedAssetId);

    if (selected == nullptr)
    {
        DrawText("No asset selected", (int)box.x + 10, (int)box.y + 10, 18, WHITE);
        return;
    }

    int x = (int)box.x + 10;
    int y = (int)box.y + 10;
    int line = 22;
    int labelWidth = 78;
    int valueWidth = (int)box.width - labelWidth - 20;
    int fontSize = 18;

    DrawText("Selected Asset", x, y, 20, YELLOW);
    y += 28;

    std::string idText = FitTextToWidth(selected->id, valueWidth, fontSize);
    std::string nameText = FitTextToWidth(selected->displayName, valueWidth, fontSize);
    std::string typeText = FitTextToWidth(selected->type, valueWidth, fontSize);

    DrawText("Id:", x, y, fontSize, LIGHTGRAY);
    DrawText(idText.c_str(), x + labelWidth, y, fontSize, WHITE);
    y += line;

    DrawText("Name:", x, y, fontSize, LIGHTGRAY);
    DrawText(nameText.c_str(), x + labelWidth, y, fontSize, WHITE);
    y += line;

    DrawText("Type:", x, y, fontSize, LIGHTGRAY);
    DrawText(typeText.c_str(), x + labelWidth, y, fontSize, WHITE);
    y += line;

    Layer* layer = GetSelectedLayer();
    if (hasSelectedPlacedTile && layer != nullptr)
    {
        auto it = layer->tiles.find(selectedPlacedCell);
        if (it != layer->tiles.end())
        {
            const PlacedTile& placed = it->second;

            DrawText("Fit:", x, y, fontSize, LIGHTGRAY);
            DrawText(BoolToString(placed.fitToCell).c_str(), x + labelWidth, y, fontSize, WHITE);
            y += line;

            DrawText("Size:", x, y, fontSize, LIGHTGRAY);
            DrawText(TextFormat("%.0f x %.0f", placed.drawWidth, placed.drawHeight),
                     x + labelWidth, y, fontSize, WHITE);
            y += line;

            DrawText("Walkable:", x, y, fontSize, LIGHTGRAY);
            DrawText(placed.walkable ? "true" : "false",
                     x + labelWidth, y, fontSize,
                     placed.walkable ? GREEN : RED);
            y += line;

            DrawText("Offset:", x, y, fontSize, LIGHTGRAY);
            DrawText(TextFormat("%.0f, %.0f", placed.offsetX, placed.offsetY),
                     x + labelWidth, y, fontSize, SKYBLUE);
            y += line;
        }
    }

    if (editingDisplayName)
    {
        DrawText(TextFormat("Editing Name: %s", editBuffer.c_str()),
                 x, (int)box.y + (int)box.height - 42, 18, GREEN);
    }
    else if (editingType)
    {
        DrawText(TextFormat("Editing Type: %s", editBuffer.c_str()),
                 x, (int)box.y + (int)box.height - 42, 18, GREEN);
    }

    DrawText("F2 Name   F3 Type   W Tile Walkable   Enter Save   Esc Cancel",
             x, (int)box.y + (int)box.height - 18, 16, ORANGE);

    if (currentTool == EditorTool::Collision &&
        selectedColliderIndex >= 0 &&
        selectedColliderIndex < (int)colliders.size())
    {
        const Collider& c = colliders[selectedColliderIndex];
        const char* shapeName = (c.shape == ColliderShape::Rectangle) ? "Rect" : "Circle";

        DrawText(
            TextFormat("Collider: %s", shapeName),
            (int)box.x + (int)box.width - 160,
            (int)box.y + 34,
            16,
            RED
        );
    }
}

void TileEditor::DrawUI()
{
    int x = 10;
    int y = 10;
    int line = 22;

    DrawRectangle(6, 6, 280, 230, Fade(BLACK, 0.65f));
    DrawRectangleLines(6, 6, 280, 230, Fade(WHITE, 0.20f));

    DrawText("Middle Drag = Move Camera", x, y, 18, WHITE); y += line;
    DrawText("Wheel = Zoom", x, y, 18, WHITE); y += line;
    DrawText("1=256  2=128  3=64  4=32  5=Props", x, y, 18, ORANGE); y += line;

    if (selectedLayerIndex >= 0 && selectedLayerIndex < (int)layers.size())
    {
        DrawText(TextFormat("Active Layer: %s", layers[selectedLayerIndex].name.c_str()), x, y, 18, SKYBLUE); y += line;
        DrawText(TextFormat("Layer Grid: %d", layers[selectedLayerIndex].gridSize), x, y, 18, YELLOW); y += line;
    }

    if (currentTool == EditorTool::Tiles)
    {
        DrawText("Left Click = Paint", x, y, 18, WHITE); y += line;
        DrawText("Right Click = Erase", x, y, 18, WHITE); y += line;
        DrawText("Middle Click = Select Tile", x, y, 18, WHITE); y += line;
        DrawText("Arrows = Move Selected", x, y, 18, WHITE); y += line;
        DrawText("Shift + Arrows = Faster", x, y, 18, WHITE); y += line;
    }
    else if (currentTool == EditorTool::Collision)
    {
        DrawText("R = Rect Collider", x, y, 18, WHITE); y += line;
        DrawText("C = Circle Collider", x, y, 18, WHITE); y += line;
        DrawText("[ ] = Resize", x, y, 18, WHITE); y += line;
        DrawText("Delete = Remove", x, y, 18, WHITE); y += line;
        DrawText("Left Drag = Move", x, y, 18, WHITE); y += line;
    }

    DrawText("Ctrl+S = Save", x, y, 18, WHITE); y += line;
    DrawText("Ctrl+L = Load", x, y, 18, WHITE); y += line;
    DrawText("Ctrl+Z = Undo   Ctrl+Y = Redo", x, y, 18, WHITE); y += line;
}

Rectangle TileEditor::GetPanelRect() const
{
    return Rectangle{ 0.0f, (float)GetScreenHeight() - 150.0f, (float)GetScreenWidth(), 150.0f };
}

Rectangle TileEditor::GetAssetSlotRect(int index) const
{
    Rectangle panel = GetPanelRect();

    float slotWidth = 120.0f;
    float slotHeight = 120.0f;
    float spacing = 10.0f;
    float startX = panel.x + 12.0f - panelScrollX;
    float startY = panel.y + 26.0f;

    return Rectangle{
        startX + index * (slotWidth + spacing),
        startY,
        slotWidth,
        slotHeight
    };
}

Rectangle TileEditor::GetLayerPanelRect() const
{
    float width = 190.0f;
    float topOffset = 120.0f;
    float bottomOffset = panelOpen ? 150.0f : 0.0f;
    float height = (float)GetScreenHeight() - topOffset - bottomOffset;

    return Rectangle{
        (float)GetScreenWidth() - width,
        topOffset,
        width,
        height
    };
}

Rectangle TileEditor::GetLayerRowRect(int index) const
{
    Rectangle panel = GetLayerPanelRect();

    return Rectangle{
        panel.x + 8.0f,
        panel.y + 42.0f + index * 32.0f,
        panel.width - 16.0f,
        28.0f
    };
}

bool TileEditor::IsMouseOverPanel() const
{
    return panelOpen && CheckCollisionPointRec(GetMousePosition(), GetPanelRect());
}

bool TileEditor::IsMouseOverLayerPanel() const
{
    return CheckCollisionPointRec(GetMousePosition(), GetLayerPanelRect());
}

bool TileEditor::IsMouseOverAnyUI() const
{
    return IsMouseOverPanel()
        || IsMouseOverLayerPanel()
        || CheckCollisionPointRec(GetMousePosition(), GetToolPanelRect());
}

TileAsset* TileEditor::FindAssetById(const std::string& id)
{
    for (TileAsset& asset : assets)
    {
        if (asset.id == id)
        {
            return &asset;
        }
    }
    return nullptr;
}

const TileAsset* TileEditor::FindAssetById(const std::string& id) const
{
    for (const TileAsset& asset : assets)
    {
        if (asset.id == id)
        {
            return &asset;
        }
    }
    return nullptr;
}

void TileEditor::RemoveSelectedAsset()
{
    if (selectedAssetId.empty())
    {
        return;
    }

    std::string filePathToDelete = "";

    RemoveAssetFromAllLayers(selectedAssetId);

    for (int i = 0; i < (int)assets.size(); i++)
    {
        if (assets[i].id == selectedAssetId)
        {
            filePathToDelete = assets[i].sourcePath;

            if (assets[i].texture.id != 0)
            {
                UnloadTexture(assets[i].texture);
            }

            assets.erase(assets.begin() + i);
            break;
        }
    }

    // delete file from Assets folder
    if (!filePathToDelete.empty())
    {
        if (std::remove(filePathToDelete.c_str()) == 0)
        {
            std::cout << "Deleted file: " << filePathToDelete << "\n";
        }
        else
        {
            std::cout << "Failed to delete file: " << filePathToDelete << "\n";
        }
    }

    selectedAssetId = assets.empty() ? "" : assets.back().id;
    hasSelectedPlacedTile = false;
}

void TileEditor::RemoveAssetFromAllLayers(const std::string& assetId)
{
    for (Layer& layer : layers)
    {
        for (auto it = layer.tiles.begin(); it != layer.tiles.end(); )
        {
            if (it->second.assetId == assetId)
                it = layer.tiles.erase(it);
            else
                ++it;
        }
    }
}

void TileEditor::AddLayer()
{
    Layer layer;
    layer.name = "Layer " + std::to_string((int)layers.size());
    layer.visible = true;
    layer.gridSize = 256;
    layers.push_back(layer);
    selectedLayerIndex = (int)layers.size() - 1;
}

void TileEditor::RemoveSelectedLayer()
{
    if (layers.size() <= 1) return;
    if (selectedLayerIndex < 0 || selectedLayerIndex >= (int)layers.size()) return;

    layers.erase(layers.begin() + selectedLayerIndex);

    if (selectedLayerIndex >= (int)layers.size())
    {
        selectedLayerIndex = (int)layers.size() - 1;
    }

    hasSelectedPlacedTile = false;
}

Layer* TileEditor::GetSelectedLayer()
{
    if (selectedLayerIndex < 0 || selectedLayerIndex >= (int)layers.size()) return nullptr;
    return &layers[selectedLayerIndex];
}

const Layer* TileEditor::GetSelectedLayer() const
{
    if (selectedLayerIndex < 0 || selectedLayerIndex >= (int)layers.size()) return nullptr;
    return &layers[selectedLayerIndex];
}

std::string TileEditor::GetFileNameOnly(const std::string& fullPath) const
{
    size_t slashPos = fullPath.find_last_of("/\\");
    if (slashPos == std::string::npos) return fullPath;
    return fullPath.substr(slashPos + 1);
}

std::string TileEditor::GetFileNameWithoutExtension(const std::string& fullPath) const
{
    std::string fileName = GetFileNameOnly(fullPath);
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos == std::string::npos) return fileName;
    return fileName.substr(0, dotPos);
}

std::string TileEditor::MakeUniqueAssetId(const std::string& baseName) const
{
    std::string clean = baseName;
    std::replace(clean.begin(), clean.end(), ' ', '_');

    if (clean.empty()) clean = "asset";

    std::string candidate = clean;
    int counter = 1;

    while (FindAssetById(candidate) != nullptr)
    {
        candidate = clean + "_" + std::to_string(counter);
        counter++;
    }

    return candidate;
}

std::string TileEditor::MakeDefaultType(const std::string& baseName) const
{
    std::string result = baseName;
    std::replace(result.begin(), result.end(), '_', ' ');
    return result;
}

std::string TileEditor::BoolToString(bool value)
{
    return value ? "true" : "false";
}

bool TileEditor::StringToBool(const std::string& text)
{
    return text == "true" || text == "1" || text == "TRUE";
}

void TileEditor::SaveToFile(const std::string& filePath)
{
    std::ofstream file(filePath);
    if (!file.is_open())
    {
        std::cout << "Failed to save file\n";
        return;
    }

    file << "ASSET_COUNT " << assets.size() << "\n";
    for (const TileAsset& asset : assets)
    {
        file << asset.id << "|"
             << asset.displayName << "|"
             << asset.type << "|"
             << asset.sourcePath << "\n";
    }

    file << "LAYER_COUNT " << layers.size() << "\n";
    for (const Layer& layer : layers)
    {
        file << layer.name << "|"
             << BoolToString(layer.visible) << "|"
             << layer.gridSize << "|"
             << layer.tiles.size() << "\n";

        for (const auto& entry : layer.tiles)
        {
            file << entry.first.first << "|"
                 << entry.first.second << "|"
                 << entry.second.assetId << "|"
                 << entry.second.offsetX << "|"
                 << entry.second.offsetY << "|"
                 << entry.second.rotation << "|"
                 << BoolToString(entry.second.walkable) << "|"
                 << BoolToString(entry.second.fitToCell) << "|"
                 << entry.second.drawWidth << "|"
                 << entry.second.drawHeight << "\n";
        }
    }

    file << "COLLIDER_COUNT " << colliders.size() << "\n";
    for (const Collider& c : colliders)
    {
        file << (c.shape == ColliderShape::Rectangle ? "rect" : "circle") << "|"
             << c.center.x << "|"
             << c.center.y << "|"
             << c.size.x << "|"
             << c.size.y << "|"
             << c.rotation << "\n";
    }

    std::cout << "Saved to " << filePath << "\n";
}
void TileEditor::LoadFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cout << "Failed to load file\n";
        return;
    }

    for (TileAsset& asset : assets)
    {
        if (asset.texture.id != 0)
        {
            UnloadTexture(asset.texture);
        }
    }

    assets.clear();
    layers.clear();
    colliders.clear();
    selectedAssetId.clear();
    selectedLayerIndex = 0;
    hasSelectedPlacedTile = false;
    selectedColliderIndex = -1;
    draggingCollider = false;
    undoStack.clear();
    redoStack.clear();

    std::string label;
    size_t assetCount = 0;

    file >> label >> assetCount;
    file.ignore();

    if (label != "ASSET_COUNT")
    {
        std::cout << "Invalid file format\n";
        return;
    }

    for (size_t i = 0; i < assetCount; i++)
    {
        std::string line;
        std::getline(file, line);

        std::stringstream ss(line);

        std::string id;
        std::string displayName;
        std::string type;
        std::string sourcePath;

        std::getline(ss, id, '|');
        std::getline(ss, displayName, '|');
        std::getline(ss, type, '|');
        std::getline(ss, sourcePath);

        Texture2D tex = LoadTexture(sourcePath.c_str());
        if (tex.id == 0)
        {
            std::cout << "Failed to reload texture: " << sourcePath << "\n";
            continue;
        }

        SetTextureFilter(tex, TEXTURE_FILTER_POINT);

        TileAsset asset;
        asset.id = id;
        asset.displayName = displayName;
        asset.type = type;
        asset.sourcePath = sourcePath;
        asset.texture = tex;

        assets.push_back(asset);
    }

        size_t layerCount = 0;
    file >> label >> layerCount;
    file.ignore();

    if (label != "LAYER_COUNT")
    {
        std::cout << "Invalid layer section\n";
        return;
    }

    for (size_t i = 0; i < layerCount; i++)
    {
        std::string headerLine;
        std::getline(file, headerLine);

        std::stringstream header(headerLine);

        std::string layerName;
        std::string visibleText;
        std::string gridSizeText;
        std::string tileCountText;

        std::getline(header, layerName, '|');
        std::getline(header, visibleText, '|');
        std::getline(header, gridSizeText, '|');
        std::getline(header, tileCountText);

        Layer layer;
        layer.name = layerName;
        layer.visible = StringToBool(visibleText);
        layer.gridSize = gridSizeText.empty() ? 256 : std::stoi(gridSizeText);

        int tileCount = std::stoi(tileCountText);

        for (int t = 0; t < tileCount; t++)
        {
            std::string tileLine;
            std::getline(file, tileLine);

            std::stringstream tileStream(tileLine);

            std::string rowText;
            std::string colText;
            std::string assetId;
            std::string offsetXText;
            std::string offsetYText;
            std::string rotationText;
            std::string walkableTileText;
            std::string fitToCellText;
            std::string drawWidthText;
            std::string drawHeightText;

            std::getline(tileStream, rowText, '|');
            std::getline(tileStream, colText, '|');
            std::getline(tileStream, assetId, '|');
            std::getline(tileStream, offsetXText, '|');
            std::getline(tileStream, offsetYText, '|');
            std::getline(tileStream, rotationText, '|');
            std::getline(tileStream, walkableTileText, '|');
            std::getline(tileStream, fitToCellText, '|');
            std::getline(tileStream, drawWidthText, '|');
            std::getline(tileStream, drawHeightText);

            int row = std::stoi(rowText);
            int col = std::stoi(colText);

            const TileAsset* asset = FindAssetById(assetId);
            if (asset != nullptr)
            {
                PlacedTile placed;
                placed.assetId = assetId;
                placed.offsetX = offsetXText.empty() ? 0.0f : std::stof(offsetXText);
                placed.offsetY = offsetYText.empty() ? 0.0f : std::stof(offsetYText);
                placed.rotation = rotationText.empty() ? 0.0f : std::stof(rotationText);
                placed.walkable = walkableTileText.empty() ? true : StringToBool(walkableTileText);
                placed.fitToCell = fitToCellText.empty() ? false : StringToBool(fitToCellText);
                placed.drawWidth = drawWidthText.empty() ? (float)asset->texture.width : std::stof(drawWidthText);
                placed.drawHeight = drawHeightText.empty() ? (float)asset->texture.height : std::stof(drawHeightText);

                layer.tiles[{row, col}] = placed;
            }
        }

        layers.push_back(layer);
    }

    size_t colliderCount = 0;
    if (file >> label >> colliderCount)
    {
        file.ignore();

        if (label == "COLLIDER_COUNT")
        {
            for (size_t i = 0; i < colliderCount; i++)
            {
                std::string line;
                std::getline(file, line);

                std::stringstream ss(line);

                std::string shapeText;
                std::string xText, yText, sxText, syText, rotText;

                std::getline(ss, shapeText, '|');
                std::getline(ss, xText, '|');
                std::getline(ss, yText, '|');
                std::getline(ss, sxText, '|');
                std::getline(ss, syText, '|');
                std::getline(ss, rotText);

                Collider c;
                c.shape = (shapeText == "rect") ? ColliderShape::Rectangle : ColliderShape::Circle;
                c.center = { std::stof(xText), std::stof(yText) };
                c.size = { std::stof(sxText), std::stof(syText) };
                c.rotation = std::stof(rotText);

                colliders.push_back(c);
            }
        }
    }

    if (layers.empty())
    {
        AddLayer();
        layers[0].name = "Ground";
    }

    selectedLayerIndex = 0;

    if (!assets.empty())
    {
        selectedAssetId = assets[0].id;
    }

    std::cout << "Loaded from " << filePath << "\n";
}

void TileEditor::HandleToolPanelInput()
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    Rectangle panel = GetToolPanelRect();
    if (!CheckCollisionPointRec(GetMousePosition(), panel))
    {
        return;
    }

    Rectangle tilesBtn = { panel.x + 10, panel.y + 20, panel.width - 20, 32 };
    Rectangle collisionBtn = { panel.x + 10, panel.y + 60, panel.width - 20, 32 };

    if (CheckCollisionPointRec(GetMousePosition(), tilesBtn))
    {
        currentTool = EditorTool::Tiles;
        return;
    }

    if (CheckCollisionPointRec(GetMousePosition(), collisionBtn))
    {
        currentTool = EditorTool::Collision;
        return;
    }
}
void TileEditor::HandleCollisionModeInput()
{
    if (IsMouseOverAnyUI())
    {
        draggingCollider = false;
        resizingCollider = false;
        activeResizeHandle = -1;
        return;
    }

    Vector2 mouseScreen = GetMousePosition();
    Vector2 mouseWorld = {
        (mouseScreen.x - cameraOffset.x) / zoom,
        (mouseScreen.y - cameraOffset.y) / zoom
    };

    if (IsKeyPressed(KEY_R))
    {
        Collider c;
        c.shape = ColliderShape::Rectangle;
        c.center = mouseWorld;
        c.size = { 120.0f, 80.0f };
        c.rotation = 0.0f;
        colliders.push_back(c);
        selectedColliderIndex = (int)colliders.size() - 1;
    }

    if (IsKeyPressed(KEY_C))
    {
        Collider c;
        c.shape = ColliderShape::Circle;
        c.center = mouseWorld;
        c.size = { 48.0f, 48.0f };
        c.rotation = 0.0f;
        colliders.push_back(c);
        selectedColliderIndex = (int)colliders.size() - 1;
    }

    if (selectedColliderIndex >= 0 && selectedColliderIndex < (int)colliders.size())
    {
        Collider& c = colliders[selectedColliderIndex];

        if (IsKeyPressed(KEY_LEFT_BRACKET))
        {
            if (c.shape == ColliderShape::Rectangle)
            {
                c.size.x -= 10.0f;
                c.size.y -= 10.0f;
                if (c.size.x < 10.0f) c.size.x = 10.0f;
                if (c.size.y < 10.0f) c.size.y = 10.0f;
            }
            else
            {
                c.size.x -= 8.0f;
                if (c.size.x < 8.0f) c.size.x = 8.0f;
            }
        }

        if (IsKeyPressed(KEY_RIGHT_BRACKET))
        {
            if (c.shape == ColliderShape::Rectangle)
            {
                c.size.x += 10.0f;
                c.size.y += 10.0f;
            }
            else
            {
                c.size.x += 8.0f;
            }
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        draggingCollider = false;
        resizingCollider = false;
        activeResizeHandle = -1;

        if (selectedColliderIndex >= 0 && selectedColliderIndex < (int)colliders.size())
        {
            Collider& selected = colliders[selectedColliderIndex];

            if (selected.shape == ColliderShape::Rectangle)
            {
                int handleIndex = GetRectangleHandleAtPoint(selected, mouseWorld);
                if (handleIndex != -1)
                {
                    resizingCollider = true;
                    activeResizeHandle = handleIndex;
                    return;
                }
            }
        }

        selectedColliderIndex = -1;

        for (int i = (int)colliders.size() - 1; i >= 0; i--)
        {
            const Collider& c = colliders[i];
            bool hit = false;

            if (c.shape == ColliderShape::Rectangle)
            {
                Rectangle r = {
                    c.center.x - c.size.x * 0.5f,
                    c.center.y - c.size.y * 0.5f,
                    c.size.x,
                    c.size.y
                };
                hit = CheckCollisionPointRec(mouseWorld, r);
            }
            else
            {
                hit = CheckCollisionPointCircle(mouseWorld, c.center, c.size.x);
            }

            if (hit)
            {
                selectedColliderIndex = i;
                draggingCollider = true;
                colliderDragOffset = {
                    mouseWorld.x - c.center.x,
                    mouseWorld.y - c.center.y
                };
                break;
            }
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && resizingCollider)
    {
        if (selectedColliderIndex >= 0 && selectedColliderIndex < (int)colliders.size())
        {
            Collider& c = colliders[selectedColliderIndex];

            if (c.shape == ColliderShape::Rectangle)
            {
                float left = c.center.x - c.size.x * 0.5f;
                float right = c.center.x + c.size.x * 0.5f;
                float top = c.center.y - c.size.y * 0.5f;
                float bottom = c.center.y + c.size.y * 0.5f;

                switch (activeResizeHandle)
                {
                    case 0: left = mouseWorld.x; top = mouseWorld.y; break;
                    case 1: right = mouseWorld.x; top = mouseWorld.y; break;
                    case 2: left = mouseWorld.x; bottom = mouseWorld.y; break;
                    case 3: right = mouseWorld.x; bottom = mouseWorld.y; break;
                    default: break;
                }

                const float minSize = 10.0f;

                if (right - left < minSize)
                {
                    if (activeResizeHandle == 0 || activeResizeHandle == 2)
                        left = right - minSize;
                    else
                        right = left + minSize;
                }

                if (bottom - top < minSize)
                {
                    if (activeResizeHandle == 0 || activeResizeHandle == 1)
                        top = bottom - minSize;
                    else
                        bottom = top + minSize;
                }

                c.center.x = (left + right) * 0.5f;
                c.center.y = (top + bottom) * 0.5f;
                c.size.x = right - left;
                c.size.y = bottom - top;
            }
        }
    }
    else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggingCollider)
    {
        if (selectedColliderIndex >= 0 && selectedColliderIndex < (int)colliders.size())
        {
            colliders[selectedColliderIndex].center = {
                mouseWorld.x - colliderDragOffset.x,
                mouseWorld.y - colliderDragOffset.y
            };
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        draggingCollider = false;
        resizingCollider = false;
        activeResizeHandle = -1;
    }
}

void TileEditor::DrawColliders()
{
    for (int i = 0; i < (int)colliders.size(); i++)
    {
        const Collider& c = colliders[i];
        bool selected = (i == selectedColliderIndex);

        Color fill = selected ? Fade(RED, 0.25f) : Fade(BLUE, 0.20f);
        Color line = selected ? RED : SKYBLUE;

        float screenX = c.center.x * zoom + cameraOffset.x;
        float screenY = c.center.y * zoom + cameraOffset.y;

        if (c.shape == ColliderShape::Rectangle)
        {
            Rectangle r = {
                screenX - (c.size.x * zoom) * 0.5f,
                screenY - (c.size.y * zoom) * 0.5f,
                c.size.x * zoom,
                c.size.y * zoom
            };

            DrawRectangleRec(r, fill);
            DrawRectangleLinesEx(r, 2.0f, line);

            if (selected)
            {
                DrawRectangleHandles(c);
            }
        }
        else
        {
            float radius = c.size.x * zoom;
            DrawCircleV({ screenX, screenY }, radius, fill);
            DrawCircleLines((int)screenX, (int)screenY, radius, line);
        }
    }
}

void TileEditor::DrawToolPanel()
{
    Rectangle panel = GetToolPanelRect();

    DrawRectangleRec(panel, Color{18, 18, 18, 245});
    DrawRectangleLinesEx(panel, 2.0f, GRAY);

    DrawText("Tools", (int)panel.x + 10, (int)panel.y + 8, 22, WHITE);

    Rectangle tilesBtn = { panel.x + 10, panel.y + 36, panel.width - 20, 28 };
    Rectangle collisionBtn = { panel.x + 10, panel.y + 70, panel.width - 20, 28 };

    DrawRectangleRec(tilesBtn, currentTool == EditorTool::Tiles ? DARKGREEN : DARKGRAY);
    DrawRectangleRec(collisionBtn, currentTool == EditorTool::Collision ? DARKGREEN : DARKGRAY);

    DrawText("Tiles", (int)tilesBtn.x + 10, (int)tilesBtn.y + 5, 18, WHITE);
    DrawText("Collision", (int)collisionBtn.x + 10, (int)collisionBtn.y + 5, 18, WHITE);
}

Rectangle TileEditor::GetToolPanelRect() const
{
    float width = 190.0f;
    float height = 120.0f;

    return Rectangle{
        (float)GetScreenWidth() - width,
        0.0f,
        width,
        height
    };
}





void TileEditor::PushTileEditAction(const TileEditAction& action)
{
    undoStack.push_back(action);
    redoStack.clear();
}

void TileEditor::Undo()
{
    if (undoStack.empty())
    {
        return;
    }

    TileEditAction action = undoStack.back();
    undoStack.pop_back();

    if (action.layerIndex < 0 || action.layerIndex >= (int)layers.size())
    {
        return;
    }

    Layer& layer = layers[action.layerIndex];

    if (action.hadOldTile)
    {
        layer.tiles[action.cell] = action.oldTile;
    }
    else
    {
        layer.tiles.erase(action.cell);
    }

    redoStack.push_back(action);
}

void TileEditor::Redo()
{
    if (redoStack.empty())
    {
        return;
    }

    TileEditAction action = redoStack.back();
    redoStack.pop_back();

    if (action.layerIndex < 0 || action.layerIndex >= (int)layers.size())
    {
        return;
    }

    Layer& layer = layers[action.layerIndex];

    if (action.hasNewTile)
    {
        layer.tiles[action.cell] = action.newTile;
    }
    else
    {
        layer.tiles.erase(action.cell);
    }

    undoStack.push_back(action);
}


int TileEditor::GetRectangleHandleAtPoint(const Collider& c, Vector2 mouseWorld) const
{
    if (c.shape != ColliderShape::Rectangle)
    {
        return -1;
    }

    float left = c.center.x - c.size.x * 0.5f;
    float right = c.center.x + c.size.x * 0.5f;
    float top = c.center.y - c.size.y * 0.5f;
    float bottom = c.center.y + c.size.y * 0.5f;

    Vector2 handles[4] =
    {
        { left, top },       // 0 top-left
        { right, top },      // 1 top-right
        { left, bottom },    // 2 bottom-left
        { right, bottom }    // 3 bottom-right
    };

    const float handleRadius = 10.0f;

    for (int i = 0; i < 4; i++)
    {
        if (CheckCollisionPointCircle(mouseWorld, handles[i], handleRadius))
        {
            return i;
        }
    }

    return -1;
}

void TileEditor::DrawRectangleHandles(const Collider& c) const
{
    if (c.shape != ColliderShape::Rectangle)
    {
        return;
    }

    float left = c.center.x - c.size.x * 0.5f;
    float right = c.center.x + c.size.x * 0.5f;
    float top = c.center.y - c.size.y * 0.5f;
    float bottom = c.center.y + c.size.y * 0.5f;

    Vector2 handles[4] =
    {
        { left, top },
        { right, top },
        { left, bottom },
        { right, bottom }
    };

    for (int i = 0; i < 4; i++)
    {
        float screenX = handles[i].x * zoom + cameraOffset.x;
        float screenY = handles[i].y * zoom + cameraOffset.y;

        DrawCircleV({ screenX, screenY }, 6.0f, YELLOW);
        DrawCircleLines((int)screenX, (int)screenY, 6.0f, BLACK);
    }
}