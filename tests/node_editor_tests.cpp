# ifndef IMGUI_DEFINE_MATH_OPERATORS
#     define IMGUI_DEFINE_MATH_OPERATORS
# endif
# include "node_editor_tests.h"
# include "../imgui_node_editor.h"
# include <imgui.h>
# include <imgui_internal.h>
# include "imgui_test_engine/imgui_te_engine.h"
# include "imgui_test_engine/imgui_te_context.h"
# include <map>
# include <string>

namespace ed = ax::NodeEditor;

// Draw callback that the canvas uses as a marker inside the draw list (same value as in imgui_canvas.cpp)
# ifndef ImDrawCallback_ImCanvas
#     define ImDrawCallback_ImCanvas        (ImDrawCallback)(-2)
# endif

//------------------------------------------------------------------------------
// Scene
//------------------------------------------------------------------------------
static const char* const gComboItems[] = { "AAAA", "BBBB", "CCCC", "DDDD", "EEEE", "FFFF", "GGGG", "HHHH", "IIII", "JJJJ", "KKKK", "LLLL", "MMMM", "NNNN" };

struct Scene
{
    ed::EditorContext* Editor = nullptr;
    bool   FirstFrame = true;

    // State edited by the widgets: tests reset it, interact, then check it
    int    ComboIdx = 0;
    ImVec4 Color = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);
    char   Text[1024] = "Line 1\nLine 2\nLine 3";
    char   WideText[1024] = "Lorem ipsum dolor sit amet,\nconsectetur adipiscing elit";
    int    PlainPopupClicks = 0;
    int    LegacyPopupClicks = 0;
    int    LayoutComboIdx = 0;
    ImVec4 PinColor = ImVec4(0.8f, 0.3f, 0.2f, 1.0f);
    int    ChannelsPopupClicks = 0;
    int    BackNodeClicks = 0;
    int    FrontNodeClicks = 0;
    std::string LastMenuItem;
    ed::NodeId  ContextNodeId = 0;
    bool   MoveWidgetsNodeRequest = false;   // tests ask the scene to move the first node (it must be done inside ed::Begin / ed::End)
    ImVec2 MoveWidgetsNodePos;

    // Screen rect of the widgets placed inside nodes, updated each frame.
    // Inside a node, items are registered in canvas coordinates, so the test engine cannot reach them by name:
    // tests move the mouse to those rects instead.
    std::map<std::string, ImRect> ItemRects;
    ImVector<ImRect> NodeRects;   // screen rect of the nodes (index = node id - 1)

    // Draw list of the window, inspected at each frame right after ed::End(). The counters accumulate until a test resets them.
    int    SentinelCmdCount = 0;      // canvas markers left in the draw list: the renderer would call them as a function (crash)
    int    BadClipRectCount = 0;      // draw commands of the editor whose clip rect is not inside the window
    ImRect FirstBadClipRect;
    int    VisibleCmdCount = 0;       // in the last frame: draw commands of the editor that draw something inside the canvas
    int    VtxCount = 0;              // in the last frame: number of vertices in the draw list
    int    EmptyCmdCount = 0;         // in the last frame: draw commands of the editor that have no element
    int    UncoveredNodeCount = 0;    // visible nodes that no draw command can draw entirely (their clip rects are too small)

    // View of the canvas, updated each frame (tests zoom with the mouse wheel and pan with a right button drag)
    float  Zoom = 1.0f;
    ImRect CanvasRect;   // screen rect of the canvas
    ImVec2 ZoomAnchor;   // screen position of an empty spot next to the top-left of the first node: zooming around it keeps the node visible
};
static Scene gScene;

// Call right after a widget placed inside a node
static void RecordLastItem(const char* name)
{
    gScene.ItemRects[name] = ImRect(ed::CanvasToScreen(ImGui::GetItemRectMin()), ed::CanvasToScreen(ImGui::GetItemRectMax()));
}

