# ifndef IMGUI_DEFINE_MATH_OPERATORS
#     define IMGUI_DEFINE_MATH_OPERATORS
# endif
# include "imgui_canvas.h"
# include <type_traits>
# include <cstdio>

// https://stackoverflow.com/a/36079786
# define DECLARE_HAS_MEMBER(__trait_name__, __member_name__)                         \
                                                                                     \
    template <typename __boost_has_member_T__>                                       \
    class __trait_name__                                                             \
    {                                                                                \
        using check_type = ::std::remove_const_t<__boost_has_member_T__>;            \
        struct no_type {char x[2];};                                                 \
        using  yes_type = char;                                                      \
                                                                                     \
        struct  base { void __member_name__() {}};                                   \
        struct mixin : public base, public check_type {};                            \
                                                                                     \
        template <void (base::*)()> struct aux {};                                   \
                                                                                     \
        template <typename U> static no_type  test(aux<&U::__member_name__>*);       \
        template <typename U> static yes_type test(...);                             \
                                                                                     \
        public:                                                                      \
                                                                                     \
        static constexpr bool value = (sizeof(yes_type) == sizeof(test<mixin>(0)));  \
    }

// Special sentinel value. This needs to be unique, so allow it to be overridden in the user's ImGui config
# ifndef ImDrawCallback_ImCanvas
#     define ImDrawCallback_ImCanvas        (ImDrawCallback)(-2)
# endif

// The canvas which is currently in local space (between EnterLocalSpace() and LeaveLocalSpace()), if any
static ImGuiEx::Canvas* s_CanvasInLocalSpace = nullptr;

bool ImGuiEx::IsInsideCanvas()
{
    return s_CanvasInLocalSpace != nullptr;
}

namespace ImCanvasDetails {

DECLARE_HAS_MEMBER(HasFringeScale, _FringeScale);

struct FringeScaleRef
{
    // Overload is present when ImDrawList does have _FringeScale member variable.
    template <typename T>
    static float& Get(typename std::enable_if<HasFringeScale<T>::value, T>::type* drawList)
    {
        return drawList->_FringeScale;
    }

    // Overload is present when ImDrawList does not have _FringeScale member variable.
    template <typename T>
    static float& Get(typename std::enable_if<!HasFringeScale<T>::value, T>::type*)
    {
        static float placeholder = 1.0f;
        return placeholder;
    }
};

DECLARE_HAS_MEMBER(HasVtxCurrentOffset, _VtxCurrentOffset);

struct VtxCurrentOffsetRef
{
    // Overload is present when ImDrawList does have _FringeScale member variable.
    template <typename T>
    static unsigned int& Get(typename std::enable_if<HasVtxCurrentOffset<T>::value, T>::type* drawList)
    {
        return drawList->_VtxCurrentOffset;
    }

    // Overload is present when ImDrawList does not have _FringeScale member variable.
    template <typename T>
    static unsigned int& Get(typename std::enable_if<!HasVtxCurrentOffset<T>::value, T>::type* drawList)
    {
        return drawList->_CmdHeader.VtxOffset;
    }
};

} // namespace ImCanvasDetails

// Returns a reference to _FringeScale extension to ImDrawList
//
// If ImDrawList does not have _FringeScale a placeholder is returned.
static inline float& ImFringeScaleRef(ImDrawList* drawList)
{
    using namespace ImCanvasDetails;
    return FringeScaleRef::Get<ImDrawList>(drawList);
}

static inline unsigned int& ImVtxOffsetRef(ImDrawList* drawList)
{
    using namespace ImCanvasDetails;
    return VtxCurrentOffsetRef::Get<ImDrawList>(drawList);
}

static inline ImVec2 ImSelectPositive(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x > 0.0f ? lhs.x : rhs.x, lhs.y > 0.0f ? lhs.y : rhs.y); }

bool ImGuiEx::Canvas::Begin(const char* id, const ImVec2& size)
{
    return Begin(ImGui::GetID(id), size);
}

bool ImGuiEx::Canvas::Begin(ImGuiID id, const ImVec2& size)
{
    IM_ASSERT(m_InBeginEnd == false);

    m_WidgetPosition = ImGui::GetCursorScreenPos();
    m_WidgetSize = ImSelectPositive(size, ImGui::GetContentRegionAvail());
    m_WidgetRect = ImRect(m_WidgetPosition, m_WidgetPosition + m_WidgetSize);
    m_DrawList = ImGui::GetWindowDrawList();

    UpdateViewTransformPosition();

    if (ImGui::IsClippedEx(m_WidgetRect, id))
        return false;

    // Save current channel, so we can assert when user
    // call canvas API with different one.
    m_ExpectedChannel = m_DrawList->_Splitter._Current;

    // #debug: Canvas content.
    //m_DrawList->AddRectFilled(m_StartPos, m_StartPos + m_CurrentSize, IM_COL32(0, 0, 0, 64));
    //m_DrawList->AddRect(m_WidgetRect.Min, m_WidgetRect.Max, IM_COL32(255, 0, 255, 64));

    ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));

