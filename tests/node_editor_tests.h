// Automated tests for widgets placed inside nodes (popups, combos, color pickers, multiline text...)
//
// These files are runner-agnostic: they depend only on Dear ImGui, imgui_test_engine and imgui-node-editor.
// To plug them into an application that already runs the test engine:
//     - compile tests/node_editor_tests.cpp with your application
//     - call NodeEditorTests_Register(engine) once, after the test engine was created
//     - call NodeEditorTests_ShowGui() once per frame (it opens its own window, "Node editor tests")
//     - call NodeEditorTests_Shutdown() before destroying the ImGui context
// The tests can then be launched from the test engine window, or queued with
//     ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, "node_editor");
// Two tests need multi-viewports (ImGuiConfigFlags_ViewportsEnable) and are skipped without them. With multi-viewports, clear
// ImGuiBackendFlags_HasMouseHoveredViewport at each frame: the backend reports the OS window under the REAL mouse, while the
// test engine simulates the mouse.
# pragma once

struct ImGuiTestEngine;

void NodeEditorTests_ShowGui();
void NodeEditorTests_Register(ImGuiTestEngine* engine);
void NodeEditorTests_Shutdown();