// Widgets that open a popup or a tooltip, used the way one would use them outside of a node (no Suspend / Resume)
static void ShowWidgetsNode()
{
    const float em = ImGui::GetFontSize();

    ed::BeginNode(ed::NodeId(1));
    ImGui::TextUnformatted("widgets");
    RecordLastItem("widgets_title");
    ImGui::PushItemWidth(em * 10.0f);

    if (ImGui::BeginCombo("combo", gComboItems[gScene.ComboIdx]))
    {
        for (int n = 0; n < IM_ARRAYSIZE(gComboItems); n++)
        {
            const bool is_selected = (gScene.ComboIdx == n);
            if (ImGui::Selectable(gComboItems[n], is_selected))
                gScene.ComboIdx = n;
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    RecordLastItem("combo");

    ImGui::ColorEdit4("color", &gScene.Color.x, ImGuiColorEditFlags_NoInputs);
    RecordLastItem("color");

    ImGui::InputTextMultiline("text", gScene.Text, IM_ARRAYSIZE(gScene.Text), ImVec2(em * 10.0f, em * 4.0f));
    RecordLastItem("text");

    ImGui::Button("tooltip target");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("tooltip text");
    RecordLastItem("tooltip_target");

    if (ImGui::Button("open popup"))
        ImGui::OpenPopup("plain_popup");
    RecordLastItem("open_popup");
    if (ImGui::BeginPopup("plain_popup"))
    {
        ImGui::TextUnformatted("A popup opened from a node");
        if (ImGui::Button("popup button"))
            gScene.PlainPopupClicks++;
        ImGui::EndPopup();
    }

    ImGui::PopItemWidth();
    ed::EndNode();
}

// User code written for the upstream node editor: popups are wrapped in Suspend / Resume. It must keep working.
static void ShowLegacyNode()
{
    ed::BeginNode(ed::NodeId(2));
    ImGui::TextUnformatted("legacy (Suspend / Resume)");

    if (ImGui::Button("open legacy popup"))
    {
        ed::Suspend();
        ImGui::OpenPopup("legacy_popup");
        ed::Resume();
    }
    RecordLastItem("open_legacy_popup");
    ed::Suspend();
    if (ImGui::BeginPopup("legacy_popup"))
    {
        ImGui::TextUnformatted("A popup opened between Suspend and Resume");
        if (ImGui::Button("popup button"))
            gScene.LegacyPopupClicks++;
        ImGui::EndPopup();
    }
    ed::Resume();

    ed::EndNode();
}

// Context menus follow the pattern documented in imgui_node_editor.h, with Suspend / Resume.
// Known limit: outside of a node, a popup begun without Suspend / Resume asserts in ImGuiEx::Canvas::Suspend()
// (the draw list is not on the channel that the canvas expects), so this case is not part of the scene.
// Widgets that span the available width: with Config::ForceWindowContentWidthToNodeWidth, they use the width of the node
// (without it they would use the width of the window, and the node would grow at each frame)
static void ShowWidthNode()
{
    const float em = ImGui::GetFontSize();

    ed::BeginNode(ed::NodeId(3));
    ImGui::TextUnformatted("width");
    RecordLastItem("width_title");
    ImGui::Dummy(ImVec2(em * 16.0f, 0.0f));   // this is what gives its width to the node

    ImGui::SeparatorText("separator text");
    RecordLastItem("width_separator_text");
    ImGui::Separator();
    RecordLastItem("width_separator");
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::CollapsingHeader("collapsing header"))
    {
        RecordLastItem("width_header");
        ImGui::InputTextMultiline("##wide_text", gScene.WideText, IM_ARRAYSIZE(gScene.WideText), ImVec2(0.0f, em * 3.0f));
        RecordLastItem("width_text");
    }

    ed::EndNode();
}

// A combo inside a horizontal layout, after a spring
static void ShowLayoutNode()
{
    const float em = ImGui::GetFontSize();

    ed::BeginNode(ed::NodeId(4));
    ImGui::TextUnformatted("layout");
    RecordLastItem("layout_title");
    ImGui::BeginHorizontal("layout_h", ImVec2(em * 9.0f, 0.0f));
    ImGui::TextUnformatted("left");
    RecordLastItem("layout_left");
    ImGui::Spring();
    ImGui::SetNextItemWidth(em * 5.0f);
    if (ImGui::BeginCombo("##layout_combo", gComboItems[gScene.LayoutComboIdx]))
    {
        for (int n = 0; n < IM_ARRAYSIZE(gComboItems); n++)
            if (ImGui::Selectable(gComboItems[n], gScene.LayoutComboIdx == n))
                gScene.LayoutComboIdx = n;
        ImGui::EndCombo();
    }
    RecordLastItem("layout_combo");
    ImGui::EndHorizontal();
    ed::EndNode();
}

// Two nodes linked from the right one to the left one: the link would pass through both nodes,
// which is the case where Style::AngledLinks routes it around them
static void ShowLinkedNodes()
{
    // With Config::ForceWindowContentWidthToNodeWidth, text wraps at the width of the node:
    // a node that contains only text needs something that gives it a width
    const float em = ImGui::GetFontSize();

    ed::BeginNode(ed::NodeId(5));
    ImGui::Dummy(ImVec2(em * 6.0f, 0.0f));
    ImGui::TextUnformatted("link target");
    RecordLastItem("link_title");
    ed::BeginPin(ed::PinId(51), ed::PinKind::Input);
    ImGui::TextUnformatted("-> in");
    ed::EndPin();
    ed::EndNode();

    ed::BeginNode(ed::NodeId(6));
    ImGui::Dummy(ImVec2(em * 6.0f, 0.0f));
    ImGui::TextUnformatted("link source");
    ed::BeginPin(ed::PinId(61), ed::PinKind::Output);
    ImGui::TextUnformatted("out ->");
    ed::EndPin();
    ed::EndNode();

    ed::Link(ed::LinkId(100), ed::PinId(61), ed::PinId(51));
}

// A color editor inside a pin
static void ShowPinsNode()
{
    ed::BeginNode(ed::NodeId(7));
    ImGui::Dummy(ImVec2(ImGui::GetFontSize() * 8.0f, 0.0f));   // gives its width to the node (its text wraps at the width of the node)
    ImGui::TextUnformatted("pins");
    RecordLastItem("pins_title");
    ed::BeginPin(ed::PinId(71), ed::PinKind::Input);
    ImGui::ColorEdit4("pin color", &gScene.PinColor.x, ImGuiColorEditFlags_NoInputs);
    RecordLastItem("pin_color");
    ed::EndPin();
    ed::EndNode();
}

// A node whose content splits the draw list into channels (to draw something behind its widgets),
// and which opens a popup while a channel other than the first one is current
static void ShowChannelsNode()
{
    ed::BeginNode(ed::NodeId(8));
    ImGui::Dummy(ImVec2(ImGui::GetFontSize() * 10.0f, 0.0f));   // gives its width to the node
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->ChannelsSplit(2);
    draw_list->ChannelsSetCurrent(1);   // foreground: the widgets

    ImGui::TextUnformatted("draw channels");
    RecordLastItem("channels_title");
    const ImVec2 background_min = ImGui::GetItemRectMin();
    if (ImGui::Button("open popup##channels"))
        ImGui::OpenPopup("channels_popup");
    RecordLastItem("channels_open_popup");
    const ImVec2 background_max = ImGui::GetItemRectMax();
    if (ImGui::BeginPopup("channels_popup"))
    {
        ImGui::TextUnformatted("A popup opened while the draw channel 1 is current");
        if (ImGui::Button("popup button"))
            gScene.ChannelsPopupClicks++;
        ImGui::EndPopup();
    }

    draw_list->ChannelsSetCurrent(0);   // background: a rectangle behind the widgets
    draw_list->AddRectFilled(background_min, background_max, IM_COL32(60, 90, 140, 255));
    draw_list->ChannelsMerge();
    ed::EndNode();
}

// Two nodes that overlap: the second one is drawn over the first one, and their buttons overlap too.
// Dear ImGui gives the hover to the first item submitted: without care, the button of the node BEHIND would get the clicks.
static void ShowOverlappingNodes()
{
    const float em = ImGui::GetFontSize();

    ed::BeginNode(ed::NodeId(9));
    ImGui::TextUnformatted("behind");
    if (ImGui::Button("button##behind", ImVec2(em * 8.0f, em * 2.0f)))
        gScene.BackNodeClicks++;
    ed::EndNode();

    ed::BeginNode(ed::NodeId(10));
    ImGui::TextUnformatted("in front");
    RecordLastItem("front_title");
    if (ImGui::Button("button##front", ImVec2(em * 8.0f, em * 2.0f)))
        gScene.FrontNodeClicks++;
    RecordLastItem("front_button");
    ed::EndNode();
}

// Inspect the draw commands that the editor added to the draw list of the window (call right after ed::End())
static void InspectDrawList(int first_cmd)
{
    const ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImRect window_rect = ImGui::GetCurrentWindow()->Rect();
    window_rect.Expand(1.0f);
    gScene.VisibleCmdCount = 0;
    gScene.VtxCount = draw_list->VtxBuffer.Size;
    int empty_cmd_count = 0;
    for (int i = 0; i < draw_list->CmdBuffer.Size; i++)
    {
        const ImDrawCmd& cmd = draw_list->CmdBuffer[i];
        if (cmd.UserCallback == ImDrawCallback_ImCanvas)
            gScene.SentinelCmdCount++;
        if (i >= first_cmd && cmd.ElemCount == 0 && cmd.UserCallback == nullptr)
            empty_cmd_count++;
        if (i < first_cmd || cmd.ElemCount == 0 || cmd.UserCallback != nullptr)
            continue;
        const ImRect clip_rect(cmd.ClipRect.x, cmd.ClipRect.y, cmd.ClipRect.z, cmd.ClipRect.w);
        if (!window_rect.Contains(clip_rect))
        {
            if (gScene.BadClipRectCount == 0)
                gScene.FirstBadClipRect = clip_rect;
            gScene.BadClipRectCount++;
        }
        else if (clip_rect.Overlaps(gScene.CanvasRect))
            gScene.VisibleCmdCount++;
    }

    gScene.EmptyCmdCount = empty_cmd_count;

    // The visible part of each node must be inside the clip rect of at least one draw command of the editor.
    // (when a popup or a docked window shrinks the clip rects of what was submitted before it, a part of a node disappears)
    for (const ImRect& node_rect : gScene.NodeRects)
    {
        ImRect visible_rect = node_rect;
        visible_rect.ClipWith(gScene.CanvasRect);
        visible_rect.Expand(-2.0f);
        if (visible_rect.GetWidth() <= 0.0f || visible_rect.GetHeight() <= 0.0f)
            continue;
        bool is_covered = false;
        for (int i = first_cmd; i < draw_list->CmdBuffer.Size && !is_covered; i++)
        {
            const ImDrawCmd& cmd = draw_list->CmdBuffer[i];
            if (cmd.ElemCount > 0 && cmd.UserCallback == nullptr)
                is_covered = ImRect(cmd.ClipRect.x, cmd.ClipRect.y, cmd.ClipRect.z, cmd.ClipRect.w).Contains(visible_rect);
        }
        if (!is_covered)
            gScene.UncoveredNodeCount++;
    }
}

static void ShowContextMenus()
{
    ed::Suspend();
    if (ed::ShowNodeContextMenu(&gScene.ContextNodeId))
        ImGui::OpenPopup("node_menu");
    ed::Resume();
    ed::Suspend();
    if (ImGui::BeginPopup("node_menu"))
    {
        if (ImGui::MenuItem("node item"))
            gScene.LastMenuItem = "node item";
        ImGui::EndPopup();
    }
    ed::Resume();

    // Background menu: without Suspend / Resume (possible when Dear ImGui provides the BeginWindow / EndWindow hooks)
# if defined(IMGUI_HAS_CONTEXT_HOOK_BEGIN_WINDOW)
    if (ed::ShowBackgroundContextMenu())
        ImGui::OpenPopup("background_menu");
    if (ImGui::BeginPopup("background_menu"))
    {
        if (ImGui::MenuItem("background item"))
            gScene.LastMenuItem = "background item";
        ImGui::EndPopup();
    }
# else
    ed::Suspend();
    if (ed::ShowBackgroundContextMenu())
        ImGui::OpenPopup("background_menu");
    if (ImGui::BeginPopup("background_menu"))
    {
        if (ImGui::MenuItem("background item"))
            gScene.LastMenuItem = "background item";
        ImGui::EndPopup();
    }
    ed::Resume();
# endif
}

void NodeEditorTests_ShowGui()
{
    if (gScene.Editor == nullptr)
    {
        ed::Config config;
        config.SettingsFile = "";          // no settings file: each run starts from the same state
        config.EnableSmoothZoom = false;   // with the zoom levels below, each mouse wheel step gives a known zoom
        config.CustomZoomLevels.push_back(0.5f);
        config.CustomZoomLevels.push_back(1.0f);
        config.CustomZoomLevels.push_back(2.0f);
        config.ForceWindowContentWidthToNodeWidth = true;
        gScene.Editor = ed::CreateEditor(&config);
    }

    const float em = ImGui::GetFontSize();
    // Placed on the right, so that it does not cover the test engine window (which opens on the left)
    ImGui::SetNextWindowPos(ImVec2(em * 54.0f, em * 2.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(em * 38.0f, em * 55.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Node editor tests"))
    {
        // Computed before ed::Begin(): between ed::Begin() and ed::End(), the window and the cursor are in canvas coordinates
        gScene.CanvasRect = ImRect(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImGui::GetContentRegionAvail());

        const int first_cmd = ImGui::GetWindowDrawList()->CmdBuffer.Size;

        ed::SetCurrentEditor(gScene.Editor);
        ed::Begin("editor");
        if (gScene.FirstFrame)
        {
            ed::SetNodePosition(ed::NodeId(1), ImVec2(em * 2.0f, em * 2.0f));
            ed::SetNodePosition(ed::NodeId(2), ImVec2(em * 2.0f, em * 18.0f));
            ed::SetNodePosition(ed::NodeId(3), ImVec2(em * 20.0f, em * 2.0f));
            ed::SetNodePosition(ed::NodeId(4), ImVec2(em * 20.0f, em * 14.0f));
            ed::SetNodePosition(ed::NodeId(5), ImVec2(em * 2.0f, em * 24.0f));
            ed::SetNodePosition(ed::NodeId(6), ImVec2(em * 12.0f, em * 28.0f));
            ed::SetNodePosition(ed::NodeId(7), ImVec2(em * 40.0f, em * 2.0f));
            ed::SetNodePosition(ed::NodeId(8), ImVec2(em * 40.0f, em * 10.0f));
            ed::SetNodePosition(ed::NodeId(9), ImVec2(em * 40.0f, em * 20.0f));
            ed::SetNodePosition(ed::NodeId(10), ImVec2(em * 41.0f, em * 20.5f));
        }
        if (gScene.MoveWidgetsNodeRequest)
        {
            ed::SetNodePosition(ed::NodeId(1), gScene.MoveWidgetsNodePos);
            gScene.MoveWidgetsNodeRequest = false;
        }
        ShowWidgetsNode();
        ShowLegacyNode();
        ShowWidthNode();
        ShowLayoutNode();
        ShowLinkedNodes();
        ShowPinsNode();
        ShowChannelsNode();
        ShowOverlappingNodes();
        ShowContextMenus();
        gScene.NodeRects.clear();
        for (int node_id = 1; node_id <= 10; node_id++)
        {
            const ImVec2 node_pos = ed::GetNodePosition(ed::NodeId(node_id));
            gScene.NodeRects.push_back(ImRect(ed::CanvasToScreen(node_pos), ed::CanvasToScreen(node_pos + ed::GetNodeSize(ed::NodeId(node_id)))));
        }
        gScene.Zoom = 1.0f / ed::GetCurrentZoom(); // GetCurrentZoom() returns the inverse of the scale: 2 when the content is drawn at half size
        gScene.ZoomAnchor = ed::CanvasToScreen(ImVec2(em, em));
        ed::End();
        InspectDrawList(first_cmd);
        ed::SetCurrentEditor(nullptr);
        gScene.FirstFrame = false;
    }
    ImGui::End();

# ifdef IMGUI_HAS_DOCK
    // A window to dock with (see the test "docked")
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGui::SetNextWindowPos(ImVec2(em * 4.0f, em * 46.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(em * 30.0f, em * 8.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Node editor tests (dock target)");
        ImGui::TextUnformatted("The test \"docked\" docks this window with the editor.");
        ImGui::End();
    }
# endif
}

void NodeEditorTests_Shutdown()
{
    if (gScene.Editor != nullptr)
        ed::DestroyEditor(gScene.Editor);
    gScene = Scene();
}

//------------------------------------------------------------------------------
// Test helpers
//------------------------------------------------------------------------------
static ImGuiWindow* TopPopupWindow()
{
    ImGuiContext& g = *GImGui;
    return (g.OpenPopupStack.Size > 0) ? g.OpenPopupStack.back().Window : nullptr;
}

// Zoom with the mouse wheel: the editor was created with the zoom levels 0.5 / 1 / 2, so each wheel step gives a known zoom
static void SetZoom(ImGuiTestContext* ctx, float zoom)
{
    for (int i = 0; i < 4 && ImAbs(gScene.Zoom - zoom) > 0.01f; i++)
    {
        ctx->MouseMoveToPos(ImClamp(gScene.ZoomAnchor, gScene.CanvasRect.Min + ImVec2(2.0f, 2.0f), gScene.CanvasRect.Max - ImVec2(2.0f, 2.0f)));
        ctx->MouseWheelY((gScene.Zoom < zoom) ? 1.0f : -1.0f);
        // The zoom is animated over a fraction of a second: wait until it did not change for 4 frames.
        // The wait is bounded in time, not in frames: without vsync (on a CI server) frames can be very short.
        const double start_time = ImGui::GetTime();
        float previous_zoom = -1.0f;
        for (int stable_frames = 0; stable_frames < 4 && ImGui::GetTime() - start_time < 3.0; )
        {
            stable_frames = (previous_zoom == gScene.Zoom) ? stable_frames + 1 : 0;
            previous_zoom = gScene.Zoom;
            ctx->Yield();
        }
    }
    IM_CHECK_LT(ImAbs(gScene.Zoom - zoom), 0.01f);
}

// Pan with a right button drag. The drag is centered on the canvas, so that the mouse stays inside it.
static void PanView(ImGuiTestContext* ctx, ImVec2 delta)
{
    const ImVec2 start = gScene.CanvasRect.GetCenter() - delta * 0.5f;
    ctx->MouseMoveToPos(start);
    ctx->MouseDown(1);
    ctx->MouseMoveToPos(start + delta);
    ctx->MouseUp(1);
    ctx->Yield(2);
}

// Pan until a recorded item lies in the upper left part of the canvas, which leaves room for a popup below it and on its right
static void BringIntoView(ImGuiTestContext* ctx, const char* name)
{
    IM_CHECK(gScene.ItemRects.count(name) == 1);
    const float em = ImGui::GetFontSize();
    for (int i = 0; i < 20; i++)   // an item that is far away needs several pans
    {
        const ImRect item = gScene.ItemRects[name];
        // (a color picker popup is about 22 em wide and tall: it must fit between the item and the border of the application window,
        //  otherwise Dear ImGui moves it, and the tests could not predict its position)
        const ImRect target(gScene.CanvasRect.Min + ImVec2(em, em), gScene.CanvasRect.Min + gScene.CanvasRect.GetSize() * ImVec2(0.35f, 0.5f));
        ImVec2 delta(0.0f, 0.0f);
        if (item.Min.x < target.Min.x)      delta.x = target.Min.x - item.Min.x;
        else if (item.Max.x > target.Max.x) delta.x = ImMin(target.Max.x - item.Max.x, 0.0f) + 0.0f;
        if (item.Min.y < target.Min.y)      delta.y = target.Min.y - item.Min.y;
        else if (item.Max.y > target.Max.y) delta.y = target.Max.y - item.Max.y;
        if (item.GetWidth() > target.GetWidth())   delta.x = target.Min.x - item.Min.x;   // too large: align its top left
        if (item.GetHeight() > target.GetHeight()) delta.y = target.Min.y - item.Min.y;
        if (ImAbs(delta.x) < 1.0f && ImAbs(delta.y) < 1.0f)
            return;
        const ImVec2 max_delta = gScene.CanvasRect.GetSize() * 0.7f;
        PanView(ctx, ImVec2(ImClamp(delta.x, -max_delta.x, max_delta.x), ImClamp(delta.y, -max_delta.y, max_delta.y)));
    }
}

// A spot of the canvas that is not covered by a node, with room on its right for a popup
static bool FindEmptySpot(ImVec2* out_pos)
{
    const float em = ImGui::GetFontSize();
    for (float y = gScene.CanvasRect.Max.y - em * 2.0f; y > gScene.CanvasRect.Min.y; y -= em * 2.0f)
        for (float x = gScene.CanvasRect.Max.x - em * 14.0f; x > gScene.CanvasRect.Min.x; x -= em * 2.0f)
        {
            bool is_free = true;
            for (const ImRect& node_rect : gScene.NodeRects)
            {
                ImRect r = node_rect;
                r.Expand(em);
                if (r.Contains(ImVec2(x, y)))
                    is_free = false;
            }
            if (is_free)
            {
                *out_pos = ImVec2(x, y);
                return true;
            }
        }
    return false;
}

static ImVec2 NodeItemCenter(const char* name)
{
    return gScene.ItemRects[name].GetCenter();
}

static void NodeItemClick(ImGuiTestContext* ctx, const char* name, ImGuiMouseButton button = 0)
{
    BringIntoView(ctx, name);
    ctx->MouseMoveToPos(NodeItemCenter(name));
    ctx->MouseClick(button);
}

// Screenshot of the whole application, written to node_editor_tests_captures/<name>.png (relative to the working directory)
static void CaptureApp(ImGuiTestContext* ctx, const char* test_name, const char* view_name)
{
    ctx->Yield();
    ctx->CaptureReset();
    ImFormatString(ctx->CaptureArgs->InOutputFile, IM_ARRAYSIZE(ctx->CaptureArgs->InOutputFile), "node_editor_tests_captures/%s_%s.png", test_name, view_name);
    ctx->CaptureScreenshot(ImGuiCaptureFlags_HideMouseCursor);
}

// A popup must open inside the viewport, with its top left corner close to the expected screen position
static void CheckPopupPos(ImGuiTestContext* ctx, ImGuiWindow* popup, ImVec2 expected_pos, float tolerance)
{
    IM_CHECK(popup != nullptr);
    ctx->LogInfo("zoom=%.2f popup pos=(%.0f,%.0f) expected=(%.0f,%.0f)", gScene.Zoom, popup->Pos.x, popup->Pos.y, expected_pos.x, expected_pos.y);
    IM_CHECK_LT(ImAbs(popup->Pos.x - expected_pos.x), tolerance);
    IM_CHECK_LT(ImAbs(popup->Pos.y - expected_pos.y), tolerance);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    IM_CHECK(ImRect(viewport->Pos, viewport->Pos + viewport->Size).Contains(popup->Rect()));
}

// Checks on what the editor added to the draw list, over all the frames since the counters were reset:
// - no canvas marker was left behind (the renderer would call it as a function)
// - the clip rect of each draw command is inside the window (a clip rect converted twice to screen space lands outside)
// - the visible part of each node is inside the clip rect of at least one draw command (nothing was clipped away)
// - the editor leaves no empty draw command behind
// - the editor draws something inside the canvas
static void CheckDrawList(ImGuiTestContext* ctx)
{
    ctx->Yield();
    IM_CHECK_EQ(gScene.SentinelCmdCount, 0);
    if (gScene.BadClipRectCount > 0)
        ctx->LogError("first bad clip rect: (%.0f,%.0f,%.0f,%.0f)", gScene.FirstBadClipRect.Min.x, gScene.FirstBadClipRect.Min.y, gScene.FirstBadClipRect.Max.x, gScene.FirstBadClipRect.Max.y);
    IM_CHECK_EQ(gScene.BadClipRectCount, 0);
    IM_CHECK_EQ(gScene.UncoveredNodeCount, 0);
    IM_CHECK_EQ(gScene.EmptyCmdCount, 0);
    IM_CHECK_GT(gScene.VisibleCmdCount, 0);
}

// Run a check at zoom 1 / 0.5 / 2, then once more after a pan. The draw list is checked over all the frames of the run.
// Changing the zoom is what takes time (it is animated): the zooms are visited starting from the current one,
// so that a test starts where the previous one ended.
typedef void (*ViewCheck)(ImGuiTestContext* ctx, const char* view_name);
static void RunInAllViews(ImGuiTestContext* ctx, ViewCheck check)
{
    const float em = ImGui::GetFontSize();
    ctx->WindowFocus("//Node editor tests");
    ctx->Yield(2);
    gScene.SentinelCmdCount = 0;
    gScene.BadClipRectCount = 0;
    gScene.UncoveredNodeCount = 0;

    const float zooms_up[]   = { 0.5f, 1.0f, 2.0f };
    const float zooms_down[] = { 2.0f, 1.0f, 0.5f };
    const float* zooms = (gScene.Zoom > 1.5f) ? zooms_down : zooms_up;
    for (int i = 0; i < 3; i++)
    {
        SetZoom(ctx, zooms[i]);
        if (ctx->IsError())
            return;
        check(ctx, Str16f("zoom_%.1f", zooms[i]).c_str());
        ctx->PopupCloseAll();
        CheckDrawList(ctx);
        if (ctx->IsError())
            return;
    }

    const ImVec2 anchor_before_pan = gScene.ZoomAnchor;
    PanView(ctx, ImVec2(em * 6.0f, em * 4.0f));
    IM_CHECK_LT(ImAbs(gScene.ZoomAnchor.x - anchor_before_pan.x - em * 6.0f), 2.0f);
    IM_CHECK_LT(ImAbs(gScene.ZoomAnchor.y - anchor_before_pan.y - em * 4.0f), 2.0f);
    check(ctx, "panned");
    ctx->PopupCloseAll();
    CheckDrawList(ctx);
}

//------------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------------
void NodeEditorTests_Register(ImGuiTestEngine* engine)
{
    ImGuiTest* t = nullptr;

    // ## Combo inside a node: the popup opens right below the combo, an item can be picked
    t = IM_REGISTER_TEST(engine, "node_editor", "combo");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            gScene.ComboIdx = 0;
            NodeItemClick(ctx, "combo");
            const ImRect combo_rect = gScene.ItemRects["combo"];
            CheckPopupPos(ctx, TopPopupWindow(), combo_rect.GetBL(), ImGui::GetFontSize());
            CaptureApp(ctx, "combo", view_name);
            ctx->ItemClick("//##Combo_00/CCCC");
            IM_CHECK_EQ(gScene.ComboIdx, 2);
        });
    };

    // ## Same combo, when the node is at negative canvas coordinates (positions inside the canvas are then negative numbers,
    // which the popup placement code of Dear ImGui could mistake for positions outside of the screen)
    t = IM_REGISTER_TEST(engine, "node_editor", "combo_negative_coords");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        const float em = ImGui::GetFontSize();
        gScene.MoveWidgetsNodePos = ImVec2(-em * 60.0f, -em * 40.0f);
        gScene.MoveWidgetsNodeRequest = true;
        ctx->Yield(2);
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            gScene.ComboIdx = 0;
            NodeItemClick(ctx, "combo");
            CheckPopupPos(ctx, TopPopupWindow(), gScene.ItemRects["combo"].GetBL(), ImGui::GetFontSize());
            CaptureApp(ctx, "combo_negative_coords", view_name);
            ctx->ItemClick("//##Combo_00/CCCC");
            IM_CHECK_EQ(gScene.ComboIdx, 2);
        });
        gScene.MoveWidgetsNodePos = ImVec2(em * 2.0f, em * 2.0f);
        gScene.MoveWidgetsNodeRequest = true;
        ctx->Yield(2);
    };

    // ## Same combo at zoom 2, low in the window: its popup fits below it, but only just.
    // (Dear ImGui decides whether the popup fits by comparing a position, which is in canvas space here, with a size in pixels:
    //  at zoom 2 it believes that the popup is twice as big as it is, and would open it above the combo, with a gap)
    t = IM_REGISTER_TEST(engine, "node_editor", "combo_low_in_window");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->WindowFocus("//Node editor tests");
        SetZoom(ctx, 2.0f);
        BringIntoView(ctx, "combo");
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float popup_height = ImGui::GetFontSize() * 14.0f;   // more than the height of a combo popup (8 items)
        for (int i = 0; i < 4; i++)
        {
            const float wanted_y = viewport->Pos.y + viewport->Size.y - popup_height;
            const float delta_y = wanted_y - gScene.ItemRects["combo"].Max.y;
            if (ImAbs(delta_y) < 2.0f)
                break;
            PanView(ctx, ImVec2(0.0f, ImClamp(delta_y, -gScene.CanvasRect.GetHeight() * 0.7f, gScene.CanvasRect.GetHeight() * 0.7f)));
        }
        IM_CHECK(gScene.CanvasRect.Contains(gScene.ItemRects["combo"]));

        gScene.ComboIdx = 0;
        ctx->MouseMoveToPos(NodeItemCenter("combo"));
        ctx->MouseClick(0);
        CheckPopupPos(ctx, TopPopupWindow(), gScene.ItemRects["combo"].GetBL(), ImGui::GetFontSize());
        CaptureApp(ctx, "combo_low_in_window", "zoom_2.0");
        ctx->ItemClick("//##Combo_00/CCCC");
        IM_CHECK_EQ(gScene.ComboIdx, 2);
    };

    // ## ColorEdit4 inside a node: the picker opens below the color square, the color can be changed
    t = IM_REGISTER_TEST(engine, "node_editor", "color_edit");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            const ImVec4 initial_color(0.2f, 0.4f, 0.8f, 1.0f);
            gScene.Color = initial_color;
            BringIntoView(ctx, "color");
            const ImRect color_rect = gScene.ItemRects["color"];   // the color square, followed by the label
            ctx->MouseMoveToPos(ImVec2(color_rect.Min.x + color_rect.GetHeight() * 0.5f, color_rect.GetCenter().y));
            ctx->MouseClick(0);
            ImGuiWindow* popup = TopPopupWindow();
            CheckPopupPos(ctx, popup, color_rect.GetBL(), ImGui::GetFontSize() * 1.5f);
            CaptureApp(ctx, "color_edit", view_name);
            if (popup == nullptr)
                return;
            ctx->SetRef(popup);
            ctx->ItemClick("##picker/sv");
            IM_CHECK(memcmp(&gScene.Color, &initial_color, sizeof(ImVec4)) != 0);
        });
    };

    // ## InputTextMultiline inside a node: a click opens a popup with the editor, the text can be edited
    t = IM_REGISTER_TEST(engine, "node_editor", "input_text_multiline");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            ImStrncpy(gScene.Text, "Line 1\nLine 2\nLine 3", IM_ARRAYSIZE(gScene.Text));
            NodeItemClick(ctx, "text");
            ImGuiWindow* popup = TopPopupWindow();
            CheckPopupPos(ctx, popup, NodeItemCenter("text"), ImGui::GetFontSize());
            if (popup == nullptr)
                return;
            ctx->SetRef(popup);
            ctx->ItemClick("##edit");
            ctx->KeyCharsReplace("typed in the popup");
            CaptureApp(ctx, "input_text_multiline", view_name);
            ctx->PopupCloseAll();
            IM_CHECK_STR_EQ(gScene.Text, "typed in the popup");
        });
    };

    // ## Popup opened from a node without Suspend / Resume
    t = IM_REGISTER_TEST(engine, "node_editor", "popup");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            gScene.PlainPopupClicks = 0;
            NodeItemClick(ctx, "open_popup");
            ImGuiWindow* popup = TopPopupWindow();
            CheckPopupPos(ctx, popup, NodeItemCenter("open_popup"), ImGui::GetFontSize());
            CaptureApp(ctx, "popup", view_name);
            if (popup == nullptr)
                return;
            ctx->SetRef(popup);
            ctx->ItemClick("popup button");
            IM_CHECK_EQ(gScene.PlainPopupClicks, 1);
        });
    };

    // ## Popup opened from a node with Suspend / Resume (user code written for the upstream node editor)
    t = IM_REGISTER_TEST(engine, "node_editor", "popup_legacy");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            gScene.LegacyPopupClicks = 0;
            NodeItemClick(ctx, "open_legacy_popup");
            ImGuiWindow* popup = TopPopupWindow();
            CheckPopupPos(ctx, popup, NodeItemCenter("open_legacy_popup"), ImGui::GetFontSize());
            CaptureApp(ctx, "popup_legacy", view_name);
            if (popup == nullptr)
                return;
            ctx->SetRef(popup);
            ctx->ItemClick("popup button");
            IM_CHECK_EQ(gScene.LegacyPopupClicks, 1);
        });
    };

    // ## Tooltip of an item inside a node: it shows up next to the mouse
    t = IM_REGISTER_TEST(engine, "node_editor", "tooltip");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            BringIntoView(ctx, "tooltip_target");
            ctx->MouseMoveToPos(NodeItemCenter("tooltip_target"));
            ctx->Yield(3);
            ImGuiWindow* tooltip = ctx->GetWindowByRef("//##Tooltip_00");
            IM_CHECK(tooltip != nullptr);
            IM_CHECK(tooltip->Active && !tooltip->Hidden);
            const ImVec2 mouse_pos = NodeItemCenter("tooltip_target");
            ctx->LogInfo("zoom=%.2f tooltip pos=(%.0f,%.0f) mouse=(%.0f,%.0f)", gScene.Zoom, tooltip->Pos.x, tooltip->Pos.y, mouse_pos.x, mouse_pos.y);
            IM_CHECK_LT(ImAbs(tooltip->Pos.x - mouse_pos.x), ImGui::GetFontSize() * 3.0f);
            IM_CHECK_LT(ImAbs(tooltip->Pos.y - mouse_pos.y), ImGui::GetFontSize() * 3.0f);
            CaptureApp(ctx, "tooltip", view_name);
        });
    };

    // ## Widgets that span the available width use the width of the node, and the node does not grow
    t = IM_REGISTER_TEST(engine, "node_editor", "node_width");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            BringIntoView(ctx, "width_title");
            const ImRect node_rect = gScene.NodeRects[2];
            const char* names[] = { "width_separator_text", "width_separator", "width_header", "width_text" };
            for (const char* name : names)
            {
                IM_CHECK(gScene.ItemRects.count(name) == 1);
                const ImRect item_rect = gScene.ItemRects[name];
                ctx->LogInfo("zoom=%.2f %s: x=%.0f..%.0f, node: x=%.0f..%.0f", gScene.Zoom, name, item_rect.Min.x, item_rect.Max.x, node_rect.Min.x, node_rect.Max.x);
                IM_CHECK_GE(item_rect.Min.x, node_rect.Min.x - 1.0f);
                IM_CHECK_LE(item_rect.Max.x, node_rect.Max.x + 1.0f);
                // Separators and headers span the node. Framed widgets are narrower: the editor keeps room for their label.
                const bool is_framed_widget = (strcmp(name, "width_text") == 0);
                IM_CHECK_GT(item_rect.GetWidth(), node_rect.GetWidth() * (is_framed_widget ? 0.5f : 0.8f));
            }
            ctx->Yield(10);
            IM_CHECK_LT(ImAbs(gScene.NodeRects[2].GetWidth() - node_rect.GetWidth()), 1.0f);
            IM_CHECK_LT(ImAbs(gScene.NodeRects[2].GetHeight() - node_rect.GetHeight()), 1.0f);
            CaptureApp(ctx, "node_width", view_name);
        });
    };

    // ## Combo inside a horizontal layout, after a spring
    t = IM_REGISTER_TEST(engine, "node_editor", "layout_combo");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            gScene.LayoutComboIdx = 0;
            BringIntoView(ctx, "layout_title");
            BringIntoView(ctx, "layout_combo");
            CaptureApp(ctx, "layout_combo_closed", view_name);
            const ImRect left_rect_before = gScene.ItemRects["layout_left"];
            NodeItemClick(ctx, "layout_combo");
            CheckPopupPos(ctx, TopPopupWindow(), gScene.ItemRects["layout_combo"].GetBL(), ImGui::GetFontSize());
            CaptureApp(ctx, "layout_combo", view_name);
            // The items of the layout do not move while the popup is open
            IM_CHECK_LT(ImAbs(gScene.ItemRects["layout_left"].Min.x - left_rect_before.Min.x), 1.0f);
            IM_CHECK_LT(ImAbs(gScene.ItemRects["layout_left"].Min.y - left_rect_before.Min.y), 1.0f);
            ctx->ItemClick("//##Combo_00/CCCC");
            IM_CHECK_EQ(gScene.LayoutComboIdx, 2);
        });
    };

    // ## Style::AngledLinks: a link that would pass through its nodes is routed around them, or drawn as a single curve
    t = IM_REGISTER_TEST(engine, "node_editor", "angled_links");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->WindowFocus("//Node editor tests");
        SetZoom(ctx, 1.0f);
        BringIntoView(ctx, "link_title");
        ed::SetCurrentEditor(gScene.Editor);
        ed::Style& style = ed::GetStyle();
        ed::SetCurrentEditor(nullptr);

        style.AngledLinks = true;
        ctx->Yield(2);
        const int vtx_count_angled = gScene.VtxCount;
        CaptureApp(ctx, "angled_links", "on");

        style.AngledLinks = false;
        ctx->Yield(2);
        const int vtx_count_curve = gScene.VtxCount;
        CaptureApp(ctx, "angled_links", "off");

        style.AngledLinks = true;
        // The two paths are different: they do not produce the same geometry
        IM_CHECK_NE(vtx_count_angled, vtx_count_curve);
    };

    // ## Color editor inside a pin: the tooltip of the color button shows up next to the mouse, the picker opens below the button
    t = IM_REGISTER_TEST(engine, "node_editor", "color_in_pin");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            const ImVec4 initial_color(0.8f, 0.3f, 0.2f, 1.0f);
            gScene.PinColor = initial_color;
            BringIntoView(ctx, "pin_color");
            const ImRect color_rect = gScene.ItemRects["pin_color"];   // the color button, followed by the label
            const ImVec2 button_center(color_rect.Min.x + color_rect.GetHeight() * 0.5f, color_rect.GetCenter().y);

            ctx->MouseMoveToPos(button_center);
            ctx->SleepNoSkip(1.0f, 1.0f / 30.0f);   // the tooltip of a color button appears after a delay
            ImGuiWindow* tooltip = ctx->GetWindowByRef("//##Tooltip_00");
            IM_CHECK(tooltip != nullptr);
            IM_CHECK(tooltip->Active && !tooltip->Hidden);
            ctx->LogInfo("zoom=%.2f tooltip pos=(%.0f,%.0f) mouse=(%.0f,%.0f)", gScene.Zoom, tooltip->Pos.x, tooltip->Pos.y, button_center.x, button_center.y);
            IM_CHECK_LT(ImAbs(tooltip->Pos.x - button_center.x), ImGui::GetFontSize() * 3.0f);
            IM_CHECK_LT(ImAbs(tooltip->Pos.y - button_center.y), ImGui::GetFontSize() * 3.0f);
            CaptureApp(ctx, "color_in_pin_tooltip", view_name);

            ctx->MouseClick(0);
            ImGuiWindow* popup = TopPopupWindow();
            CheckPopupPos(ctx, popup, color_rect.GetBL(), ImGui::GetFontSize() * 1.5f);
            CaptureApp(ctx, "color_in_pin_picker", view_name);
            if (popup == nullptr)
                return;
            ctx->SetRef(popup);
            ctx->ItemClick("##picker/sv");
            IM_CHECK(memcmp(&gScene.PinColor, &initial_color, sizeof(ImVec4)) != 0);
        });
    };

    // ## Popup opened from a node whose content uses draw channels, while a channel other than the first one is current
    t = IM_REGISTER_TEST(engine, "node_editor", "draw_channels");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            gScene.ChannelsPopupClicks = 0;
            NodeItemClick(ctx, "channels_open_popup");
            ImGuiWindow* popup = TopPopupWindow();
            CheckPopupPos(ctx, popup, NodeItemCenter("channels_open_popup"), ImGui::GetFontSize());
            CaptureApp(ctx, "draw_channels", view_name);
            if (popup == nullptr)
                return;
            ctx->SetRef(popup);
            ctx->ItemClick("popup button");
            IM_CHECK_EQ(gScene.ChannelsPopupClicks, 1);
        });
    };

    // ## Two overlapping nodes: a click goes to the button of the node in front, not to the button hidden behind it
    t = IM_REGISTER_TEST(engine, "node_editor", "overlapping_nodes");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            gScene.BackNodeClicks = 0;
            gScene.FrontNodeClicks = 0;
            NodeItemClick(ctx, "front_button");
            ctx->Yield(2);
            CaptureApp(ctx, "overlapping_nodes", view_name);
            IM_CHECK_EQ(gScene.FrontNodeClicks, 1);
            IM_CHECK_EQ(gScene.BackNodeClicks, 0);
        });
    };

    // ## Same checks when the window of the editor is docked (clip rects and popups went wrong in docked windows)
