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
//    Enums
//------------------------------------------------------------------------------

// The kind of a pin, given to BeginPin(): an input or an output
enum class PinKind
{
    Input,
    Output
};

// The direction of the flow animation along a link, given to Flow()
enum class FlowDirection
{
    Forward,
    Backward
};

// How the view adapts when the editor's window is resized (Config::CanvasSizeMode)
enum class CanvasSizeMode
{
    FitVerticalView,        // Previous view will be scaled to fit new view on Y axis
    FitHorizontalView,      // Previous view will be scaled to fit new view on X axis
    CenterOnly,             // Previous view will be centered on new view
};


//------------------------------------------------------------------------------
//    Config
//------------------------------------------------------------------------------

// Why the editor saves its settings: given to the callbacks Config::SaveSettings and SaveNodeSettings
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

// The configuration of an editor, given to CreateEditor(): settings file, callbacks, mouse buttons, zoom
struct Config
{
    using CanvasSizeModeAlias = ax::NodeEditor::CanvasSizeMode;

    const char*             SettingsFile;           // Where the editor saves its state (nullptr: nowhere)
    ConfigSession           BeginSaveSession;
    ConfigSession           EndSaveSession;
    ConfigSaveSettings      SaveSettings;
    ConfigLoadSettings      LoadSettings;
    ConfigSaveNodeSettings  SaveNodeSettings;
    ConfigLoadNodeSettings  LoadNodeSettings;
    void*                   UserPointer;            // Passed to the callbacks above
    ImVector<float>         CustomZoomLevels;
    CanvasSizeModeAlias     CanvasSizeMode;         // How the view adapts when the editor's window is resized
    int                     DragButtonIndex;        // Mouse button index drag action will react to (0-left, 1-right, 2-middle)
    int                     SelectButtonIndex;      // Mouse button index select action will react to (0-left, 1-right, 2-middle)
    int                     NavigateButtonIndex;    // Mouse button index navigate action will react to (0-left, 1-right, 2-middle)
    int                     ContextMenuButtonIndex; // Mouse button index context menu action will react to (0-left, 1-right, 2-middle)
    bool                    EnableSmoothZoom;       // Smooth zoom with the wheel (false: steps through the zoom levels)
    float                   SmoothZoomPower;        // With smooth zoom, the zoom factor of one wheel step