# if IMGUI_EX_CANVAS_DEFERED()
    m_Ranges.resize(0);
# endif

    SaveInputState();
    SaveViewportState();

    // Record cursor max to prevent scrollbars from appearing.
    m_WindowCursorMaxBackup = ImGui::GetCurrentWindow()->DC.CursorMaxPos;

    EnterLocalSpace();

# if IMGUI_VERSION_NUM >= 18967
    ImGui::SetNextItemAllowOverlap();
# endif

    // Emit dummy widget matching bounds of the canvas.
    ImGui::SetCursorScreenPos(m_ViewRect.Min);
    ImGui::Dummy(m_ViewRect.GetSize());

    ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));

    m_InBeginEnd = true;

    // Popups, combos, color pickers and tooltips begun from inside the canvas work without Suspend() / Resume() when Dear ImGui
    // provides the context hooks ImGuiContextHookType_BeginWindow / EndWindow (they are not part of Dear ImGui: it is a small patch,
    // see misc/imgui_patches). The hooks convert the position of the window to screen space and suspend the canvas while it is open.
    // Without them, user code must wrap such windows in Suspend() / Resume(), as with the upstream version of this library.
    // (idea of @lukaasm, https://github.com/thedmd/imgui-node-editor/issues/242#issuecomment-1681806764)
# if defined(IMGUI_HAS_CONTEXT_HOOK_BEGIN_WINDOW)
    {
        auto beginWindowHook = ImGuiContextHook{};
        beginWindowHook.UserData = this;
        beginWindowHook.Type = ImGuiContextHookType_BeginWindow;
        beginWindowHook.Callback = []( ImGuiContext * context, ImGuiContextHook * hook )
        {
            //ImGui::SetNextWindowViewport( ImGui::GetCurrentWindow()->Viewport->ID );

            auto canvas = reinterpret_cast< Canvas * >( hook->UserData );

            // A child window is about to begin inside the canvas: this cannot work (BeginChildEx() sets the child flags right before calling Begin())
            if ( canvas->m_BeginWindowDepth == 0 && canvas->m_SuspendCounter == 0 && ( context->NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasChildFlags ) != 0 )
            {
                fprintf(stderr, "%s", R"(
Sorry, child windows are incompatible with the canvas of imgui-node-editor, and cannot be used while it is active.
    Incompatible widgets are:
        ImGui::BeginChild() and ImGui::EndChild()
        ImGui::BeginListBox() and ImGui::EndListBox()
        ImGui::InputTextMultiline(), unless Dear ImGui provides ImGuiContext::InputTextMultilineOverride (use ed::InputTextMultiline() instead)
    Please examine the call stack to find the culprit.
)");
                context->NextWindowData.ClearFlags(); // IM_ASSERT may throw (Python bindings): do not leave the child settings to the next window
                IM_ASSERT(false && "ImGui::BeginChild() should not be called inside the canvas of imgui-node-editor");
            }

            canvas->m_BeginWindowDepth += 1;
            if (canvas->m_BeginWindowDepth > 1)
                return;

            if ( canvas->m_SuspendCounter == 0 )
            {
                // Combo popup: BeginComboPopup() chose its position by testing where the popup fits, with a position in canvas space
                // and a size in pixels: the result is wrong when the canvas is zoomed (at zoom 2, a combo located low in the window
                // opens its popup above itself, with a gap). Anchor the popup below the combo instead.
                // (the combo is the last item, and the id of its popup derives from the id of the combo: see ImGui::BeginCombo())
                if ( ( context->NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasPos ) != 0 && context->OpenPopupStack.Size > 0
                    && context->OpenPopupStack.back().PopupId == ImHashStr( "##ComboPopup", 0, context->LastItemData.ID ) )
                    context->NextWindowData.PosVal = context->LastItemData.Rect.GetBL();

                if ( ( context->NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasPos ) != 0 )
                {
                    auto pos = canvas->FromLocal( context->NextWindowData.PosVal );
                    ImGui::SetNextWindowPos( pos, context->NextWindowData.PosCond, context->NextWindowData.PosPivotVal );
                }

                if ( context->BeginPopupStack.size() )
                {
                    auto & popup = context->BeginPopupStack.back();
                    popup.OpenPopupPos = canvas->FromLocal( popup.OpenPopupPos );
                    popup.OpenMousePos = canvas->FromLocal( popup.OpenMousePos );
                }

                if ( context->OpenPopupStack.size() )
                {
                    auto & popup = context->OpenPopupStack.back();
                    popup.OpenPopupPos = canvas->FromLocal( popup.OpenPopupPos );
                    popup.OpenMousePos = canvas->FromLocal( popup.OpenMousePos );
                }

            }
            canvas->m_BeginWindowCursorBackup = ImGui::GetCursorScreenPos();
            // Suspend() must run on the draw channel that was current in Begin(). The node editor splits the draw list into channels:
            // between two nodes, another channel is current (the editor does the same thing in its own Suspend())
            const int lastChannel = canvas->m_DrawList->_Splitter._Current;
            canvas->m_DrawList->ChannelsSetCurrent(canvas->m_ExpectedChannel);
            canvas->Suspend();
            canvas->m_DrawList->ChannelsSetCurrent(lastChannel);
        };

        m_beginWindowHook = ImGui::AddContextHook( ImGui::GetCurrentContext(), &beginWindowHook );

        auto endWindowHook = ImGuiContextHook{};
        endWindowHook.UserData = this;
        endWindowHook.Type = ImGuiContextHookType_EndWindow;
        endWindowHook.Callback = []( ImGuiContext * ctx, ImGuiContextHook * hook )
        {
            auto canvas = reinterpret_cast< Canvas * >( hook->UserData );

            canvas->m_BeginWindowDepth -= 1;
            if (canvas->m_BeginWindowDepth > 0)
                return;

            const int lastChannel = canvas->m_DrawList->_Splitter._Current;
            canvas->m_DrawList->ChannelsSetCurrent(canvas->m_ExpectedChannel);
            canvas->Resume();
            canvas->m_DrawList->ChannelsSetCurrent(lastChannel);
            ImGui::SetCursorScreenPos( canvas->m_BeginWindowCursorBackup );
            ImGui::GetCurrentWindow()->DC.IsSetPos = false;
        };

        m_endWindowHook = ImGui::AddContextHook( ImGui::GetCurrentContext(), &endWindowHook );
    }
