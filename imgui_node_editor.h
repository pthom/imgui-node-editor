//------------------------------------------------------------------------------
// VERSION 0.9.1
//
// LICENSE
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.
//
// CREDITS
//   Written by Michal Cichon
//------------------------------------------------------------------------------
# ifndef __IMGUI_NODE_EDITOR_H__
# define __IMGUI_NODE_EDITOR_H__
# pragma once


//------------------------------------------------------------------------------
# include <imgui.h>
# include <cstdint> // std::uintXX_t
# include <utility> // std::move


//------------------------------------------------------------------------------
# define IMGUI_NODE_EDITOR_VERSION      "0.10.0"
# define IMGUI_NODE_EDITOR_VERSION_NUM  001000


//------------------------------------------------------------------------------
#ifndef IMGUI_NODE_EDITOR_API
#define IMGUI_NODE_EDITOR_API
#endif


//------------------------------------------------------------------------------
namespace ax {
namespace NodeEditor {


//------------------------------------------------------------------------------
struct NodeId;
struct LinkId;
struct PinId;


//------------------------------------------------------------------------------
enum class PinKind
{
    Input,
    Output
};

enum class FlowDirection
{
    Forward,
    Backward
};

enum class CanvasSizeMode
{
    FitVerticalView,        // Previous view will be scaled to fit new view on Y axis
    FitHorizontalView,      // Previous view will be scaled to fit new view on X axis
    CenterOnly,             // Previous view will be centered on new view
};


//------------------------------------------------------------------------------
enum class SaveReasonFlags: uint32_t
{
    None       = 0x00000000,
    Navigation = 0x00000001,
    Position   = 0x00000002,
    Size       = 0x00000004,
    Selection  = 0x00000008,
    AddNode    = 0x00000010,
    RemoveNode = 0x00000020,
    User       = 0x00000040
};

inline SaveReasonFlags operator |(SaveReasonFlags lhs, SaveReasonFlags rhs) { return static_cast<SaveReasonFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)); }
inline SaveReasonFlags operator &(SaveReasonFlags lhs, SaveReasonFlags rhs) { return static_cast<SaveReasonFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)); }

using ConfigSaveSettings     = bool   (*)(const char* data, size_t size, SaveReasonFlags reason, void* userPointer);
using ConfigLoadSettings     = size_t (*)(char* data, void* userPointer);

using ConfigSaveNodeSettings = bool   (*)(NodeId nodeId, const char* data, size_t size, SaveReasonFlags reason, void* userPointer);
using ConfigLoadNodeSettings = size_t (*)(NodeId nodeId, char* data, void* userPointer);

using ConfigSession          = void   (*)(void* userPointer);

struct Config
{
    using CanvasSizeModeAlias = ax::NodeEditor::CanvasSizeMode;

    const char*             SettingsFile;
    ConfigSession           BeginSaveSession;
    ConfigSession           EndSaveSession;
    ConfigSaveSettings      SaveSettings;
    ConfigLoadSettings      LoadSettings;
    ConfigSaveNodeSettings  SaveNodeSettings;
    ConfigLoadNodeSettings  LoadNodeSettings;
    void*                   UserPointer;
    ImVector<float>         CustomZoomLevels;
    CanvasSizeModeAlias     CanvasSizeMode;
    int                     DragButtonIndex;        // Mouse button index drag action will react to (0-left, 1-right, 2-middle)
    int                     SelectButtonIndex;      // Mouse button index select action will react to (0-left, 1-right, 2-middle)
    int                     NavigateButtonIndex;    // Mouse button index navigate action will react to (0-left, 1-right, 2-middle)
    int                     ContextMenuButtonIndex; // Mouse button index context menu action will react to (0-left, 1-right, 2-middle)
    bool                    EnableSmoothZoom;
    float                   SmoothZoomPower;

    // Inside a node, Dear ImGui believes that the available width is the width of the window that hosts the editor:
    // Separator(), SeparatorText(), CollapsingHeader() and TextWrapped() go far beyond the node, and sliders / input fields
    // get a default width derived from the window.
    // Set ForceWindowContentWidthToNodeWidth to true so that they use the width of the node (false by default).
    // - All the text then wraps at the width of the node, so text does not give a width to the node: a node needs at least one
    //   item with a fixed width (Dummy, a widget preceded by SetNextItemWidth()...), otherwise it collapses.
    // - The default item width leaves room for a label of 4 wide characters. With a longer label, call SetNextItemWidth(),
    //   otherwise the node grows at each frame (this is detected, and reported with an IM_ASSERT).
    bool                    ForceWindowContentWidthToNodeWidth;

