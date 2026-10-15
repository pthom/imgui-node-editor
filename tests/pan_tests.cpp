//------------------------------------------------------------------------------
// Where a pan with the navigate button may start: the background, a node, a
// widget of a node which does not use that button. Not a widget which uses it,
// nor a press outside the editor (#238).
//------------------------------------------------------------------------------
# include "harness.h"

namespace {

struct PanScene
{
    ImVec2 ButtonCenter, RightButtonCenter, OriginOnScreen;
    bool   RightButtonHeld = false;

    void operator()(int frame)
    {
        if (frame == 0)
            ed::SetNodePosition(1, ImVec2(100.0f, 100.0f));
        ed::BeginNode(1);
        ImGui::Text("Node");
        ImGui::Button("A button", ImVec2(150.0f, 40.0f));
        ButtonCenter = ed::CanvasToScreen((ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) * 0.5f);
        ImGui::InvisibleButton("uses the navigate button", ImVec2(150.0f, 40.0f), ImGuiButtonFlags_MouseButtonRight);
        RightButtonCenter = ed::CanvasToScreen((ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) * 0.5f);
        RightButtonHeld = RightButtonHeld || ImGui::IsItemActive();
        ed::EndNode();
        OriginOnScreen = ed::CanvasToScreen(ImVec2(0.0f, 0.0f));
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