    // Inside a node, Dear ImGui believes that the available width is the width of the window that hosts the editor:
    // Separator(), SeparatorText(), CollapsingHeader() and TextWrapped() go far beyond the node, and sliders / input fields
    // get a default width derived from the window.
    // Set ForceWindowContentWidthToNodeWidth to true so that they use the width of the node (false by default).
    // - All the text then wraps at the width of the node, so text does not give a width to the node: a node needs at least one
    //   item with a fixed width (Dummy, a widget preceded by SetNextItemWidth()...), otherwise it collapses (this is
    //   detected, and reported with an IM_ASSERT).
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
//    Style
//------------------------------------------------------------------------------

// The colors of an editor: the indices of Style::Colors (see PushStyleColor())
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

// The style variables that PushStyleVar() changes: the fields of Style
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

// The style of an editor (GetStyle()): sizes, roundings, the links, the flow animation, the colors
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
//    Functions
//------------------------------------------------------------------------------

struct EditorContext;


// --- Editor context lifecycle --------------------------------------------
// You may keep multiple editors and switch between them with SetCurrentEditor.
// Pass a Config to CreateEditor to set e.g. SettingsFile (where node positions
// are persisted) or to override the default mouse buttons.

// Makes this editor the current one: all the other functions apply to the current editor.
IMGUI_NODE_EDITOR_API void SetCurrentEditor(EditorContext* ctx);
IMGUI_NODE_EDITOR_API EditorContext* GetCurrentEditor(); // The current editor (nullptr if none)
// Creates an editor, with a copy of this config (or the default one). It does not become current (SetCurrentEditor).
IMGUI_NODE_EDITOR_API EditorContext* CreateEditor(const Config* config = nullptr);
IMGUI_NODE_EDITOR_API void DestroyEditor(EditorContext* ctx); // Destroys an editor created by CreateEditor()
// The config of this editor (nullptr: the current one). It is read-only: give your Config to CreateEditor().
IMGUI_NODE_EDITOR_API const Config& GetConfig(EditorContext* ctx = nullptr);

// --- Style ----------------------------------------------------------------
// Editor-specific style, separate from ImGui::GetStyle().
// Push/PopStyleColor and Push/PopStyleVar work like the ImGui equivalents.

IMGUI_NODE_EDITOR_API Style& GetStyle(); // The style of the current editor: its fields can be changed
IMGUI_NODE_EDITOR_API const char* GetStyleColorName(StyleColor colorIndex); // The name of a style color

// Pushes a style color until PopStyleColor(), e.g. PushStyleColor(StyleColor_NodeBg, color).
IMGUI_NODE_EDITOR_API void PushStyleColor(StyleColor colorIndex, const ImVec4& color);
IMGUI_NODE_EDITOR_API void PopStyleColor(int count = 1); // Pops the last `count` colors pushed by PushStyleColor()

// Pushes a style variable until PopStyleVar(). This one for a float variable, the next ones for an ImVec2 or an ImVec4.
IMGUI_NODE_EDITOR_API void PushStyleVar(StyleVar varIndex, float value);
IMGUI_NODE_EDITOR_API void PushStyleVar(StyleVar varIndex, const ImVec2& value); // PushStyleVar(), for an ImVec2
IMGUI_NODE_EDITOR_API void PushStyleVar(StyleVar varIndex, const ImVec4& value); // PushStyleVar(), for an ImVec4
IMGUI_NODE_EDITOR_API void PopStyleVar(int count = 1); // Pops the last `count` variables pushed by PushStyleVar()

// --- Frame ----------------------------------------------------------------
// All node-editor calls (BeginNode, Link, BeginCreate, ...) must be made
// between Begin() and End(), and Begin() must be called inside a real ImGui
// window. `id` distinguishes editor instances inside the same window;
// `size` matches ImGui::BeginChild semantics (0 = available).

// Starts drawing the current editor, in the current ImGui window: the other calls follow, until End().
IMGUI_NODE_EDITOR_API void Begin(const char* id, const ImVec2& size = ImVec2(0, 0));
IMGUI_NODE_EDITOR_API void End(); // Ends the editor started by Begin()

// --- Nodes & pins ---------------------------------------------------------
// Inside Begin/End: declare each node with BeginNode(id) ... EndNode(),
// and (optionally) declare its pins with BeginPin(id, kind) ... EndPin()
// in between. Anything you draw between the Begin/End is rendered inside
// the node; pins are typically wrapped around a Text/Button so the user
// has something to grab onto.

IMGUI_NODE_EDITOR_API void BeginNode(NodeId id); // Starts a node: its widgets follow, until EndNode()
IMGUI_NODE_EDITOR_API void BeginPin(PinId id, PinKind kind); // Starts a pin: its widgets follow, until EndPin()

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

IMGUI_NODE_EDITOR_API void PinRect(const ImVec2& a, const ImVec2& b); // Sets the pin's hover rectangle
IMGUI_NODE_EDITOR_API void PinPivotRect(const ImVec2& a, const ImVec2& b); // Sets the rectangle where links attach
IMGUI_NODE_EDITOR_API void PinPivotSize(const ImVec2& size); // Sets the pivot's size (-1 on an axis: the pin's)
IMGUI_NODE_EDITOR_API void PinPivotScale(const ImVec2& scale); // Scales the pivot's size
IMGUI_NODE_EDITOR_API void PinPivotAlignment(const ImVec2& alignment); // Where links attach: (0.5, 0.5) is the center
IMGUI_NODE_EDITOR_API void EndPin(); // Ends the pin started by BeginPin()

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

IMGUI_NODE_EDITOR_API void Group(const ImVec2& size); // Makes the current node a group, of this initial size
IMGUI_NODE_EDITOR_API void EndNode(); // Ends the node started by BeginNode()

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

IMGUI_NODE_EDITOR_API bool BeginGroupHint(NodeId nodeId); // True when zoomed out: draw the group's hint then
IMGUI_NODE_EDITOR_API ImVec2 GetGroupMin(); // The top left corner of the group, in screen coords (in a hint)
IMGUI_NODE_EDITOR_API ImVec2 GetGroupMax(); // The bottom right corner of the group, in screen coords (in a hint)
IMGUI_NODE_EDITOR_API ImDrawList* GetHintForegroundDrawList(); // A draw list above the editor's content (in a hint)
IMGUI_NODE_EDITOR_API ImDrawList* GetHintBackgroundDrawList(); // A draw list below the editor's content (in a hint)
IMGUI_NODE_EDITOR_API void EndGroupHint(); // Ends the group hint (see the example above)

// Returns the draw list used for the node's BACKGROUND layer (drawn under
// the node's content). Useful to add badges, highlights, etc. behind a node.
// TODO: Add a way to manage node background channels
IMGUI_NODE_EDITOR_API ImDrawList* GetNodeBackgroundDrawList(NodeId nodeId);

// Declares an existing link between two pins. Call once per frame for every
// link you want shown. Returns false when one of its pins was not drawn this frame.
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
//       EndCreate();                    // only when BeginCreate() returned true
//   }
//
// The QueryNewLink/QueryNewNode/AcceptNewItem overloads taking a color and
// thickness customize the in-progress link's drawing while the user drags.
//
// Once QueryNewLink() or QueryNewNode() returned true, and until EndCreate(), the editor is suspended (it draws the
// dragged link in screen space): ImGui::GetMousePos() and the cursor are then in SCREEN coords, while everywhere else
// between Begin() and End() they are in CANVAS coords. To place a new node at the mouse, use GetMousePosOnCanvas().

// Starts the create action: true while the user drags a link from a pin. Then call EndCreate().
IMGUI_NODE_EDITOR_API bool BeginCreate(const ImVec4& color = ImVec4(0, 0, 0, 0), float thickness = 1.0f);
// True while the dragged link is not over empty space: the pin it starts from, and the pin under the mouse (0 if none).
IMGUI_NODE_EDITOR_API bool QueryNewLink(PinId* startId, PinId* endId);
// QueryNewLink(), with the color and thickness of the dragged link.
IMGUI_NODE_EDITOR_API bool QueryNewLink(PinId* startId, PinId* endId, const ImVec4& color, float thickness = 1.0f);
IMGUI_NODE_EDITOR_API bool QueryNewNode(PinId* pinId); // True while the dragged link is over empty space: its pin
// QueryNewNode(), with the color and thickness of the dragged link.
IMGUI_NODE_EDITOR_API bool QueryNewNode(PinId* pinId, const ImVec4& color, float thickness = 1.0f);
IMGUI_NODE_EDITOR_API bool AcceptNewItem(); // Accepts the queried link or node: true when the mouse is released
// AcceptNewItem(), with the color and thickness of the dragged link.
IMGUI_NODE_EDITOR_API bool AcceptNewItem(const ImVec4& color, float thickness = 1.0f);
IMGUI_NODE_EDITOR_API void RejectNewItem(); // Refuses the queried link or node: the dragged link shows it
// RejectNewItem(), with the color and thickness of the dragged link (e.g. red).
IMGUI_NODE_EDITOR_API void RejectNewItem(const ImVec4& color, float thickness = 1.0f);
IMGUI_NODE_EDITOR_API void EndCreate(); // Ends the create action: only when BeginCreate() returned true

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

IMGUI_NODE_EDITOR_API bool BeginDelete(); // Starts the delete action: true when items are to be deleted this frame
// True for each link to delete, one per call: its id, and its pins if you want them.
IMGUI_NODE_EDITOR_API bool QueryDeletedLink(LinkId* linkId, PinId* startId = nullptr, PinId* endId = nullptr);
IMGUI_NODE_EDITOR_API bool QueryDeletedNode(NodeId* nodeId); // True for each node to delete, one per call: its id
// Accepts the deletion of the queried item: then remove it from your data (see deleteDependencies above).
IMGUI_NODE_EDITOR_API bool AcceptDeletedItem(bool deleteDependencies = true);
IMGUI_NODE_EDITOR_API void RejectDeletedItem(); // Refuses the deletion of the queried item: it stays
IMGUI_NODE_EDITOR_API void EndDelete(); // Ends the delete action (harmless when BeginDelete() returned false)

// --- Node geometry --------------------------------------------------------
// Positions and sizes are in EDITOR (canvas) space, not screen space. Use
// CanvasToScreen / ScreenToCanvas to convert.
// GetNodeSize returns (0,0) on the very first frame a node is drawn (the
// editor has no measurement yet). It stabilizes immediately after.

// Sets a node's position, in canvas coords (e.g. once, when you create it): the editor keeps it afterwards.
IMGUI_NODE_EDITOR_API void SetNodePosition(NodeId nodeId, const ImVec2& editorPosition);
IMGUI_NODE_EDITOR_API void SetGroupSize(NodeId nodeId, const ImVec2& size); // Sets the size of a group node
IMGUI_NODE_EDITOR_API ImVec2 GetNodePosition(NodeId nodeId); // In canvas coords; (FLT_MAX, FLT_MAX) if unknown
IMGUI_NODE_EDITOR_API ImVec2 GetNodeSize(NodeId nodeId); // In canvas coords; (0, 0) before the node was drawn
// Moves the node (a group: with its nodes) to the center of the view, when it is next drawn.
IMGUI_NODE_EDITOR_API void CenterNodeOnScreen(NodeId nodeId);
IMGUI_NODE_EDITOR_API void SetNodeZPosition(NodeId nodeId, float z); // Sets node z position, nodes with higher value are drawn over nodes with lower value
IMGUI_NODE_EDITOR_API float GetNodeZPosition(NodeId nodeId); // Returns node z position, defaults is 0.0f

// Re-load the node's position/size from the editor's persisted settings
// (the SettingsFile, if any). Useful right after creating a node whose
// previous layout you want to bring back without the user having to drag it.
IMGUI_NODE_EDITOR_API void RestoreNodeState(NodeId nodeId);

// --- Suspend / Resume -----------------------------------------------------
// Temporarily disable the editor's input/canvas state machine: positions are then in SCREEN coords. With a stock
// Dear ImGui, you MUST suspend before calling ImGui popup APIs like ImGui::OpenPopup or ImGui::BeginPopup that should
// appear ABOVE the canvas (otherwise the popup's coordinates and event capture will be wrong). With a Dear ImGui that
// has the patches of docs/fork_imgui_bundle.md (chapter 3), popups work without it. Resume() restores editor input
// handling. See the ShowNodeContextMenu example below.

IMGUI_NODE_EDITOR_API void Suspend(); // Suspends the canvas: positions are in screen coords until Resume()
IMGUI_NODE_EDITOR_API void Resume(); // Resumes the canvas suspended by Suspend()
IMGUI_NODE_EDITOR_API bool IsSuspended(); // True between Suspend() and Resume()

// --- InputTextMultiline inside a node ---------------------------------------
// ImGui::InputTextMultiline() uses a child window, and child windows do not work inside the editor.
// This version shows a read-only preview box with the requested size, and opens a resizable popup with the real
// editor when the box is clicked. Outside of the editor, it calls ImGui::InputTextMultiline().
// With a Dear ImGui that provides ImGuiContext::InputTextMultilineOverride, you do not need to call it:
// ImGui::InputTextMultiline() does the same thing when called inside a node.
IMGUI_NODE_EDITOR_API bool InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);