    Config()
        : SettingsFile("NodeEditor.json")
        , BeginSaveSession(nullptr)
        , EndSaveSession(nullptr)
        , SaveSettings(nullptr)
        , LoadSettings(nullptr)
        , SaveNodeSettings(nullptr)
        , LoadNodeSettings(nullptr)
        , UserPointer(nullptr)
        , CustomZoomLevels()
        , CanvasSizeMode(CanvasSizeModeAlias::FitVerticalView)
        , DragButtonIndex(0)
        , SelectButtonIndex(0)
        , NavigateButtonIndex(1)
        , ContextMenuButtonIndex(1)
        , EnableSmoothZoom(true)
# ifdef __APPLE__
        , SmoothZoomPower(1.1f)
# else
        , SmoothZoomPower(1.3f)
# endif
        , ForceWindowContentWidthToNodeWidth(false)
    {
    }
};


//------------------------------------------------------------------------------
enum StyleColor
{
    StyleColor_Bg,
    StyleColor_Grid,
    StyleColor_NodeBg,
    StyleColor_NodeBorder,
    StyleColor_HovNodeBorder,
    StyleColor_SelNodeBorder,
    StyleColor_NodeSelRect,
    StyleColor_NodeSelRectBorder,
    StyleColor_HovLinkBorder,
    StyleColor_SelLinkBorder,
    StyleColor_HighlightLinkBorder,
    StyleColor_LinkSelRect,
    StyleColor_LinkSelRectBorder,
    StyleColor_PinRect,
    StyleColor_PinRectBorder,
    StyleColor_Flow,
    StyleColor_FlowMarker,
    StyleColor_GroupBg,
    StyleColor_GroupBorder,

    StyleColor_Count
};

enum StyleVar
{
    StyleVar_NodePadding,
    StyleVar_NodeRounding,
    StyleVar_NodeBorderWidth,
    StyleVar_HoveredNodeBorderWidth,
    StyleVar_SelectedNodeBorderWidth,
    StyleVar_PinRounding,
    StyleVar_PinBorderWidth,
    StyleVar_LinkStrength,
    StyleVar_SourceDirection,
    StyleVar_TargetDirection,
    StyleVar_ScrollDuration,
    StyleVar_FlowMarkerDistance,
    StyleVar_FlowSpeed,
    StyleVar_FlowDuration,
    StyleVar_PivotAlignment,
    StyleVar_PivotSize,
    StyleVar_PivotScale,
    StyleVar_PinCorners,
    StyleVar_PinRadius,
    StyleVar_PinArrowSize,
    StyleVar_PinArrowWidth,
    StyleVar_GroupRounding,
    StyleVar_GroupBorderWidth,
    StyleVar_HighlightConnectedLinks,
    StyleVar_SnapLinkToPinDir,
    StyleVar_HoveredNodeBorderOffset,
    StyleVar_SelectedNodeBorderOffset,
    StyleVar_GridSize,

    StyleVar_Count
};

struct Style
{
    ImVec4  NodePadding;
    float   NodeRounding;
    float   NodeBorderWidth;
    float   HoveredNodeBorderWidth;
    float   HoverNodeBorderOffset;
    float   SelectedNodeBorderWidth;
    float   SelectedNodeBorderOffset;
    float   PinRounding;
    float   PinBorderWidth;
    float   LinkStrength;
    ImVec2  SourceDirection;
    ImVec2  TargetDirection;
    float   ScrollDuration;
    float   FlowMarkerDistance;
    float   FlowSpeed;
    float   FlowDuration;
    ImVec2  PivotAlignment;
    ImVec2  PivotSize;
    ImVec2  PivotScale;
    float   PinCorners;
    float   PinRadius;
    float   PinArrowSize;
    float   PinArrowWidth;
    float   GroupRounding;
    float   GroupBorderWidth;
    float   HighlightConnectedLinks;
    float   SnapLinkToPinDir; // when true link will start on the line defined by pin direction
    bool    AngledLinks;      // when true (default), a link that would pass through its source or target node is routed around them, with angles
    ImVec2  GridSize;         // size of a background grid cell, in canvas units (x and y independent)
    ImVec4  Colors[StyleColor_Count];