# endif // IMGUI_HAS_CONTEXT_HOOK_BEGIN_WINDOW

    return true;
}

void ImGuiEx::Canvas::End()
{
    // If you're here your call to Begin() returned false,
    // or Begin() wasn't called at all.
    IM_ASSERT(m_InBeginEnd == true);

    // If you're here, please make sure you do not interleave
    // channel splitter with canvas.
    // Always call canvas function with using same channel.
    IM_ASSERT(m_DrawList->_Splitter._Current == m_ExpectedChannel);

    //auto& io = ImGui::GetIO();

    // Check: Unmatched calls to Suspend() / Resume(). Please check your code.
    IM_ASSERT(m_SuspendCounter == 0);

    LeaveLocalSpace();

    // Sentinel-keyed clip-rect transform (post-merge).
    // By now the node editor has merged all channels (it calls ChannelsMerge() before Canvas::End),
    // so command order is final. Walk the buffer: a BEGIN sentinel (UserCallbackData==nullptr) opens
    // a canvas local-space region, an END sentinel (UserCallbackData==(void*)1) closes it. Transform
    // the clip rect of every command inside a region exactly once (vertices were already transformed
    // in LeaveLocalSpace), then remove all sentinels (they must not reach the backend).
    {
        int depth = 0;
        for (int i = 0; i < m_DrawList->CmdBuffer.Size; ++i)
        {
            ImDrawCmd& cmd = m_DrawList->CmdBuffer[i];
            if (cmd.UserCallback == ImDrawCallback_ImCanvas)
            {
                if (cmd.UserCallbackData == nullptr)
                    ++depth;   // BEGIN
                else
                    --depth;   // END
                continue;
            }
            if (depth > 0)
            {
                cmd.ClipRect.x = cmd.ClipRect.x * m_View.Scale + m_ViewTransformPosition.x;
                cmd.ClipRect.y = cmd.ClipRect.y * m_View.Scale + m_ViewTransformPosition.y;
                cmd.ClipRect.z = cmd.ClipRect.z * m_View.Scale + m_ViewTransformPosition.x;
                cmd.ClipRect.w = cmd.ClipRect.w * m_View.Scale + m_ViewTransformPosition.y;
            }
        }
        // Remove all sentinels (they must not reach the backend).
        for (auto it = m_DrawList->CmdBuffer.begin(); it != m_DrawList->CmdBuffer.end(); )
        {
            if (it->UserCallback == ImDrawCallback_ImCanvas)
                it = m_DrawList->CmdBuffer.erase(it);
            else
                ++it;
        }
    }

    ImGui::GetCurrentWindow()->DC.CursorMaxPos = m_WindowCursorMaxBackup;

# if IMGUI_VERSION_NUM < 18967
    ImGui::SetItemAllowOverlap();
# endif

    // Emit dummy widget matching bounds of the canvas.
    ImGui::SetCursorScreenPos(m_WidgetPosition);
    ImGui::Dummy(m_WidgetSize);

    // #debug: Rect around canvas. Content should be inside these bounds.
    //m_DrawList->AddRect(m_WidgetPosition - ImVec2(1.0f, 1.0f), m_WidgetPosition + m_WidgetSize + ImVec2(1.0f, 1.0f), IM_COL32(196, 0, 0, 255));

    m_InBeginEnd = false;

# if defined(IMGUI_HAS_CONTEXT_HOOK_BEGIN_WINDOW)
    {
        ImGui::RemoveContextHook( ImGui::GetCurrentContext(), m_beginWindowHook );
        ImGui::RemoveContextHook( ImGui::GetCurrentContext(), m_endWindowHook );
    }
# endif
}

