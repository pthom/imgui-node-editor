# ifndef IMGUI_DEFINE_MATH_OPERATORS
#     define IMGUI_DEFINE_MATH_OPERATORS
# endif
# include "imgui_canvas.h"
# include <type_traits>

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
}