    Style()
    {
        NodePadding              = ImVec4(8, 8, 8, 8);
        NodeRounding             = 12.0f;
        NodeBorderWidth          = 1.5f;
        HoveredNodeBorderWidth   = 3.5f;
        HoverNodeBorderOffset    = 0.0f;
        SelectedNodeBorderWidth  = 3.5f;
        SelectedNodeBorderOffset = 0.0f;
        PinRounding              = 4.0f;
        PinBorderWidth           = 0.0f;
        LinkStrength             = 100.0f;
        SourceDirection          = ImVec2(1.0f, 0.0f);
        TargetDirection          = ImVec2(-1.0f, 0.0f);
        ScrollDuration           = 0.35f;
        FlowMarkerDistance       = 30.0f;
        FlowSpeed                = 150.0f;
        FlowDuration             = 2.0f;
        PivotAlignment           = ImVec2(0.5f, 0.5f);
        PivotSize                = ImVec2(0.0f, 0.0f);
        PivotScale               = ImVec2(1, 1);
        PinCorners               = ImDrawFlags_RoundCornersAll;
        PinRadius                = 0.0f;
        PinArrowSize             = 0.0f;
        PinArrowWidth            = 0.0f;
        GroupRounding            = 6.0f;
        GroupBorderWidth         = 1.0f;
        HighlightConnectedLinks  = 0.0f;
        SnapLinkToPinDir         = 0.0f;
        AngledLinks              = true;
        GridSize                 = ImVec2(32.0f, 32.0f);

        Colors[StyleColor_Bg]                 = ImColor( 60,  60,  70, 200);
        Colors[StyleColor_Grid]               = ImColor(120, 120, 120,  40);
        Colors[StyleColor_NodeBg]             = ImColor( 32,  32,  32, 200);
        Colors[StyleColor_NodeBorder]         = ImColor(255, 255, 255,  96);
        Colors[StyleColor_HovNodeBorder]      = ImColor( 50, 176, 255, 255);
        Colors[StyleColor_SelNodeBorder]      = ImColor(255, 176,  50, 255);
        Colors[StyleColor_NodeSelRect]        = ImColor(  5, 130, 255,  64);
        Colors[StyleColor_NodeSelRectBorder]  = ImColor(  5, 130, 255, 128);
        Colors[StyleColor_HovLinkBorder]      = ImColor( 50, 176, 255, 255);
        Colors[StyleColor_SelLinkBorder]      = ImColor(255, 176,  50, 255);
        Colors[StyleColor_HighlightLinkBorder]= ImColor(204, 105,   0, 255);
        Colors[StyleColor_LinkSelRect]        = ImColor(  5, 130, 255,  64);
        Colors[StyleColor_LinkSelRectBorder]  = ImColor(  5, 130, 255, 128);
        Colors[StyleColor_PinRect]            = ImColor( 60, 180, 255, 100);
        Colors[StyleColor_PinRectBorder]      = ImColor( 60, 180, 255, 128);
        Colors[StyleColor_Flow]               = ImColor(255, 128,  64, 255);
        Colors[StyleColor_FlowMarker]         = ImColor(255, 128,  64, 255);
        Colors[StyleColor_GroupBg]            = ImColor(  0,   0,   0, 160);
        Colors[StyleColor_GroupBorder]        = ImColor(255, 255, 255,  32);
    }
};


//------------------------------------------------------------------------------
struct EditorContext;


//------------------------------------------------------------------------------
// --- Editor context lifecycle --------------------------------------------
// You may keep multiple editors and switch between them with SetCurrentEditor.
// Pass a Config to CreateEditor to set e.g. SettingsFile (where node positions
// are persisted) or to override the default mouse buttons.
IMGUI_NODE_EDITOR_API void SetCurrentEditor(EditorContext* ctx);
IMGUI_NODE_EDITOR_API EditorContext* GetCurrentEditor();
IMGUI_NODE_EDITOR_API EditorContext* CreateEditor(const Config* config = nullptr);
IMGUI_NODE_EDITOR_API void DestroyEditor(EditorContext* ctx);
IMGUI_NODE_EDITOR_API const Config& GetConfig(EditorContext* ctx = nullptr);

// --- Style ----------------------------------------------------------------
// Editor-specific style, separate from ImGui::GetStyle().
// Push/PopStyleColor and Push/PopStyleVar work like the ImGui equivalents.
IMGUI_NODE_EDITOR_API Style& GetStyle();
IMGUI_NODE_EDITOR_API const char* GetStyleColorName(StyleColor colorIndex);

IMGUI_NODE_EDITOR_API void PushStyleColor(StyleColor colorIndex, const ImVec4& color);
IMGUI_NODE_EDITOR_API void PopStyleColor(int count = 1);

IMGUI_NODE_EDITOR_API void PushStyleVar(StyleVar varIndex, float value);
IMGUI_NODE_EDITOR_API void PushStyleVar(StyleVar varIndex, const ImVec2& value);
IMGUI_NODE_EDITOR_API void PushStyleVar(StyleVar varIndex, const ImVec4& value);
IMGUI_NODE_EDITOR_API void PopStyleVar(int count = 1);

// --- Frame ----------------------------------------------------------------
// All node-editor calls (BeginNode, Link, BeginCreate, ...) must be made
// between Begin() and End(), and Begin() must be called inside a real ImGui
// window. `id` distinguishes editor instances inside the same window;
// `size` matches ImGui::BeginChild semantics (0 = available).
IMGUI_NODE_EDITOR_API void Begin(const char* id, const ImVec2& size = ImVec2(0, 0));
IMGUI_NODE_EDITOR_API void End();