// True when the editor's window has the focus: the editor's keyboard shortcuts work only then.
IMGUI_NODE_EDITOR_API bool IsActive();

// --- Selection ------------------------------------------------------------
// HasSelectionChanged returns true for one frame after the selection set
// changed (use it to react to selection changes once, not every frame).
// SelectNode/SelectLink with append=false replaces the current selection.

IMGUI_NODE_EDITOR_API bool HasSelectionChanged(); // True during the frame after the selection changed
IMGUI_NODE_EDITOR_API int  GetSelectedObjectCount(); // The number of selected nodes and links
IMGUI_NODE_EDITOR_API int  GetSelectedNodes(NodeId* nodes, int size); // Fills at most `size` nodes; returns their count
IMGUI_NODE_EDITOR_API int  GetSelectedLinks(LinkId* links, int size); // Fills at most `size` links; returns their count
IMGUI_NODE_EDITOR_API bool IsNodeSelected(NodeId nodeId); // True if the node is selected
IMGUI_NODE_EDITOR_API bool IsLinkSelected(LinkId linkId); // True if the link is selected
IMGUI_NODE_EDITOR_API void ClearSelection(); // Deselects all the nodes and links
IMGUI_NODE_EDITOR_API void SelectNode(NodeId nodeId, bool append = false); // Selects a node (append: keep the others)
IMGUI_NODE_EDITOR_API void SelectLink(LinkId linkId, bool append = false); // Selects a link (append: keep the others)
IMGUI_NODE_EDITOR_API void DeselectNode(NodeId nodeId); // Removes a node from the selection
IMGUI_NODE_EDITOR_API void DeselectLink(LinkId linkId); // Removes a link from the selection