void ImGuiEx::Canvas::SetView(const ImVec2& origin, float scale)
{
    SetView(CanvasView(origin, scale));
}

void ImGuiEx::Canvas::SetView(const CanvasView& view)
{
    if (m_InBeginEnd)
        LeaveLocalSpace();

    if (m_View.Origin.x != view.Origin.x || m_View.Origin.y != view.Origin.y)
    {
        m_View.Origin = view.Origin;

        UpdateViewTransformPosition();
    }

    if (m_View.Scale != view.Scale)
    {
        m_View.Scale    = view.Scale;
        m_View.InvScale = view.InvScale;
    }

    if (m_InBeginEnd)
        EnterLocalSpace();
}

void ImGuiEx::Canvas::CenterView(const ImVec2& canvasPoint)
{
    auto view = CalcCenterView(canvasPoint);
    SetView(view);
}

ImGuiEx::CanvasView ImGuiEx::Canvas::CalcCenterView(const ImVec2& canvasPoint) const
{
    auto localCenter = ToLocal(m_WidgetPosition + m_WidgetSize * 0.5f);
    auto localOffset = canvasPoint - localCenter;
    auto offset      = FromLocalV(localOffset);

    return CanvasView{ m_View.Origin - offset, m_View.Scale };
}

void ImGuiEx::Canvas::CenterView(const ImRect& canvasRect)
{
    auto view = CalcCenterView(canvasRect);

    SetView(view);
}

ImGuiEx::CanvasView ImGuiEx::Canvas::CalcCenterView(const ImRect& canvasRect) const
{
    auto canvasRectSize = canvasRect.GetSize();

    if (canvasRectSize.x <= 0.0f || canvasRectSize.y <= 0.0f)
        return View();

    auto widgetAspectRatio     = m_WidgetSize.y   > 0.0f ? m_WidgetSize.x   / m_WidgetSize.y   : 0.0f;
    auto canvasRectAspectRatio = canvasRectSize.y > 0.0f ? canvasRectSize.x / canvasRectSize.y : 0.0f;

    if (widgetAspectRatio <= 0.0f || canvasRectAspectRatio <= 0.0f)
        return View();

    auto newOrigin = m_View.Origin;
    auto newScale  = m_View.Scale;
    if (canvasRectAspectRatio > widgetAspectRatio)
    {
        // width span across view
        newScale = m_WidgetSize.x / canvasRectSize.x;
        newOrigin = canvasRect.Min * -newScale;
        newOrigin.y += (m_WidgetSize.y - canvasRectSize.y * newScale) * 0.5f;
    }
    else
    {
        // height span across view
        newScale = m_WidgetSize.y / canvasRectSize.y;
        newOrigin = canvasRect.Min * -newScale;
        newOrigin.x += (m_WidgetSize.x - canvasRectSize.x * newScale) * 0.5f;
    }

    return CanvasView{ newOrigin, newScale };
}

void ImGuiEx::Canvas::Suspend()
{
    // If you're here, please make sure you do not interleave
    // channel splitter with canvas.
    // Always call canvas function with using same channel.
    IM_ASSERT(m_DrawList->_Splitter._Current == m_ExpectedChannel);

    if (m_SuspendCounter == 0)
    {
        LeaveLocalSpace();
    }

    ++m_SuspendCounter;
}