// --- Nodes & pins ---------------------------------------------------------
// Inside Begin/End: declare each node with BeginNode(id) ... EndNode(),
// and (optionally) declare its pins with BeginPin(id, kind) ... EndPin()
// in between. Anything you draw between the Begin/End is rendered inside
// the node; pins are typically wrapped around a Text/Button so the user
// has something to grab onto.
IMGUI_NODE_EDITOR_API void BeginNode(NodeId id);
IMGUI_NODE_EDITOR_API void BeginPin(PinId id, PinKind kind);

// --- Pin geometry overrides (advanced) -----------------------------------
// By default the pin's own item rectangle is used both as the hover area
// and as the place where links attach (the "pivot"). Use these calls to
// customize either independently, between BeginPin and EndPin.
//
//   PinRect(a, b)         : override the pin's hover/visual rectangle
//                           (otherwise inferred from drawn content).
//   PinPivotRect(a, b)    : override the rectangle used to compute where
//                           a link attaches.
//   PinPivotSize(size)    : set the pivot rect's size; -1 on a component
//                           means "use the pin's size on that axis".
//   PinPivotScale(scale)  : multiplicative scale applied to PivotSize.
//   PinPivotAlignment(al) : where in the pin's rect the pivot is anchored;
//                           (0,0)=top-left, (1,1)=bottom-right, (0.5,0.5)=center.
//
// You will rarely need these unless you draw custom-shaped pins (e.g. a
// triangle whose tip should be the link attach point).
IMGUI_NODE_EDITOR_API void PinRect(const ImVec2& a, const ImVec2& b);
IMGUI_NODE_EDITOR_API void PinPivotRect(const ImVec2& a, const ImVec2& b);
IMGUI_NODE_EDITOR_API void PinPivotSize(const ImVec2& size);
IMGUI_NODE_EDITOR_API void PinPivotScale(const ImVec2& scale);
IMGUI_NODE_EDITOR_API void PinPivotAlignment(const ImVec2& alignment);
IMGUI_NODE_EDITOR_API void EndPin();

// --- Group nodes ----------------------------------------------------------
//
// Calling Group(size) inside a BeginNode/EndNode block turns the node into a
// "Group node": a tinted, resizable rectangle (StyleColor_GroupBg / GroupBorder)
// that can act as a labeled container for other nodes.
//
// Behavior:
//   - The user can resize the group by dragging its bottom-right corner.
//   - Dragging the group drags every node whose bounds fall inside it
//     (handled internally by the editor's DragAction).
//   - `size` is only the INITIAL size, applied on the very first frame the
//     group exists. Subsequent user resizes are persisted by the editor and
//     override `size`. To resize a group programmatically afterwards, use
//     SetGroupSize(node_id, size).
//   - Anything drawn between BeginNode and Group(size) lands in the group's
//     "title strip" above the colored rectangle: title text, buttons, and
//     even pins via BeginPin/EndPin (useful for "summary" pins on a
//     collapsed group).
//
// Minimal example (C++):
//
//     ed::BeginNode(groupId);
//         ImGui::TextUnformatted("My Group");
//         ed::Group(ImVec2(300, 200));   // initial size only
//     ed::EndNode();
//
// Minimal example (Python):
//
//     ed.begin_node(group_id)
//     imgui.text_unformatted("My Group")
//     ed.group(imgui.ImVec2(300, 200))   # initial size only
//     ed.end_node()
//
// To find the nodes contained in a group, use the editor's geometry getters
// (no internal API needed): get the group's GetNodePosition / GetNodeSize,
// then for each candidate node test whether its center sits inside the
// group's rectangle.
//
IMGUI_NODE_EDITOR_API void Group(const ImVec2& size);
IMGUI_NODE_EDITOR_API void EndNode();