// Programmatically queue a node/link for deletion. The next BeginDelete()
// loop will yield it via QueryDeletedNode/QueryDeletedLink.

IMGUI_NODE_EDITOR_API bool DeleteNode(NodeId nodeId); // Queues a node for deletion (see BeginDelete())
IMGUI_NODE_EDITOR_API bool DeleteLink(LinkId linkId); // Queues a link for deletion (see BeginDelete())

IMGUI_NODE_EDITOR_API bool HasAnyLinks(NodeId nodeId); // Returns true if node has any link connected
IMGUI_NODE_EDITOR_API bool HasAnyLinks(PinId pinId); // Return true if pin has any link connected
IMGUI_NODE_EDITOR_API int BreakLinks(NodeId nodeId); // Break all links connected to this node
IMGUI_NODE_EDITOR_API int BreakLinks(PinId pinId); // Break all links connected to this pin

// --- Navigation -----------------------------------------------------------
// Programmatic equivalents of pressing F (with no modifier and with Shift).
// `duration` is the animation length in seconds; -1 means "use the editor
// default". NavigateToSelection requires a non-empty selection.

// Moves the view to show all the nodes, as F does. Call it after End(). New nodes are measured over two frames:
// to fit them, call it at their third frame.
IMGUI_NODE_EDITOR_API void NavigateToContent(float duration = -1);
// Moves the view to the selected nodes, as Shift+F does (zoomIn: zoom in too, to fit them).
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
// (With the Dear ImGui patches of docs/fork_imgui_bundle.md, the Suspend() / Resume() pairs are not needed.)