void ImGuiEx::Canvas::Resume()
{
    // If you're here, please make sure you do not interleave
    // channel splitter with canvas.
    // Always call canvas function with using same channel.
    IM_ASSERT(m_DrawList->_Splitter._Current == m_ExpectedChannel);

    // Check: Number of calls to Resume() do not match calls to Suspend(). Please check your code.
    IM_ASSERT(m_SuspendCounter > 0);
    if (--m_SuspendCounter == 0)
        EnterLocalSpace();
}

ImVec2 ImGuiEx::Canvas::FromLocal(const ImVec2& point) const
{
    return point * m_View.Scale + m_ViewTransformPosition;
}

ImVec2 ImGuiEx::Canvas::FromLocal(const ImVec2& point, const CanvasView& view) const
{
    return point * view.Scale + view.Origin + m_WidgetPosition;
}

ImVec2 ImGuiEx::Canvas::FromLocalV(const ImVec2& vector) const
{
    return vector * m_View.Scale;
}

ImVec2 ImGuiEx::Canvas::FromLocalV(const ImVec2& vector, const CanvasView& view) const
{
    return vector * view.Scale;
}

ImVec2 ImGuiEx::Canvas::ToLocal(const ImVec2& point) const
{
    return (point - m_ViewTransformPosition) * m_View.InvScale;
}

ImVec2 ImGuiEx::Canvas::ToLocal(const ImVec2& point, const CanvasView& view) const
{
    return (point - view.Origin - m_WidgetPosition) * view.InvScale;
}

ImVec2 ImGuiEx::Canvas::ToLocalV(const ImVec2& vector) const
{
    return vector * m_View.InvScale;
}

ImVec2 ImGuiEx::Canvas::ToLocalV(const ImVec2& vector, const CanvasView& view) const
{
    return vector * view.InvScale;
}

ImRect ImGuiEx::Canvas::CalcViewRect(const CanvasView& view) const
{
    ImRect result;
    result.Min = ImVec2(-view.Origin.x, -view.Origin.y) * view.InvScale;
    result.Max = (m_WidgetSize - view.Origin) * view.InvScale;
    return result;
}

void ImGuiEx::Canvas::UpdateViewTransformPosition()
{
    m_ViewTransformPosition = m_View.Origin + m_WidgetPosition;
}

void ImGuiEx::Canvas::SaveInputState()
{
    auto& io = ImGui::GetIO();
    m_MousePosBackup = io.MousePos;
    m_MousePosPrevBackup = io.MousePosPrev;
    for (auto i = 0; i < IM_ARRAYSIZE(m_MouseClickedPosBackup); ++i)
        m_MouseClickedPosBackup[i] = io.MouseClickedPos[i];
}

void ImGuiEx::Canvas::RestoreInputState()
{
    auto& io = ImGui::GetIO();
    io.MousePos = m_MousePosBackup;
    io.MousePosPrev = m_MousePosPrevBackup;
    for (auto i = 0; i < IM_ARRAYSIZE(m_MouseClickedPosBackup); ++i)
        io.MouseClickedPos[i] = m_MouseClickedPosBackup[i];
}

void ImGuiEx::Canvas::SaveViewportState()
{
# if defined(IMGUI_HAS_VIEWPORT)
    auto window = ImGui::GetCurrentWindow();
    auto viewport = ImGui::GetWindowViewport();

    m_WindowPosBackup = window->Pos;
    m_ViewportPosBackup = viewport->Pos;
    m_ViewportSizeBackup = viewport->Size;
    m_ViewportWorkPosBackup = viewport->WorkPos;
    m_ViewportWorkSizeBackup = viewport->WorkSize;
# endif
}

void ImGuiEx::Canvas::RestoreViewportState()
{
# if defined(IMGUI_HAS_VIEWPORT)
    auto window = ImGui::GetCurrentWindow();
    auto viewport = ImGui::GetWindowViewport();

    window->Pos = m_WindowPosBackup;
    viewport->Pos = m_ViewportPosBackup;
    viewport->Size = m_ViewportSizeBackup;
    viewport->WorkPos = m_ViewportWorkPosBackup;
    viewport->WorkSize = m_ViewportWorkSizeBackup;
# endif
}