// --- Group hints ----------------------------------------------------------
//
// A "group hint" is overlay UI that the editor renders ONLY when zoomed out
// far enough that the in-node title becomes hard to read. BeginGroupHint
// returns false at normal zoom; it returns true and fades in below ~0.75x.
// The intended use is to draw a large title above the group so users can
// still identify it at low zoom.
//
// GetGroupMin / GetGroupMax return the targeted group's bounds in SCREEN
// coordinates so you can position your overlay relative to it.
// GetHintForegroundDrawList / GetHintBackgroundDrawList return draw lists
// that sit above (foreground) and below (background) the editor's normal
// content, so your overlay isn't clipped by the canvas.
//
// Minimal example (C++):
//
//     if (ed::BeginGroupHint(groupId))
//     {
//         ImVec2 min = ed::GetGroupMin();
//         auto*  fg  = ed::GetHintForegroundDrawList();
//         fg->AddText(ImVec2(min.x, min.y - 24.0f),
//                     ImGui::GetColorU32(ImGuiCol_Text),
//                     "My Group");
//     }
//     ed::EndGroupHint();
//
// Minimal example (Python):
//
//     if ed.begin_group_hint(group_id):
//         min_ = ed.get_group_min()
//         fg = ed.get_hint_foreground_draw_list()
//         fg.add_text(imgui.ImVec2(min_.x, min_.y - 24.0),
//                     imgui.get_color_u32(imgui.Col_.text.value),
//                     "My Group")
//     ed.end_group_hint()
//
IMGUI_NODE_EDITOR_API bool BeginGroupHint(NodeId nodeId);
IMGUI_NODE_EDITOR_API ImVec2 GetGroupMin();
IMGUI_NODE_EDITOR_API ImVec2 GetGroupMax();
IMGUI_NODE_EDITOR_API ImDrawList* GetHintForegroundDrawList();
IMGUI_NODE_EDITOR_API ImDrawList* GetHintBackgroundDrawList();
IMGUI_NODE_EDITOR_API void EndGroupHint();

// Returns the draw list used for the node's BACKGROUND layer (drawn under
// the node's content). Useful to add badges, highlights, etc. behind a node.
// TODO: Add a way to manage node background channels
IMGUI_NODE_EDITOR_API ImDrawList* GetNodeBackgroundDrawList(NodeId nodeId);

// Declares an existing link between two pins. Call once per frame for every
// link you want shown. Returns true if the link is currently visible/active.
// `color` default is the sentinel ImVec4(0,0,0,0) ("auto"): when alpha is 0
// the implementation substitutes the current ImGuiCol_Text, so links stay
// readable on both light and dark themes. Pass any non-zero-alpha color to
// override.
IMGUI_NODE_EDITOR_API bool Link(LinkId id, PinId startPinId, PinId endPinId, const ImVec4& color = ImVec4(0, 0, 0, 0), float thickness = 1.0f);

// Trigger a one-shot animated "flow" pulse along a link. Calling this once
// is enough; the editor handles the time-bounded animation internally.
IMGUI_NODE_EDITOR_API void Flow(LinkId linkId, FlowDirection direction = FlowDirection::Forward);

// --- Item creation (drag-out new link / new node) ------------------------
// Interaction protocol fired while the user drags a link from a pin:
//
//   if (BeginCreate()) {
//       PinId a, b;
//       if (QueryNewLink(&a, &b)) {     // user is hovering a candidate endpoint
//           if (/* link a->b is invalid */)
//               RejectNewItem();         // shows red feedback
//           else if (AcceptNewItem())    // returns true on mouse-release
//               /* commit the new link to your data model */;
//       }
//       if (QueryNewNode(&a)) {         // user dragged a link into empty space
//           if (AcceptNewItem())         // -> typical UX: open a "Add node" popup
//               /* spawn a new node and connect pin `a` to one of its pins */;
//       }
//   }
//   EndCreate();
//
// The QueryNewLink/QueryNewNode/AcceptNewItem overloads taking a color and
// thickness customize the in-progress link's drawing while the user drags.
IMGUI_NODE_EDITOR_API bool BeginCreate(const ImVec4& color = ImVec4(0, 0, 0, 0), float thickness = 1.0f);
IMGUI_NODE_EDITOR_API bool QueryNewLink(PinId* startId, PinId* endId);
IMGUI_NODE_EDITOR_API bool QueryNewLink(PinId* startId, PinId* endId, const ImVec4& color, float thickness = 1.0f);
IMGUI_NODE_EDITOR_API bool QueryNewNode(PinId* pinId);
IMGUI_NODE_EDITOR_API bool QueryNewNode(PinId* pinId, const ImVec4& color, float thickness = 1.0f);
IMGUI_NODE_EDITOR_API bool AcceptNewItem();
IMGUI_NODE_EDITOR_API bool AcceptNewItem(const ImVec4& color, float thickness = 1.0f);
IMGUI_NODE_EDITOR_API void RejectNewItem();
IMGUI_NODE_EDITOR_API void RejectNewItem(const ImVec4& color, float thickness = 1.0f);
IMGUI_NODE_EDITOR_API void EndCreate();

