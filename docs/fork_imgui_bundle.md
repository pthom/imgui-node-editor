# What this fork adds to imgui-node-editor

This is the fork of [thedmd/imgui-node-editor](https://github.com/thedmd/imgui-node-editor) used by
[Dear ImGui Bundle](https://github.com/pthom/imgui_bundle) (branch `imgui_bundle`, based on upstream `develop`).
It is usable without Dear ImGui Bundle: it builds against a stock Dear ImGui, and the features that need a patched
Dear ImGui switch themselves on when the patch is present.

The history is organised by theme, and the commits are meant to be cherry-picked. The public API of upstream is unchanged:
the fork only adds to it.

The branch is rebased on upstream when upstream moves, so its history is rewritten from time to time: if you depend on it, pin a
dated tag (`bundle_YYYYMMDD`) or a commit rather than tracking the branch.

1. [Summary of the changes](#1-summary-of-the-changes)
2. [Widgets inside nodes: what works](#2-widgets-inside-nodes-what-works)
3. [Patching Dear ImGui (optional)](#3-patching-dear-imgui-optional)
4. [Added API](#4-added-api)
5. [Running the tests](#5-running-the-tests)


## 1. Summary of the changes

In the order of the commits:

| Theme | What |
|---|---|
| Fixes | Drags that started outside of the editor window are ignored. A node covered by another node does not steal the hover / click of the node in front of it. Compatibility with Dear ImGui 1.92.6 to 1.92.8 |
| Style | `StyleVar_GridSize`: size of the background grid cells |
| Documentation | API reference comments in `imgui_node_editor.h`; keyboard and mouse interactions in the [README](README.md) |
| Canvas: clip rects | Clip rects are transformed once, after the draw channels are merged. Fixes a crash in the renderer after `Suspend()` / `Resume()` (upstream issue 282), and geometry that disappeared in docked windows, or when a popup was opened while the view was panned or zoomed |
| Canvas: popups | With the Dear ImGui hooks of chapter 3, popups, combos, color pickers, tooltips and context menus opened from inside the editor are placed correctly, without `Suspend()` / `Resume()` |
| Canvas | `ImGuiEx::IsInsideCanvas()` |
| Multiline text | `ed::InputTextMultiline()`: a multiline text field that works inside a node |
| Child windows | A child window begun inside the editor is reported with an explanation, instead of silently misbehaving |
| Node width | `Config::ForceWindowContentWidthToNodeWidth`: separators, headers, wrapped text and default item widths use the width of the node. A node that grows at each frame is detected and explained |
| Links | Angled links (upstream PR 119): a link that would pass through its nodes is routed around them. Optional: `Style::AngledLinks`. The default link color follows the light / dark theme |
| Input | `EditorContext::DisableUserInputThisFrame()`: a widget inside a node (a plot...) can keep the mouse for itself |
| Tests | Automated tests, based on imgui_test_engine (chapter 5) |


## 2. Widgets inside nodes: what works

Between `ed::Begin()` and `ed::End()`, widgets are drawn in the coordinate space of a zoomable canvas. Dear ImGui does not know
about it, which is why some widgets need help.

| Widget, inside a node | Stock Dear ImGui | With the patches of chapter 3 |
|---|---|---|
| Buttons, sliders, checkboxes, single line text input, images, plots... | works | works |
| `BeginCombo()` / `Combo()`, `ColorEdit` / `ColorPicker` popups, `OpenPopup()` + `BeginPopup()`, tooltips, context menus | wrap them in `ed::Suspend()` / `ed::Resume()` (see below) | works as is |
| `InputTextMultiline()` | use `ed::InputTextMultiline()` | works as is |
| `Separator()`, `SeparatorText()`, `CollapsingHeader()`, `TextWrapped()` | too wide, unless `Config::ForceWindowContentWidthToNodeWidth` is set | same |
| `BeginChild()`, `BeginListBox()`, and any widget built on a child window | does not work | does not work, and is reported |

Without the patches, a popup must be opened and submitted while the editor is suspended:

```cpp
if (ImGui::Button("Options"))
{
    ed::Suspend();
    ImGui::OpenPopup("options");
    ed::Resume();
}
ed::Suspend();
if (ImGui::BeginPopup("options"))
{
    ...
    ImGui::EndPopup();
}
ed::Resume();
```

With the patches, write it as you would outside of a node. Code that still calls `Suspend()` / `Resume()` keeps working.

**Child windows do not work inside the editor**, and this fork does not change that: a child window has its own draw list and its
own clipping, which the canvas cannot transform. The usual workaround is to show the content in a popup (this is what
`ed::InputTextMultiline()` does). With the patches, beginning a child window inside the editor prints an explanation and asserts.
Child windows are fine inside a popup opened from a node, and while the editor is suspended.

A library that draws widgets and may be used inside a node can ask `ImGuiEx::IsInsideCanvas()` (in `imgui_canvas.h`) and avoid
child windows there.


## 3. Patching Dear ImGui (optional)

Two small patches, in [`misc/imgui_patches`](../misc/imgui_patches). Both are generic: nothing in them is specific to
imgui-node-editor. Each one defines a macro, and this library enables the corresponding code when it sees the macro.

| Patch | Macro | What it adds to Dear ImGui | What you get |
|---|---|---|---|
| `0001-context-hooks-begin-end-window` | `IMGUI_HAS_CONTEXT_HOOK_BEGIN_WINDOW` | Two values in `ImGuiContextHookType`: `BeginWindow` and `EndWindow`, called at the start of `ImGui::Begin()` and at the end of `ImGui::End()` | Popups, combos, color pickers, tooltips and context menus without `Suspend()` / `Resume()`; child windows are reported |
| `0002-input-text-multiline-override` | `IMGUI_HAS_INPUT_TEXT_MULTILINE_OVERRIDE` | `ImGuiContext::InputTextMultilineOverride`, a function that `ImGui::InputTextMultiline()` calls instead of its own code when it is set | `ImGui::InputTextMultiline()` works unchanged inside a node, including when it is called by a third party widget |

To apply them, from the folder of Dear ImGui:

```bash
git apply path/to/imgui-node-editor/misc/imgui_patches/0001-context-hooks-begin-end-window.patch
git apply path/to/imgui-node-editor/misc/imgui_patches/0002-input-text-multiline-override.patch
```

The `.patch` files are made against the `docking` branch. For the `master` branch, use
`0001-context-hooks-begin-end-window.for-imgui-master.diff` instead of the first one (the end of `ImGui::End()` differs between
the two branches); the second patch is the same. Checked against `v1.92.9b` and `v1.92.9b-docking`.

If a patch does not apply to your version, it is small enough to be done by hand:

- patch 1: add `ImGuiContextHookType_BeginWindow, ImGuiContextHookType_EndWindow` at the end of `enum ImGuiContextHookType`
  (`imgui_internal.h`) and `#define IMGUI_HAS_CONTEXT_HOOK_BEGIN_WINDOW` next to it; call
  `CallContextHooks(&g, ImGuiContextHookType_BeginWindow);` as the first statement of `ImGui::Begin()` (after `g` is defined), and
  `CallContextHooks(&g, ImGuiContextHookType_EndWindow);` as the last statement of `ImGui::End()`.
- patch 2: in `imgui_internal.h`, `#define IMGUI_HAS_INPUT_TEXT_MULTILINE_OVERRIDE`, declare the function pointer type
  `ImGuiInputTextMultilineOverride` (same parameters as `InputTextMultiline()`, returns `bool`) and add a member
  `ImGuiInputTextMultilineOverride InputTextMultilineOverride = NULL;` to `ImGuiContext`; at the top of `ImGui::InputTextMultiline()`,
  call it and return its result when it is not `NULL`.

How the hooks are used: `Canvas::Begin()` registers them and `Canvas::End()` removes them. When a window is begun from inside the
canvas, the BeginWindow hook converts its position to screen space (the position given with `SetNextWindowPos()`, and the positions
recorded by `OpenPopup()`), anchors combo popups below their combo, and suspends the canvas; the EndWindow hook resumes it.
The idea comes from [@lukaasm](https://github.com/thedmd/imgui-node-editor/issues/242#issuecomment-1681806764).

*For the maintainer.* The patches are the two commits "Context hooks..." and "ImGuiContext::InputTextMultilineOverride" of the
`imgui_bundle` branch of [pthom/imgui](https://github.com/pthom/imgui). After a rebase of that fork on a new Dear ImGui, refresh them
with `git format-patch --no-signature --zero-commit -N -o <this folder> <first commit>~1..<second commit>`, rename the two files, and
check them with `git apply --check` against the new upstream tag (and rebuild the `master` variant if its check fails).


## 4. Added API

**`Config::ForceWindowContentWidthToNodeWidth`** (`bool`, default `false`). Inside a node, "the available width" becomes the width of
the node instead of the width of the window that hosts the editor: `Separator()`, `SeparatorText()`, `CollapsingHeader()`,
`TextWrapped()` stop at the border of the node, and sliders and input fields get a default width that fits in it, with room for a
label of 4 wide characters.

```cpp
ed::Config config;
config.ForceWindowContentWidthToNodeWidth = true;
ed::EditorContext* editor = ed::CreateEditor(&config);
...
ed::BeginNode(nodeId);
    ImGui::Dummy(ImVec2(200, 0));                // gives its width to the node
    ImGui::SeparatorText("Settings");
    ImGui::TextWrapped("A long explanation...");
    ImGui::SliderFloat("gain", &gain, 0, 1);
ed::EndNode();
```

Two things to know:

- With this option, all the text wraps at the width of the node, so text does not give a width to the node anymore. A node needs at
  least one item with a fixed width (a `Dummy`, a widget preceded by `SetNextItemWidth()`...). A node that contains only text
  collapses to the width of one character.
- A slider or an input field whose label is longer than 4 wide characters is wider than the node. The node then grows at each frame.
  Call `SetNextItemWidth()` before such widgets. The editor detects a node whose width increased by the same amount during 100
  frames, and asserts with a message that explains the cause. This detection runs only when the option is on.

**`ed::InputTextMultiline()`** Same parameters as `ImGui::InputTextMultiline()`. Inside a node, it shows a read-only preview box with
the requested size; a click opens a resizable popup with the real editor. Outside of the editor, it calls
`ImGui::InputTextMultiline()`. With patch 2 of chapter 3 you do not need to call it.

**`ImGuiEx::IsInsideCanvas()`** (`imgui_canvas.h`). True between `Canvas::Begin()` and `Canvas::End()`, except while the canvas is
suspended: positions are then in canvas space, and child windows cannot be used.

**`StyleVar_GridSize`, `Style::GridSize`** (`ImVec2`, default 32 x 32). Size of a cell of the background grid, in canvas units.

**`Style::AngledLinks`** (`bool`, default `true`). When a link would pass through its source or its target node (the target is on
the left of the source), it is routed around them with angles (upstream PR 119). Set it to `false` to always draw a single curve,
as upstream does.

**Link color.** The default value of the `color` parameter of `ed::Link()` and `ed::BeginCreate()` is `ImVec4(0, 0, 0, 0)`, which
means "automatic": the link uses the text color of the current Dear ImGui style, so it stays readable with light and dark themes.
Any color with a non zero alpha is used as is.

**`EditorContext::DisableUserInputThisFrame()`** (internal API, in `imgui_node_editor_internal.h`). The editor ignores the mouse and
the keyboard until the next `ed::Begin()`: no pan, zoom, selection, node drag or shortcut. Call it, each frame, when a widget
inside a node handles the mouse itself (a plot that is dragged or zoomed with the wheel), typically while it is hovered:

```cpp
ImPlot::EndPlot();
if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
    ((ax::NodeEditor::Detail::EditorContext*)ed::GetCurrentEditor())->DisableUserInputThisFrame();
```


## 5. Running the tests

`tests/node_editor_tests.h` and `tests/node_editor_tests.cpp` contain 13 tests written with
[imgui_test_engine](https://github.com/ocornut/imgui_test_engine). They build a small scene (nodes with a combo, a color editor, a
multiline text, popups, a tooltip, context menus, a horizontal layout, links) and drive it with a simulated mouse, at zoom 0.5 / 1 / 2
and after a pan. They check that popups open where they should and that they can be used, and they inspect the draw list at each
frame (no canvas marker left behind, clip rects inside the window, no visible node clipped away). One test runs with the window of
the editor docked.

The files do not depend on any application framework. To plug them into an application that already runs the test engine:

```cpp
#include "tests/node_editor_tests.h"

NodeEditorTests_Register(engine);   // once, after the test engine was created
NodeEditorTests_ShowGui();          // once per frame: it opens its own window, "Node editor tests"
NodeEditorTests_Shutdown();         // before the Dear ImGui context is destroyed
```

then run them from the test engine window, or with `ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, "node_editor");`.
The scene uses `BeginHorizontal()` / `Spring()` (thedmd's stack layout for Dear ImGui) in one of its nodes.

Dear ImGui Bundle provides a runner, which its CI runs at each push:

```bash
git clone --recursive https://github.com/pthom/imgui_bundle.git
cd imgui_bundle/.github/ci_automation_tests
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHELLOIMGUI_DOWNLOAD_FREETYPE_IF_NEEDED=ON && cmake --build build -j
./build/ci_node_editor_tests            # interactive: look at the scene, run the tests from the test engine window
./build/ci_node_editor_tests --auto     # run all the tests and exit; the exit code is 0 if they all passed
```

In automatic mode the screenshots of each test are written to `node_editor_tests_captures/`.

The CI of this repository (`.github/workflows/tests.yml`) does the same thing with a pinned commit of Dear ImGui Bundle, in which
it replaces imgui-node-editor by the commit under test. A second job checks that the library compiles against a stock Dear ImGui
(docking and master), that the patches of chapter 3 apply to it, and that the library compiles with the patched Dear ImGui.