void ImGuiEx::Canvas::EnterLocalSpace()
{
    // Prepare ImDrawList for drawing in local coordinate system:
    //   - determine visible part of the canvas
    //   - start unique draw command
    //   - add clip rect matching canvas size
    //   - record current command index
    //   - record current vertex write index

    // Determine visible part of the canvas. Make it before
    // adding new command, to avoid round rip where command
    // is removed in PopClipRect() and added again next PushClipRect().
    ImGui::PushClipRect(m_WidgetPosition, m_WidgetPosition + m_WidgetSize, true);
    auto clipped_clip_rect = m_DrawList->_ClipRectStack.back();
    ImGui::PopClipRect();

# if IMGUI_EX_CANVAS_DEFERED()
    m_Ranges.resize(m_Ranges.Size + 1);
    m_CurrentRange = &m_Ranges.back();
    m_CurrentRange->BeginComandIndex = ImMax(m_DrawList->CmdBuffer.Size, 0);
    m_CurrentRange->BeginVertexIndex = m_DrawList->_VtxCurrentIdx + ImVtxOffsetRef(m_DrawList);
# endif
    m_DrawListStartVertexIndex       = m_DrawList->_VtxCurrentIdx + ImVtxOffsetRef(m_DrawList);

    // Insert a BEGIN sentinel (ImDrawCallback_ImCanvas, UserCallbackData==nullptr)
    // plus a fresh draw command to isolate this scope's canvas content (prevents clip-rect leakage /
    // command merging across the canvas boundary). LeaveLocalSpace() adds a matching END sentinel;
    // Canvas::End() walks BEGIN..END regions post-merge to transform their clip rects local->screen
    // exactly once. Sentinels are used as boundary markers because they travel with their commands
    // through the node editor's channel merges, whereas captured command indices do not.
    m_DrawList->AddCallback(ImDrawCallback_ImCanvas, nullptr);
    m_DrawList->AddDrawCmd();

# if defined(IMGUI_HAS_VIEWPORT)
    auto window = ImGui::GetCurrentWindow();
    window->Pos = ImVec2(0.0f, 0.0f);

    auto viewport_min = m_ViewportPosBackup;
    auto viewport_max = m_ViewportPosBackup + m_ViewportSizeBackup;

    viewport_min.x = (viewport_min.x - m_ViewTransformPosition.x) * m_View.InvScale;
    viewport_min.y = (viewport_min.y - m_ViewTransformPosition.y) * m_View.InvScale;
    viewport_max.x = (viewport_max.x - m_ViewTransformPosition.x) * m_View.InvScale;
    viewport_max.y = (viewport_max.y - m_ViewTransformPosition.y) * m_View.InvScale;

    auto viewport = ImGui::GetWindowViewport();
    viewport->Pos  = viewport_min;
    viewport->Size = viewport_max - viewport_min;

    viewport->WorkPos  = m_ViewportWorkPosBackup  * m_View.InvScale;
    viewport->WorkSize = m_ViewportWorkSizeBackup * m_View.InvScale;
# endif

    // Clip rectangle in parent canvas space and move it to local space.
    clipped_clip_rect.x = (clipped_clip_rect.x - m_ViewTransformPosition.x) * m_View.InvScale;
    clipped_clip_rect.y = (clipped_clip_rect.y - m_ViewTransformPosition.y) * m_View.InvScale;
    clipped_clip_rect.z = (clipped_clip_rect.z - m_ViewTransformPosition.x) * m_View.InvScale;
    clipped_clip_rect.w = (clipped_clip_rect.w - m_ViewTransformPosition.y) * m_View.InvScale;
    ImGui::PushClipRect(ImVec2(clipped_clip_rect.x, clipped_clip_rect.y), ImVec2(clipped_clip_rect.z, clipped_clip_rect.w), false);

    // Transform mouse position to local space.
    auto& io = ImGui::GetIO();
    io.MousePos     = (m_MousePosBackup - m_ViewTransformPosition) * m_View.InvScale;
    io.MousePosPrev = (m_MousePosPrevBackup - m_ViewTransformPosition) * m_View.InvScale;
    for (auto i = 0; i < IM_ARRAYSIZE(m_MouseClickedPosBackup); ++i)
        io.MouseClickedPos[i] = (m_MouseClickedPosBackup[i] - m_ViewTransformPosition) * m_View.InvScale;

    m_ViewRect = CalcViewRect(m_View);;

    auto& fringeScale = ImFringeScaleRef(m_DrawList);
    m_LastFringeScale = fringeScale;
    fringeScale *= m_View.InvScale;

    s_CanvasInLocalSpace = this;
# if defined(IMGUI_HAS_INPUT_TEXT_MULTILINE_OVERRIDE)
    // Inside the canvas, ImGui::InputTextMultiline() is replaced by a version that does not use a child window
    ImGui::GetCurrentContext()->InputTextMultilineOverride = &ImGuiEx::CanvasInputTextMultiline;
# endif
}