IMGUI_NODE_EDITOR_API bool ShowNodeContextMenu(NodeId* nodeId); // True when the user opens a node's menu: its id
IMGUI_NODE_EDITOR_API bool ShowPinContextMenu(PinId* pinId); // True when the user opens a pin's menu: its id
IMGUI_NODE_EDITOR_API bool ShowLinkContextMenu(LinkId* linkId); // True when the user opens a link's menu: its id
IMGUI_NODE_EDITOR_API bool ShowBackgroundContextMenu(); // True when the user opens the background's menu

// --- Keyboard shortcuts ---------------------------------------------------
// Master switch: when disabled the editor never reacts to F / Ctrl+X /
// Ctrl+C / Ctrl+V / Ctrl+D / Space etc. Useful when an ImGui text input
// has focus and you want shortcuts ignored.

IMGUI_NODE_EDITOR_API void EnableShortcuts(bool enable); // Enables or disables the editor's keyboard shortcuts
IMGUI_NODE_EDITOR_API bool AreShortcutsEnabled(); // True if the keyboard shortcuts are enabled

// --- Shortcut handling protocol ------------------------------------------
// Lets you ASK the editor which keyboard shortcut fired this frame and
// respond to it. Pattern (inside Begin/End):
//
//   if (BeginShortcut()) {
//       if (AcceptCopy())       /* user pressed Ctrl+C: copy selection */;
//       if (AcceptPaste())      /* user pressed Ctrl+V: paste at mouse */;
//       if (AcceptCut())        /* user pressed Ctrl+X */;
//       if (AcceptDuplicate())  /* user pressed Ctrl+D */;
//       if (AcceptCreateNode()) /* user pressed Space */;
//   }
//   EndShortcut();
//
// GetActionContextNodes / GetActionContextLinks return the objects the
// shortcut applies to (typically the current selection at the moment the
// shortcut fired). They are valid only between Begin/EndShortcut.