// --- Item deletion (Delete key, "Delete" context-menu, etc.) -------------
// Interaction protocol that yields the things the user wants to delete this
// frame. You decide whether to honor each one:
//
//   if (BeginDelete()) {
//       LinkId l;
//       while (QueryDeletedLink(&l))
//           if (AcceptDeletedItem()) /* remove link from your model */;
//       NodeId n;
//       while (QueryDeletedNode(&n))
//           if (AcceptDeletedItem()) /* remove node and its links */;
//   }
//   EndDelete();
//
// `deleteDependencies = true` (the default for AcceptDeletedItem) tells the
// editor to also enqueue links touching the accepted node, so a subsequent
// QueryDeletedLink call yields them too.
IMGUI_NODE_EDITOR_API bool BeginDelete();
IMGUI_NODE_EDITOR_API bool QueryDeletedLink(LinkId* linkId, PinId* startId = nullptr, PinId* endId = nullptr);
IMGUI_NODE_EDITOR_API bool QueryDeletedNode(NodeId* nodeId);
IMGUI_NODE_EDITOR_API bool AcceptDeletedItem(bool deleteDependencies = true);
IMGUI_NODE_EDITOR_API void RejectDeletedItem();
IMGUI_NODE_EDITOR_API void EndDelete();

// --- Node geometry --------------------------------------------------------
// Positions and sizes are in EDITOR (canvas) space, not screen space. Use
// CanvasToScreen / ScreenToCanvas to convert.
// GetNodeSize returns (0,0) on the very first frame a node is drawn (the
// editor has no measurement yet). It stabilizes immediately after.
// CenterNodeOnScreen moves the node so it lands at the center of the view.
IMGUI_NODE_EDITOR_API void SetNodePosition(NodeId nodeId, const ImVec2& editorPosition);
IMGUI_NODE_EDITOR_API void SetGroupSize(NodeId nodeId, const ImVec2& size);
IMGUI_NODE_EDITOR_API ImVec2 GetNodePosition(NodeId nodeId);
IMGUI_NODE_EDITOR_API ImVec2 GetNodeSize(NodeId nodeId);
IMGUI_NODE_EDITOR_API void CenterNodeOnScreen(NodeId nodeId);
IMGUI_NODE_EDITOR_API void SetNodeZPosition(NodeId nodeId, float z); // Sets node z position, nodes with higher value are drawn over nodes with lower value
IMGUI_NODE_EDITOR_API float GetNodeZPosition(NodeId nodeId); // Returns node z position, defaults is 0.0f

// Re-load the node's position/size from the editor's persisted settings
// (the SettingsFile, if any). Useful right after creating a node whose
// previous layout you want to bring back without the user having to drag it.
IMGUI_NODE_EDITOR_API void RestoreNodeState(NodeId nodeId);

// --- Suspend / Resume -----------------------------------------------------
// Temporarily disable the editor's input/canvas state machine. You MUST
// suspend before calling ImGui popup APIs like ImGui::OpenPopup or
// ImGui::BeginPopup that should appear ABOVE the canvas (otherwise the
// popup's coordinates and event capture will be wrong). Resume() restores
// editor input handling. See the ShowNodeContextMenu example below.
IMGUI_NODE_EDITOR_API void Suspend();
IMGUI_NODE_EDITOR_API void Resume();
IMGUI_NODE_EDITOR_API bool IsSuspended();

// --- InputTextMultiline inside a node ---------------------------------------
// ImGui::InputTextMultiline() uses a child window, and child windows do not work inside the editor.
// This version shows a read-only preview box with the requested size, and opens a resizable popup with the real
// editor when the box is clicked. Outside of the editor, it calls ImGui::InputTextMultiline().
// With a Dear ImGui that provides ImGuiContext::InputTextMultilineOverride, you do not need to call it:
// ImGui::InputTextMultiline() does the same thing when called inside a node.
IMGUI_NODE_EDITOR_API bool InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);

// True while the editor is processing user input this frame (drag, select,
// pan, zoom, link-create, etc.).
IMGUI_NODE_EDITOR_API bool IsActive();

// --- Selection ------------------------------------------------------------
// HasSelectionChanged returns true for one frame after the selection set
// changed (use it to react to selection changes once, not every frame).
// SelectNode/SelectLink with append=false replaces the current selection.
IMGUI_NODE_EDITOR_API bool HasSelectionChanged();
IMGUI_NODE_EDITOR_API int  GetSelectedObjectCount();
IMGUI_NODE_EDITOR_API int  GetSelectedNodes(NodeId* nodes, int size);
IMGUI_NODE_EDITOR_API int  GetSelectedLinks(LinkId* links, int size);
IMGUI_NODE_EDITOR_API bool IsNodeSelected(NodeId nodeId);
IMGUI_NODE_EDITOR_API bool IsLinkSelected(LinkId linkId);
IMGUI_NODE_EDITOR_API void ClearSelection();
IMGUI_NODE_EDITOR_API void SelectNode(NodeId nodeId, bool append = false);
IMGUI_NODE_EDITOR_API void SelectLink(LinkId linkId, bool append = false);
IMGUI_NODE_EDITOR_API void DeselectNode(NodeId nodeId);
IMGUI_NODE_EDITOR_API void DeselectLink(LinkId linkId);