void ImGuiEx::Canvas::LeaveLocalSpace()
{
    IM_ASSERT(m_DrawList->_Splitter._Current == m_ExpectedChannel);

# if IMGUI_EX_CANVAS_DEFERED()
    IM_ASSERT(m_CurrentRange != nullptr);

    m_CurrentRange->EndVertexIndex  = m_DrawList->_VtxCurrentIdx + ImVtxOffsetRef(m_DrawList);
    m_CurrentRange->EndCommandIndex = m_DrawList->CmdBuffer.size();
    if (m_CurrentRange->BeginVertexIndex == m_CurrentRange->EndVertexIndex)
    {
        // Drop empty range
        m_Ranges.resize(m_Ranges.Size - 1);
    }
    m_CurrentRange = nullptr;
# endif

    // Transform vertices to screen space inline: the vertex buffer is
    // append-only and is never reordered by the node editor's channel merges, so the recorded
    // vertex range stays valid here.
    // Clip rects are intentionally NOT transformed here: command indices ARE reordered by channel
    // merges, so transforming them by index mid-frame double/under-counts and drops geometry when
    // the view is panned/zoomed. Instead we mark this scope's end with a sentinel and transform
    // clip rects once, post-merge, in End() (sentinels travel with their commands through merges).
    auto vertex    = m_DrawList->VtxBuffer.Data + m_DrawListStartVertexIndex;
    auto vertexEnd = m_DrawList->VtxBuffer.Data + m_DrawList->_VtxCurrentIdx + ImVtxOffsetRef(m_DrawList);
    if (m_View.Scale != 1.0f)
    {
        while (vertex < vertexEnd)
        {
            vertex->pos.x = vertex->pos.x * m_View.Scale + m_ViewTransformPosition.x;
            vertex->pos.y = vertex->pos.y * m_View.Scale + m_ViewTransformPosition.y;
            ++vertex;
        }
    }
    else
    {
        while (vertex < vertexEnd)
        {
            vertex->pos.x = vertex->pos.x + m_ViewTransformPosition.x;
            vertex->pos.y = vertex->pos.y + m_ViewTransformPosition.y;
            ++vertex;
        }
    }

    // END sentinel for this local-space scope. EnterLocalSpace() added the
    // BEGIN sentinel (ImDrawCallback_ImCanvas, UserCallbackData==nullptr); this END one uses
    // UserCallbackData==(void*)1. End() pairs BEGIN/END post-merge to transform clip rects once.
    m_DrawList->AddCallback(ImDrawCallback_ImCanvas, (void*)1);

    auto& fringeScale = ImFringeScaleRef(m_DrawList);
    fringeScale = m_LastFringeScale;

    // And pop \o/
    ImGui::PopClipRect();

    RestoreInputState();
    RestoreViewportState();

    s_CanvasInLocalSpace = nullptr;
# if defined(IMGUI_HAS_INPUT_TEXT_MULTILINE_OVERRIDE)
    ImGui::GetCurrentContext()->InputTextMultilineOverride = NULL;
# endif
}

//------------------------------------------------------------------------------
// InputTextMultiline inside a canvas
//------------------------------------------------------------------------------

// A popup must be opened and begun while the canvas is suspended. With the BeginWindow / EndWindow hooks of Dear ImGui,
// the canvas does it by itself; without them, we do it here.
struct SuspendCanvasIfNoHook
{
# if !defined(IMGUI_HAS_CONTEXT_HOOK_BEGIN_WINDOW)
    ImGuiEx::Canvas* m_Canvas;
    SuspendCanvasIfNoHook(): m_Canvas(s_CanvasInLocalSpace) { if (m_Canvas) m_Canvas->Suspend(); }
    ~SuspendCanvasIfNoHook()                                { if (m_Canvas) m_Canvas->Resume(); }
# endif
};