IMGUI_NODE_EDITOR_API bool BeginShortcut(); // Starts the shortcut action: true when a shortcut fired this frame
IMGUI_NODE_EDITOR_API bool AcceptCut(); // True if the shortcut is Cut (Ctrl+X)
IMGUI_NODE_EDITOR_API bool AcceptCopy(); // True if the shortcut is Copy (Ctrl+C)
IMGUI_NODE_EDITOR_API bool AcceptPaste(); // True if the shortcut is Paste (Ctrl+V)
IMGUI_NODE_EDITOR_API bool AcceptDuplicate(); // True if the shortcut is Duplicate (Ctrl+D)
IMGUI_NODE_EDITOR_API bool AcceptCreateNode(); // True if the shortcut is Create a node (Space)
IMGUI_NODE_EDITOR_API int  GetActionContextSize(); // The number of nodes and links the shortcut applies to
IMGUI_NODE_EDITOR_API int  GetActionContextNodes(NodeId* nodes, int size); // Fills `nodes`; returns the count
IMGUI_NODE_EDITOR_API int  GetActionContextLinks(LinkId* links, int size); // Fills `links`; returns the count
IMGUI_NODE_EDITOR_API void EndShortcut(); // Ends the shortcut action (harmless when BeginShortcut() returned false)

// Returns the INVERSE of the zoom: the size of a pixel in canvas units.
// 1.0 at 100%, 2.0 when the content is drawn at half size (zoomed out), 0.5 when it is drawn twice as big (zoomed in).
// To convert positions, use ScreenToCanvas() / CanvasToScreen().
IMGUI_NODE_EDITOR_API float GetCurrentZoom();

// --- Input queries (call between Begin and End) ---------------------------
// These return the object under the mouse this frame (NodeId/PinId/LinkId,
// 0 if none) and which buttons were clicked or double-clicked on the empty
// background. The "BackgroundClick" pair returns -1 when no click happened.

IMGUI_NODE_EDITOR_API NodeId GetHoveredNode(); // The node under the mouse (0 if none)
IMGUI_NODE_EDITOR_API PinId GetHoveredPin(); // The pin under the mouse (0 if none)
IMGUI_NODE_EDITOR_API LinkId GetHoveredLink(); // The link under the mouse (0 if none)
IMGUI_NODE_EDITOR_API NodeId GetDoubleClickedNode(); // The node double-clicked this frame (0 if none)
IMGUI_NODE_EDITOR_API PinId GetDoubleClickedPin(); // The pin double-clicked this frame (0 if none)
IMGUI_NODE_EDITOR_API LinkId GetDoubleClickedLink(); // The link double-clicked this frame (0 if none)
IMGUI_NODE_EDITOR_API bool IsBackgroundClicked(); // True if the background was clicked this frame
IMGUI_NODE_EDITOR_API bool IsBackgroundDoubleClicked(); // True if the background was double-clicked this frame
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

IMGUI_NODE_EDITOR_API ImVec2 GetScreenSize(); // The size of the editor on screen, in pixels
IMGUI_NODE_EDITOR_API ImVec2 ScreenToCanvas(const ImVec2& pos); // Converts a position from screen to canvas coords
IMGUI_NODE_EDITOR_API ImVec2 CanvasToScreen(const ImVec2& pos); // Converts a position from canvas to screen coords
// The mouse position in CANVAS coords, anywhere between Begin() and End(). ImGui::GetMousePos() gives the same, except
// where the editor is suspended (after Suspend(), and in the create action once QueryNewLink() or QueryNewNode()
// returned true): it then gives SCREEN coords.
IMGUI_NODE_EDITOR_API ImVec2 GetMousePosOnCanvas();

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

// You choose the values of the ids (e.g. an index, or a pointer). 0 means none: the id then converts to false.
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