// Programmatically queue a node/link for deletion. The next BeginDelete()
// loop will yield it via QueryDeletedNode/QueryDeletedLink.
IMGUI_NODE_EDITOR_API bool DeleteNode(NodeId nodeId);
IMGUI_NODE_EDITOR_API bool DeleteLink(LinkId linkId);

IMGUI_NODE_EDITOR_API bool HasAnyLinks(NodeId nodeId); // Returns true if node has any link connected
IMGUI_NODE_EDITOR_API bool HasAnyLinks(PinId pinId); // Return true if pin has any link connected
IMGUI_NODE_EDITOR_API int BreakLinks(NodeId nodeId); // Break all links connected to this node
IMGUI_NODE_EDITOR_API int BreakLinks(PinId pinId); // Break all links connected to this pin

// --- Navigation -----------------------------------------------------------
// Programmatic equivalents of pressing F (with no modifier and with Shift).
// `duration` is the animation length in seconds; -1 means "use the editor
// default". NavigateToSelection requires a non-empty selection.
IMGUI_NODE_EDITOR_API void NavigateToContent(float duration = -1);
IMGUI_NODE_EDITOR_API void NavigateToSelection(bool zoomIn = false, float duration = -1);

// Shows context menu for node, link or background
// Typical usage (this should happen inside ed::Begin/ed::End block):
//        ed::Begin();
//        ... (Show nodes)
//        ed::Suspend();
//        if (ed::ShowNodeContextMenu(&contextNodeId))
//            ImGui::OpenPopup("Node Context Menu");
//        ed::Resume();
//        ...
//        ed::Suspend();
//        if (ImGui::BeginPopup("Node Context Menu"))
//        {
//            ImGui::Text("Node Context Menu, node ID: %d", contextNodeId);
//            ImGui::EndPopup();
//        }
//        ed::Resume();
//        ...
//        ed::End();
IMGUI_NODE_EDITOR_API bool ShowNodeContextMenu(NodeId* nodeId);
IMGUI_NODE_EDITOR_API bool ShowPinContextMenu(PinId* pinId);
IMGUI_NODE_EDITOR_API bool ShowLinkContextMenu(LinkId* linkId);
IMGUI_NODE_EDITOR_API bool ShowBackgroundContextMenu();

// --- Keyboard shortcuts ---------------------------------------------------
// Master switch: when disabled the editor never reacts to F / Ctrl+X /
// Ctrl+C / Ctrl+V / Ctrl+D / Space etc. Useful when an ImGui text input
// has focus and you want shortcuts ignored.
IMGUI_NODE_EDITOR_API void EnableShortcuts(bool enable);
IMGUI_NODE_EDITOR_API bool AreShortcutsEnabled();

// --- Shortcut handling protocol ------------------------------------------
// Lets you ASK the editor which keyboard shortcut fired this frame and
// respond to it. Pattern (inside Begin/End):
//
//   if (BeginShortcut()) {
//       if (AcceptCopy())       /* user pressed Ctrl+C: copy selection */;
//       if (AcceptPaste())      /* user pressed Ctrl+V: paste at mouse */;
//       if (AcceptCut())        /* user pressed Ctrl+X */;
//       if (AcceptDuplicate())  /* user pressed Ctrl+D */;
//       if (AcceptCreateNode()) /* user dragged a link out into empty space */;
//   }
//   EndShortcut();
//
// GetActionContextNodes / GetActionContextLinks return the objects the
// shortcut applies to (typically the current selection at the moment the
// shortcut fired). They are valid only between Begin/EndShortcut.
IMGUI_NODE_EDITOR_API bool BeginShortcut();
IMGUI_NODE_EDITOR_API bool AcceptCut();
IMGUI_NODE_EDITOR_API bool AcceptCopy();
IMGUI_NODE_EDITOR_API bool AcceptPaste();
IMGUI_NODE_EDITOR_API bool AcceptDuplicate();
IMGUI_NODE_EDITOR_API bool AcceptCreateNode();
IMGUI_NODE_EDITOR_API int  GetActionContextSize();
IMGUI_NODE_EDITOR_API int  GetActionContextNodes(NodeId* nodes, int size);
IMGUI_NODE_EDITOR_API int  GetActionContextLinks(LinkId* links, int size);
IMGUI_NODE_EDITOR_API void EndShortcut();

// Returns the INVERSE of the zoom: the size of a pixel in canvas units.
// 1.0 at 100%, 2.0 when the content is drawn at half size (zoomed out), 0.5 when it is drawn twice as big (zoomed in).
// To convert positions, use ScreenToCanvas() / CanvasToScreen().
IMGUI_NODE_EDITOR_API float GetCurrentZoom();