bool ImGuiEx::CanvasInputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    using namespace ImGui;

    if (s_CanvasInLocalSpace == nullptr)
        return InputTextMultiline(label, buf, buf_size, size, flags, callback, user_data);

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;

    PushID(label); // make sure to use unique ids
    const ImGuiID box_id = window->GetID("##ml_box");

    // Resolve the size exactly like the real multiline widget (default = 8 lines high),
    // honoring SetNextItemWidth() through CalcItemWidth().
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const float default_h = g.FontSize * 8.0f + style.FramePadding.y * 2.0f;
    const ImVec2 frame_size = CalcItemSize(size, CalcItemWidth(), default_h);

    const ImVec2 frame_min = window->DC.CursorPos;
    const ImRect frame_bb(frame_min, frame_min + frame_size);
    const ImRect total_bb(frame_min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, box_id, &frame_bb))
    {
        PopID();
        return false;
    }

    // Interaction: behave like any widget. ButtonBehavior takes ActiveId on press, so a drag
    // started on the box does NOT move the node (you grab the node body/title instead).
    bool hovered;
    bool pressed = ButtonBehavior(frame_bb, box_id, &hovered, NULL);
    if (hovered)
        SetMouseCursor(ImGuiMouseCursor_TextInput);
    // Open the popup on a click, but not when the press turned into a drag (e.g. dragging an
    // external resize grip placed over the box) - otherwise the editor would pop open on resize.
    if (pressed && !IsMouseDragPastThreshold(0))
    {
        SuspendCanvasIfNoHook suspend;
        OpenPopup("##ml_edit");
    }

    // The popup being open means "edited elsewhere": keep the box highlighted and fade its
    // text, so the link between the box and the popup stays obvious.
    const bool editing = IsPopupOpen("##ml_edit");
    const bool highlight = hovered || editing;

    // Frame background + border. On highlight, blend FrameBg -> FrameBgHovered (theme-aware, subtle).
    const ImU32 frame_col = highlight
        ? GetColorU32(ImLerp(style.Colors[ImGuiCol_FrameBg], style.Colors[ImGuiCol_FrameBgHovered], 0.25f))
        : GetColorU32(ImGuiCol_FrameBg);
    RenderNavCursor(frame_bb, box_id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, style.FrameRounding);

    // Draw the text, clipped to the inner area (no word-wrap, like the real widget).
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const float line_h = g.FontSize;
    const ImU32 text_col = GetColorU32(ImGuiCol_Text, editing ? 0.5f : 1.0f);
    const char* text_end = buf + strlen(buf);
    window->DrawList->PushClipRect(inner_bb.Min, inner_bb.Max, true);
    {
        const char* s = buf;
        ImVec2 pos = inner_bb.Min;
        int n_lines = 0;
        bool horiz_overflow = false;
        while (s <= text_end)
        {
            const char* line_end = strchr(s, '\n');
            if (line_end == NULL)
                line_end = text_end;
            if (pos.y > inner_bb.Max.y) // stop drawing once below the visible area
                { n_lines++; break; }
            window->DrawList->AddText(pos, text_col, s, line_end);
            if (CalcTextSize(s, line_end).x > inner_bb.GetWidth())
                horiz_overflow = true;
            pos.y += line_h;
            n_lines++;
            if (line_end == text_end)
                break;
            s = line_end + 1;
        }

        // Overflow hints: fade the box fill back in along each clipped edge.
        const ImU32 c_transp = frame_col & ~IM_COL32_A_MASK;
        if ((float)n_lines * line_h > inner_bb.GetHeight() + 1.0f)
        {
            const float fade_h = ImMin(line_h, inner_bb.GetHeight());
            window->DrawList->AddRectFilledMultiColor(
                ImVec2(frame_bb.Min.x, frame_bb.Max.y - fade_h), frame_bb.Max,
                c_transp, c_transp, frame_col, frame_col);
        }
        if (horiz_overflow)
        {
            const float fade_w = ImMin(line_h * 1.5f, inner_bb.GetWidth());
            window->DrawList->AddRectFilledMultiColor(
                ImVec2(frame_bb.Max.x - fade_w, frame_bb.Min.y), frame_bb.Max,
                c_transp, frame_col, frame_col, c_transp);
        }
    }
    window->DrawList->PopClipRect();

    // Label, to the right of the frame (rendered up to "##", like the real widget).
    if (label_size.x > 0.0f)
    {
        const char* label_end = FindRenderedTextEnd(label);
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label, label_end);
    }

    // Popup with the real editor. The popup is outside the canvas, so its child window works
    // (iff the node-editor popup patches are applied:
    //  https://github.com/thedmd/imgui-node-editor/issues/242#issuecomment-1681806764
    //  https://github.com/thedmd/imgui-node-editor/issues/242#issuecomment-2404714757 ).
    // We use BeginPopupEx (not BeginPopup, which forces AlwaysAutoResize) so the popup is
    // resizable: it opens at the requested size, then keeps its own resized size across reopens.
    // Resizing the popup does NOT change the read-only preview box.
    // There is no infinite recursion: inside the popup we are no longer in the canvas.
    bool changed = false;
    {
        SuspendCanvasIfNoHook suspend;
        const ImGuiID popup_id = GetID("##ml_edit");
        SetNextWindowSize(frame_size + style.WindowPadding * 2.0f, ImGuiCond_FirstUseEver);
        if (BeginPopupEx(popup_id, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
        {
            if (InputTextMultiline("##edit", buf, buf_size, GetContentRegionAvail(), flags, callback, user_data))
                changed = true;
            EndPopup();
        }
    }

    PopID();
    return changed;
}