# ifdef IMGUI_HAS_DOCK
    t = IM_REGISTER_TEST(engine, "node_editor", "docked");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) == 0)
        {
            ctx->LogWarning("Docking is not enabled: test skipped");
            return;
        }
        ctx->DockClear("Node editor tests", "Node editor tests (dock target)", NULL);
        ctx->DockInto("//Node editor tests (dock target)", "//Node editor tests");
        ctx->WindowFocus("//Node editor tests");
        ctx->Yield(2);
        ImGuiWindow* window = ctx->GetWindowByRef("//Node editor tests");
        IM_CHECK(window != nullptr);
        IM_CHECK(window->DockIsActive);

        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            gScene.ComboIdx = 0;
            NodeItemClick(ctx, "combo");
            CheckPopupPos(ctx, TopPopupWindow(), gScene.ItemRects["combo"].GetBL(), ImGui::GetFontSize());
            CaptureApp(ctx, "docked_combo", view_name);
            ctx->ItemClick("//##Combo_00/CCCC");
            IM_CHECK_EQ(gScene.ComboIdx, 2);

            gScene.PlainPopupClicks = 0;
            NodeItemClick(ctx, "open_popup");
            ImGuiWindow* popup = TopPopupWindow();
            CheckPopupPos(ctx, popup, NodeItemCenter("open_popup"), ImGui::GetFontSize());
            CaptureApp(ctx, "docked_popup", view_name);
            if (popup == nullptr)
                return;
            ctx->SetRef(popup);
            ctx->ItemClick("popup button");
            IM_CHECK_EQ(gScene.PlainPopupClicks, 1);
        });

        ctx->DockClear("Node editor tests", "Node editor tests (dock target)", NULL);
    };