// --- Input queries (call between Begin and End) ---------------------------
// These return the object under the mouse this frame (NodeId/PinId/LinkId,
// 0 if none) and which buttons were clicked or double-clicked on the empty
// background. The "BackgroundClick" pair returns -1 when no click happened.
IMGUI_NODE_EDITOR_API NodeId GetHoveredNode();
IMGUI_NODE_EDITOR_API PinId GetHoveredPin();
IMGUI_NODE_EDITOR_API LinkId GetHoveredLink();
IMGUI_NODE_EDITOR_API NodeId GetDoubleClickedNode();
IMGUI_NODE_EDITOR_API PinId GetDoubleClickedPin();
IMGUI_NODE_EDITOR_API LinkId GetDoubleClickedLink();
IMGUI_NODE_EDITOR_API bool IsBackgroundClicked();
IMGUI_NODE_EDITOR_API bool IsBackgroundDoubleClicked();
IMGUI_NODE_EDITOR_API ImGuiMouseButton GetBackgroundClickButtonIndex(); // -1 if none
IMGUI_NODE_EDITOR_API ImGuiMouseButton GetBackgroundDoubleClickButtonIndex(); // -1 if none

IMGUI_NODE_EDITOR_API bool GetLinkPins(LinkId linkId, PinId* startPinId, PinId* endPinId); // pass nullptr if particular pin do not interest you

// True if the pin was ever connected to a link in its lifetime, even if it
// is currently disconnected.
IMGUI_NODE_EDITOR_API bool PinHadAnyLinks(PinId pinId);

// --- Coordinate conversion ------------------------------------------------
// SCREEN coords are pixels in the OS window. CANVAS coords are the editor's
// virtual space (what GetNodePosition / SetNodePosition use). The two
// differ by the current pan + zoom transform.
IMGUI_NODE_EDITOR_API ImVec2 GetScreenSize();
IMGUI_NODE_EDITOR_API ImVec2 ScreenToCanvas(const ImVec2& pos);
IMGUI_NODE_EDITOR_API ImVec2 CanvasToScreen(const ImVec2& pos);

IMGUI_NODE_EDITOR_API int GetNodeCount();                                // Returns number of submitted nodes since Begin() call
IMGUI_NODE_EDITOR_API int GetOrderedNodeIds(NodeId* nodes, int size);    // Fills an array with node id's in order they're drawn; up to 'size` elements are set. Returns actual size of filled id's.







//------------------------------------------------------------------------------
namespace Details {

template <typename T, typename Tag>
struct SafeType
{
    SafeType(T t)
        : m_Value(std::move(t))
    {
    }

    SafeType(const SafeType&) = default;

    template <typename T2, typename Tag2>
    SafeType(
        const SafeType
        <
            typename std::enable_if<!std::is_same<T, T2>::value, T2>::type,
            typename std::enable_if<!std::is_same<Tag, Tag2>::value, Tag2>::type
        >&) = delete;

    SafeType& operator=(const SafeType&) = default;

    explicit operator T() const { return Get(); }

    T Get() const { return m_Value; }

private:
    T m_Value;
};


template <typename Tag>
struct SafePointerType
    : SafeType<uintptr_t, Tag>
{
    static const Tag Invalid;

    using SafeType<uintptr_t, Tag>::SafeType;

    SafePointerType()
        : SafePointerType(Invalid)
    {
    }

    template <typename T = void> explicit SafePointerType(T* ptr): SafePointerType(reinterpret_cast<uintptr_t>(ptr)) {}
    template <typename T = void> T* AsPointer() const { return reinterpret_cast<T*>(this->Get()); }

    explicit operator bool() const { return *this != Invalid; }

    // Needed by the std::map used by IsNodeGrowingIndefinitely()
    bool operator<(const SafePointerType& other) const
    {
        return this->Get() < other.Get();
    }
};

template <typename Tag>
const Tag SafePointerType<Tag>::Invalid = { 0 };

template <typename Tag>
inline bool operator==(const SafePointerType<Tag>& lhs, const SafePointerType<Tag>& rhs)
{
    return lhs.Get() == rhs.Get();
}

template <typename Tag>
inline bool operator!=(const SafePointerType<Tag>& lhs, const SafePointerType<Tag>& rhs)
{
    return lhs.Get() != rhs.Get();
}

} // namespace Details

struct NodeId final: Details::SafePointerType<NodeId>
{
    using SafePointerType::SafePointerType;
};

struct LinkId final: Details::SafePointerType<LinkId>
{
    using SafePointerType::SafePointerType;
};

struct PinId final: Details::SafePointerType<PinId>
{
    using SafePointerType::SafePointerType;
};


//------------------------------------------------------------------------------
} // namespace Editor
} // namespace ax


//------------------------------------------------------------------------------
# endif // __IMGUI_NODE_EDITOR_H__
