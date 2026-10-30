#pragma once
#include "imgui.h"
#include <cmath>

// ============================================================
//  Axiom VR Mod – Animated Orange-Green Gradient Theme
//  [made by TagtusVR]
// ============================================================

namespace Axiom {

// Returns a colour interpolated between orange and green,
// cycling over `period_sec` seconds.
inline ImVec4 GradientColor(float time_sec, float period_sec = 3.0f,
                             float alpha = 1.0f) {
    float t = (std::sin(time_sec * (2.0f * 3.14159265f / period_sec)) + 1.0f) * 0.5f;
    // orange: (1.0, 0.45, 0.0)  green: (0.0, 0.85, 0.2)
    float r = 1.0f * (1.0f - t) + 0.0f * t;
    float g = 0.45f * (1.0f - t) + 0.85f * t;
    float b = 0.0f * (1.0f - t) + 0.2f * t;
    return ImVec4(r, g, b, alpha);
}

// Lighter variant (for hovered / active states)
inline ImVec4 GradientColorLight(float time_sec, float period_sec = 3.0f) {
    ImVec4 c = GradientColor(time_sec, period_sec);
    c.x = std::fmin(c.x + 0.25f, 1.0f);
    c.y = std::fmin(c.y + 0.20f, 1.0f);
    c.z = std::fmin(c.z + 0.15f, 1.0f);
    return c;
}

// Apply the full Axiom style to ImGui.
// Call once per frame (before Begin) so gradient animates.
inline void ApplyTheme(float time_sec) {
    ImGuiStyle& s = ImGui::GetStyle();

    // ---- geometry ----
    s.WindowRounding    = 8.0f;
    s.FrameRounding     = 5.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 4.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.ItemSpacing       = ImVec2(8, 6);
    s.WindowPadding     = ImVec2(12, 10);

    ImVec4* c = s.Colors;
    ImVec4 accent      = GradientColor(time_sec);
    ImVec4 accentLight = GradientColorLight(time_sec);
    ImVec4 bg          = ImVec4(0.07f, 0.07f, 0.07f, 0.96f);
    ImVec4 bgChild     = ImVec4(0.10f, 0.10f, 0.10f, 0.96f);
    ImVec4 frame       = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
    ImVec4 text        = ImVec4(0.93f, 0.93f, 0.93f, 1.0f);
    ImVec4 textDim     = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
    ImVec4 scrollBg    = ImVec4(0.04f, 0.04f, 0.04f, 0.80f);

    c[ImGuiCol_Text]                  = text;
    c[ImGuiCol_TextDisabled]          = textDim;
    c[ImGuiCol_WindowBg]              = bg;
    c[ImGuiCol_ChildBg]               = bgChild;
    c[ImGuiCol_PopupBg]               = bg;
    c[ImGuiCol_Border]                = ImVec4(accent.x, accent.y, accent.z, 0.50f);
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = frame;
    c[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
    c[ImGuiCol_TitleBg]               = ImVec4(accent.x*0.4f, accent.y*0.4f, accent.z*0.4f, 1.0f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(accent.x*0.6f, accent.y*0.6f, accent.z*0.6f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed]      = ImVec4(accent.x*0.2f, accent.y*0.2f, accent.z*0.2f, 0.75f);
    c[ImGuiCol_MenuBarBg]             = bgChild;
    c[ImGuiCol_ScrollbarBg]           = scrollBg;
    c[ImGuiCol_ScrollbarGrab]         = accent;
    c[ImGuiCol_ScrollbarGrabHovered]  = accentLight;
    c[ImGuiCol_ScrollbarGrabActive]   = accentLight;
    c[ImGuiCol_CheckMark]             = accent;
    c[ImGuiCol_SliderGrab]            = accent;
    c[ImGuiCol_SliderGrabActive]      = accentLight;
    c[ImGuiCol_Button]                = ImVec4(accent.x*0.5f, accent.y*0.5f, accent.z*0.5f, 1.0f);
    c[ImGuiCol_ButtonHovered]         = accent;
    c[ImGuiCol_ButtonActive]          = accentLight;
    c[ImGuiCol_Header]                = ImVec4(accent.x*0.4f, accent.y*0.4f, accent.z*0.4f, 0.80f);
    c[ImGuiCol_HeaderHovered]         = ImVec4(accent.x*0.6f, accent.y*0.6f, accent.z*0.6f, 0.90f);
    c[ImGuiCol_HeaderActive]          = accent;
    c[ImGuiCol_Separator]             = ImVec4(accent.x, accent.y, accent.z, 0.40f);
    c[ImGuiCol_SeparatorHovered]      = accent;
    c[ImGuiCol_SeparatorActive]       = accentLight;
    c[ImGuiCol_ResizeGrip]            = ImVec4(accent.x, accent.y, accent.z, 0.20f);
    c[ImGuiCol_ResizeGripHovered]     = ImVec4(accent.x, accent.y, accent.z, 0.67f);
    c[ImGuiCol_ResizeGripActive]      = accentLight;
    c[ImGuiCol_Tab]                   = ImVec4(accent.x*0.3f, accent.y*0.3f, accent.z*0.3f, 0.86f);
    c[ImGuiCol_TabHovered]            = accent;
    c[ImGuiCol_TabActive]             = ImVec4(accent.x*0.6f, accent.y*0.6f, accent.z*0.6f, 1.0f);
    c[ImGuiCol_TabUnfocused]          = ImVec4(0.10f, 0.10f, 0.10f, 0.97f);
    c[ImGuiCol_TabUnfocusedActive]    = ImVec4(accent.x*0.4f, accent.y*0.4f, accent.z*0.4f, 1.0f);
    c[ImGuiCol_PlotLines]             = accent;
    c[ImGuiCol_PlotLinesHovered]      = accentLight;
    c[ImGuiCol_PlotHistogram]         = accent;
    c[ImGuiCol_PlotHistogramHovered]  = accentLight;
    c[ImGuiCol_TextSelectedBg]        = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_DragDropTarget]        = accentLight;
    c[ImGuiCol_NavHighlight]          = accent;
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1,1,1,0.70f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.8f,0.8f,0.8f,0.20f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.1f,0.1f,0.1f,0.50f);
}

// Draw a gradient-filled title bar (call inside a window).
// `label`  – text to display
// `height` – bar pixel height
inline void GradientTitleBar(const char* label, float height,
                              float time_sec, float period_sec = 3.0f) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, height);

    float t = (std::sin(time_sec * (2.0f * 3.14159265f / period_sec)) + 1.0f) * 0.5f;
    ImU32 colL = IM_COL32(
        (int)(255 * 1.0f),
        (int)(255 * 0.45f * (1.0f-t) + 255 * 0.85f * t),
        (int)(255 * 0.0f * (1.0f-t) + 255 * 0.20f * t),
        220);
    ImU32 colR = IM_COL32(
        (int)(255 * 0.0f * (1.0f-t) + 255 * 0.5f * t),
        (int)(255 * 0.85f),
        (int)(255 * 0.2f),
        220);

    dl->AddRectFilledMultiColor(
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        colL, colR, colR, colL);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + 8, pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::TextUnformatted(label);
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + 4));
}

} // namespace Axiom
