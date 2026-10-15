//------------------------------------------------------------------------------
// Where a pan with the navigate button may start: the background, a node, a
// widget of a node which does not use that button. Not a widget which uses it,
// nor a press outside the editor (#238). A click stays a click; the release that
// ends a pan is not one.
//------------------------------------------------------------------------------
# include "harness.h"

namespace {

struct PanScene
{
    ImVec2 ButtonCenter, RightButtonCenter, OriginOnScreen;
    bool   RightButtonHeld = false;
    int    ButtonContextMenus = 0;  // the condition of BeginPopupContextItem() on the button
    int    NodeContextMenus = 0;    // ed::ShowNodeContextMenu() for the node

    void operator()(int frame)
    {
        if (frame == 0)
            ed::SetNodePosition(1, ImVec2(100.0f, 100.0f));
        ed::BeginNode(1);
        ImGui::Text("Node");
        ImGui::Button("A button", ImVec2(150.0f, 40.0f));
        ButtonCenter = ed::CanvasToScreen((ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) * 0.5f);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
            ++ButtonContextMenus;
        ImGui::InvisibleButton("uses the navigate button", ImVec2(150.0f, 40.0f), ImGuiButtonFlags_MouseButtonRight);
        RightButtonCenter = ed::CanvasToScreen((ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) * 0.5f);
        RightButtonHeld = RightButtonHeld || ImGui::IsItemActive();
        ed::EndNode();
        OriginOnScreen = ed::CanvasToScreen(ImVec2(0.0f, 0.0f));
        ed::Suspend();
        ed::NodeId nodeId;
        if (ed::ShowNodeContextMenu(&nodeId) && nodeId == ed::NodeId(1))
            ++NodeContextMenus;
        ed::Resume();
    }
};

// Drags with the navigate button from 'from', and returns how far the view moved.
ImVec2 DragAndMeasure(test::Session& session, PanScene& scene, ImVec2 from, ImVec2 delta)
{
    const ImVec2 before = scene.OriginOnScreen;
    session.Drag(from, from + delta, ed::Config().NavigateButtonIndex);
    return scene.OriginOnScreen - before;
}

bool Equal(ImVec2 a, ImVec2 b) { return ImFabs(a.x - b.x) < 1.0f && ImFabs(a.y - b.y) < 1.0f; }

} // namespace

TEST_CASE(PanFromBackground, "interaction.pan_from_the_background", "#238", test::TestFlags_None)
{
    test::Storage storage;
    test::Session session(&storage);
    PanScene scene;
    session.DefaultContent = std::ref(scene);
    session.Run(3);

    const ImVec2 delta(80.0f, 50.0f);
    const ImVec2 moved = DragAndMeasure(session, scene, session.HostPos + ImVec2(700.0f, 500.0f), delta);
    test::Check(Equal(moved, delta), "a drag from the background pans the view: moved by (%g, %g), the mouse by (%g, %g)", moved.x, moved.y, delta.x, delta.y);
}

TEST_CASE(PanFromWidget, "interaction.pan_from_a_widget_in_a_node", "#238", test::TestFlags_None)
{
    test::Storage storage;
    test::Session session(&storage);
    PanScene scene;
    session.DefaultContent = std::ref(scene);
    session.Run(3);

    const ImVec2 delta(80.0f, 50.0f);
    const ImVec2 moved = DragAndMeasure(session, scene, scene.ButtonCenter, delta);
    test::Check(Equal(moved, delta), "a drag from a button of a node (which uses another mouse button) pans the view: moved by (%g, %g), the mouse by (%g, %g)", moved.x, moved.y, delta.x, delta.y);
}

TEST_CASE(NoPanFromWidgetUsingTheButton, "interaction.no_pan_from_a_widget_using_the_navigate_button", "#238", test::TestFlags_None)
{
    test::Storage storage;
    test::Session session(&storage);
    PanScene scene;
    session.DefaultContent = std::ref(scene);
    session.Run(3);

    const ImVec2 moved = DragAndMeasure(session, scene, scene.RightButtonCenter, ImVec2(80.0f, 50.0f));
    test::Check(Equal(moved, ImVec2(0.0f, 0.0f)), "a drag from a widget which uses the navigate button does not pan: moved by (%g, %g)", moved.x, moved.y);
    test::Check(scene.RightButtonHeld, "the widget keeps the navigate button: it was held");
}

TEST_CASE(NoPanFromOutside, "interaction.no_pan_from_outside_the_editor", "#238", test::TestFlags_None)
{
    test::Storage storage;
    test::Session session(&storage);
    PanScene scene;
    session.DefaultContent = std::ref(scene);
    session.Run(3);

    // Pressed right of the host window, where no window is, then dragged over the editor.
    const ImVec2 from = session.HostPos + ImVec2(session.HostSize.x + 100.0f, 300.0f);
    const ImVec2 moved = DragAndMeasure(session, scene, from, ImVec2(-500.0f, 0.0f));
    test::Check(Equal(moved, ImVec2(0.0f, 0.0f)), "a drag pressed outside the editor does not pan when it enters it: moved by (%g, %g)", moved.x, moved.y);
}

TEST_CASE(RightClickOnWidget, "interaction.right_click_on_a_widget_in_a_node", "#238", test::TestFlags_None)
{
    test::Storage storage;
    test::Session session(&storage);
    PanScene scene;
    session.DefaultContent = std::ref(scene);
    session.Run(3);

    session.Click(scene.ButtonCenter, ed::Config().ContextMenuButtonIndex);
    test::Check(scene.ButtonContextMenus == 1, "a right click on a button of a node opens its context menu: %d times", scene.ButtonContextMenus);
    test::Check(scene.NodeContextMenus == 1, "and the context menu of the node, as before: %d times", scene.NodeContextMenus);
}

TEST_CASE(NoClickAtTheEndOfAPan, "interaction.no_click_at_the_end_of_a_pan", "#238", test::TestFlags_None)
{
    test::Storage storage;
    test::Session session(&storage);
    PanScene scene;
    session.DefaultContent = std::ref(scene);
    session.Run(3);

    // The view follows the mouse: the drag ends on the button it started from
    const ImVec2 delta(80.0f, 50.0f);
    const ImVec2 moved = DragAndMeasure(session, scene, scene.ButtonCenter, delta);
    test::Check(Equal(moved, delta), "the drag pans the view: moved by (%g, %g)", moved.x, moved.y);
    test::Check(scene.ButtonContextMenus == 0, "the release that ends the pan opens no context menu of the button: %d times", scene.ButtonContextMenus);
    test::Check(scene.NodeContextMenus == 0, "nor of the node: %d times", scene.NodeContextMenus);
}
