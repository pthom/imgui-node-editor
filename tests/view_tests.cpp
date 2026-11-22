//------------------------------------------------------------------------------
// The view: where navigation takes it, and the grid drawn under it.
//------------------------------------------------------------------------------
# include "harness.h"
# include <cmath>

namespace {

// Vertices in the color of the grid at zoom 1.
int GridVertices(const ImDrawList* drawList)
{
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(ed::GetStyle().Colors[ed::StyleColor_Grid]);
    int count = 0;
    for (auto& vertex : drawList->VtxBuffer)
        if (vertex.col == color)
            ++count;
    return count;
}

void OneNode(int)
{
    ed::BeginNode(1);
    ImGui::TextUnformatted("Node");
    ed::EndNode();
}

// Without smooth zoom: one wheel step goes to the next zoom level.
ed::Config SteppedZoomConfig(std::initializer_list<float> levels)
{
    ed::Config config;
    config.SettingsFile     = nullptr;
    config.EnableSmoothZoom = false;
    for (auto level : levels)
        config.CustomZoomLevels.push_back(level);
    return config;
}

// A session with the mouse over its editor.
void HoverEditor(test::Session& session)
{
    session.DefaultContent = OneNode;
    test::MouseMove(ImVec2(500.0f, 300.0f));
    session.Run(10);
}

// One wheel event of 'delta' over the editor, and time for its animation; returns the zoom (2 at 200%).
float WheelStep(test::Session& session, float delta)
{
    test::MouseWheel(delta);
    session.Run(30);
    return session.Use([] { return test::CurrentZoom(); });
}

} // namespace

TEST_CASE(GridAtTinyZoom, "view.grid_at_tiny_zoom_is_bounded", "CORE-10", test::TestFlags_None)
{
    auto content = [](int)
    {
        ed::BeginNode(1);
        ImGui::TextUnformatted("Node");
        ed::EndNode();
    };

    {
        test::Storage storage;
        test::Session session(&storage);
        session.Frame(content);
        session.Frame(content);
        const int gridVertices = session.Use([&] { return GridVertices(session.DrawList); });
        test::Check(gridVertices > 0, "grid drawn at zoom 1: %d vertices", gridVertices);
    }

    // Zoom levels down to 1e-6, where the view is 10^9 units across.
    test::Storage storage;
    storage.Global = R"({"view":{"scroll":{"x":0,"y":0},"zoom":0.000001}})";
    auto config = test::MakeConfig(&storage);
    config.CustomZoomLevels.push_back(1e-6f);
    config.CustomZoomLevels.push_back(1.0f);
    test::Session session(config);
    session.Settings = &storage;
    for (int i = 0; i < 3; ++i)
        session.Frame(content);
    const float zoom = session.Use([] { return test::CurrentZoom(); });
    test::Check(zoom < 1e-5f, "view zoomed out to %g", zoom);
}

TEST_CASE(ZoomLevelsChangedAfterCreation, "view.zoom_levels_changed_after_creation", "CORE-19", test::TestFlags_None)
{
    // A binding may hand the editor's own Config to the application, which can then change its zoom levels.
    test::Session session(SteppedZoomConfig({ 1.0f, 2.0f }));
    HoverEditor(session);
    session.Use([]
    {
        ImVector<float> levels; // allocated while the old levels live: elsewhere
        for (float level : { 0.5f, 1.0f, 4.0f })
            levels.push_back(level);
        const_cast<ed::Config&>(ed::GetConfig()).CustomZoomLevels.swap(levels);
    }); // the old levels are freed here
    const float zoom = WheelStep(session, 1.0f);
    test::Check(std::fabs(zoom - 4.0f) < 0.01f, "one wheel step goes to the new level 4: %g", zoom);
}
