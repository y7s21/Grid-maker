#pragma once

#include <raylib.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct TileAsset
{
    std::string id;
    std::string displayName;
    std::string type;
    std::string sourcePath;
    Texture2D texture;
};
struct PlacedTile
{
    std::string assetId;

    float offsetX;
    float offsetY;
    float rotation;

    bool walkable;

    bool fitToCell;
    float drawWidth;
    float drawHeight;
};

struct Layer
{
    std::string name;
    bool visible;
    int gridSize;
    std::map<std::pair<int, int>, PlacedTile> tiles;
};

struct TileEditAction
{
    int layerIndex;
    std::pair<int, int> cell;

    bool hadOldTile;
    PlacedTile oldTile;

    bool hasNewTile;
    PlacedTile newTile;
};

enum class EditorTool
{
    Tiles,
    Collision
};

enum class ColliderShape
{
    Rectangle,
    Circle
};

struct Collider
{
    ColliderShape shape;
    Vector2 center;
    Vector2 size;     // rect: width/height, circle: radius in x
    float rotation;
};

class TileEditor
{
public:
    void DrawRectangleHandles(const Collider& c) const;
    int GetRectangleHandleAtPoint(const Collider& c, Vector2 mouseWorld) const;
    
    TileEditor();
    ~TileEditor();

    void Update();
    void Draw();

private:
    float zoom;
    int cellSize;
    Vector2 cameraOffset;

    bool panelOpen;
    float panelScrollX;

    std::vector<TileAsset> assets;
    std::vector<Layer> layers;

    std::string selectedAssetId;
    int selectedLayerIndex;

    bool editingDisplayName;
    bool editingType;
    std::string editBuffer;

    bool hasSelectedPlacedTile;
    std::pair<int, int> selectedPlacedCell;

    // tools
    EditorTool currentTool;

    // collision editor
    std::vector<Collider> colliders;
    int selectedColliderIndex;
    bool draggingCollider;
    Vector2 colliderDragOffset;

    void HandleDroppedFiles();
    void HandleCamera();
    void HandleZoom();
    void HandlePanelInput();
    void HandleLayerPanelInput();
    void HandleTilePainting();
    void HandlePlacedTileSelection();
    void HandlePlacedTileMovement();
    void HandleEditingInput();
    void HandleShortcuts();

    // new
    void HandleToolPanelInput();
    void HandleCollisionModeInput();

    void DrawGrid();
    void DrawBottomPanel();
    void DrawLayerPanel();
    void DrawInspector();
    void DrawUI();

    // new
    void DrawToolPanel();
    void DrawColliders();

    Rectangle GetPanelRect() const;
    Rectangle GetAssetSlotRect(int index) const;
    Rectangle GetLayerPanelRect() const;
    Rectangle GetLayerRowRect(int index) const;

    // new
    Rectangle GetToolPanelRect() const;

    bool IsMouseOverPanel() const;
    bool IsMouseOverLayerPanel() const;
    bool IsMouseOverAnyUI() const;

    TileAsset* FindAssetById(const std::string& id);
    const TileAsset* FindAssetById(const std::string& id) const;

    void AddDroppedTexture(const char* filePath);
    void RemoveSelectedAsset();
    void RemoveAssetFromAllLayers(const std::string& assetId);

    void AddLayer();
    void RemoveSelectedLayer();
    Layer* GetSelectedLayer();
    const Layer* GetSelectedLayer() const;

    std::string MakeUniqueAssetId(const std::string& baseName) const;
    std::string GetFileNameOnly(const std::string& fullPath) const;
    std::string GetFileNameWithoutExtension(const std::string& fullPath) const;
    std::string MakeDefaultType(const std::string& baseName) const;

    bool EnsureAssetsFolderExists();
    bool CopyFileBinary(const std::string& sourcePath, const std::string& destinationPath);

    void SaveToFile(const std::string& filePath);
    void LoadFromFile(const std::string& filePath);

    static std::string BoolToString(bool value);
    static bool StringToBool(const std::string& text);
    
    std::vector<TileEditAction> undoStack;
    std::vector<TileEditAction> redoStack;

    void PushTileEditAction(const TileEditAction& action);
    void Undo();
    void Redo();

    bool resizingCollider;
    int activeResizeHandle;
};