# endif

    // ## Context menus on a node and on the background (documented pattern, with Suspend / Resume)
    t = IM_REGISTER_TEST(engine, "node_editor", "context_menus");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        RunInAllViews(ctx, [](ImGuiTestContext* ctx, const char* view_name)
        {
            gScene.LastMenuItem = "";
            NodeItemClick(ctx, "widgets_title", 1);
            ImGuiWindow* popup = TopPopupWindow();
            CheckPopupPos(ctx, popup, NodeItemCenter("widgets_title"), ImGui::GetFontSize());
            CaptureApp(ctx, "context_menu_node", view_name);
            if (popup == nullptr)
                return;
            IM_CHECK_EQ(gScene.ContextNodeId.Get(), (size_t)1);
            ctx->SetRef(popup);
            ctx->ItemClick("node item");
            IM_CHECK_STR_EQ(gScene.LastMenuItem.c_str(), "node item");

            ImVec2 background_pos;
            IM_CHECK(FindEmptySpot(&background_pos));
            ctx->MouseMoveToPos(background_pos);
            ctx->MouseClick(1);
            popup = TopPopupWindow();
            CheckPopupPos(ctx, popup, background_pos, ImGui::GetFontSize());
            CaptureApp(ctx, "context_menu_background", view_name);
            if (popup == nullptr)
                return;
            ctx->SetRef(popup);
            ctx->ItemClick("background item");
            IM_CHECK_STR_EQ(gScene.LastMenuItem.c_str(), "background item");
        });
    };
}
