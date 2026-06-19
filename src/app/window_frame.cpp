#include "app/window_frame.hpp"

#include <algorithm>
#include <cmath>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#endif

#include <imgui.h>

namespace codewizard::app {

namespace {

constexpr int resize_border_width = 8;
constexpr float title_bar_height = 52.0F;
constexpr float title_button_size = 34.0F;
constexpr float title_button_spacing = 8.0F;
constexpr float theme_button_width = 72.0F;

float title_controls_width()
{
    return theme_button_width +
        title_button_size * 2.0F +
        title_button_spacing * 3.0F +
        16.0F;
}

} // namespace

WindowFrame::WindowFrame(GLFWwindow* window)
    : window_(window),
      cursors_{
          glfwCreateStandardCursor(GLFW_ARROW_CURSOR),
          glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR),
          glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR),
          glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR),
          glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR)
      }
{
}

WindowFrame::~WindowFrame()
{
    destroy_cursors();
}

WindowFrame::ScreenPoint WindowFrame::cursor_screen_position() const
{
#ifdef _WIN32
    POINT cursor{};
    GetCursorPos(&cursor);
    return {cursor.x, cursor.y};
#else
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    int window_x = 0;
    int window_y = 0;
    glfwGetCursorPos(window_, &cursor_x, &cursor_y);
    glfwGetWindowPos(window_, &window_x, &window_y);
    return {
        window_x + static_cast<int>(std::lround(cursor_x)),
        window_y + static_cast<int>(std::lround(cursor_y))
    };
#endif
}

bool WindowFrame::left_mouse_down() const
{
#ifdef _WIN32
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
#else
    return glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
#endif
}

void WindowFrame::set_position(int x, int y) const
{
#ifdef _WIN32
    SetWindowPos(
        glfwGetWin32Window(window_),
        nullptr,
        x,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER
    );
#else
    glfwSetWindowPos(window_, x, y);
#endif
}

void WindowFrame::set_bounds(int x, int y, int width, int height) const
{
#ifdef _WIN32
    SetWindowPos(
        glfwGetWin32Window(window_),
        nullptr,
        x,
        y,
        width,
        height,
        SWP_NOZORDER | SWP_NOOWNERZORDER
    );
#else
    glfwSetWindowPos(window_, x, y);
    glfwSetWindowSize(window_, width, height);
#endif
}

int WindowFrame::hit_test_resize_edges(int window_width, int window_height, double cursor_x, double cursor_y) const
{
    int edges = resize_none;

    if (cursor_x >= 0.0 && cursor_x < resize_border_width) {
        edges |= resize_left;
    }

    const bool right_resize_allowed =
        cursor_y < title_bar_height || cursor_y >= window_height - resize_border_width;

    if (
        right_resize_allowed &&
        cursor_x <= window_width &&
        cursor_x >= window_width - resize_border_width
    ) {
        edges |= resize_right;
    }

    if (cursor_y >= 0.0 && cursor_y < resize_border_width) {
        edges |= resize_top;
    }

    if (cursor_y <= window_height && cursor_y >= window_height - resize_border_width) {
        edges |= resize_bottom;
    }

    return edges;
}

bool WindowFrame::is_title_drag_zone(int window_width, double cursor_x, double cursor_y) const
{
    if (cursor_y < resize_border_width || cursor_y >= title_bar_height) {
        return false;
    }

    const float drag_width = static_cast<float>(window_width) - title_controls_width();
    return cursor_x >= 0.0 && cursor_x < drag_width;
}

void WindowFrame::update_cursor()
{
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    int window_width = 0;
    int window_height = 0;
    glfwGetCursorPos(window_, &cursor_x, &cursor_y);
    glfwGetWindowSize(window_, &window_width, &window_height);

    const int resize_edges = interaction_.mode == InteractionMode::resize
        ? interaction_.resize_edges
        : hit_test_resize_edges(window_width, window_height, cursor_x, cursor_y);

    GLFWcursor* cursor = cursors_.arrow;

    if ((resize_edges & resize_left) != 0 && (resize_edges & resize_top) != 0) {
        cursor = cursors_.resize_nwse;
    } else if ((resize_edges & resize_right) != 0 && (resize_edges & resize_bottom) != 0) {
        cursor = cursors_.resize_nwse;
    } else if ((resize_edges & resize_right) != 0 && (resize_edges & resize_top) != 0) {
        cursor = cursors_.resize_nesw;
    } else if ((resize_edges & resize_left) != 0 && (resize_edges & resize_bottom) != 0) {
        cursor = cursors_.resize_nesw;
    } else if ((resize_edges & (resize_left | resize_right)) != 0) {
        cursor = cursors_.resize_horizontal;
    } else if ((resize_edges & (resize_top | resize_bottom)) != 0) {
        cursor = cursors_.resize_vertical;
    }

    glfwSetCursor(window_, cursor);
}

void WindowFrame::begin_interaction(InteractionMode mode, int resize_edges)
{
    interaction_.mode = mode;
    interaction_.resize_edges = resize_edges;
    interaction_.start_cursor = cursor_screen_position();
    glfwGetWindowPos(window_, &interaction_.start_window_x, &interaction_.start_window_y);
    glfwGetWindowSize(window_, &interaction_.start_window_width, &interaction_.start_window_height);
}

void WindowFrame::update_interaction()
{
    const bool mouse_down = left_mouse_down();

    if (!mouse_down) {
        interaction_.mode = InteractionMode::none;
        interaction_.resize_edges = resize_none;
        interaction_.was_left_mouse_down = false;
        return;
    }

    double cursor_x = 0.0;
    double cursor_y = 0.0;
    int window_width = 0;
    int window_height = 0;
    glfwGetCursorPos(window_, &cursor_x, &cursor_y);
    glfwGetWindowSize(window_, &window_width, &window_height);

    if (!interaction_.was_left_mouse_down && interaction_.mode == InteractionMode::none) {
        const int resize_edges = hit_test_resize_edges(window_width, window_height, cursor_x, cursor_y);

        if (resize_edges != resize_none) {
            begin_interaction(InteractionMode::resize, resize_edges);
        } else if (is_title_drag_zone(window_width, cursor_x, cursor_y)) {
            begin_interaction(InteractionMode::move, resize_none);
        }
    }

    const ScreenPoint cursor = cursor_screen_position();
    const int delta_x = cursor.x - interaction_.start_cursor.x;
    const int delta_y = cursor.y - interaction_.start_cursor.y;

    if (interaction_.mode == InteractionMode::move) {
        set_position(
            interaction_.start_window_x + delta_x,
            interaction_.start_window_y + delta_y
        );
    } else if (interaction_.mode == InteractionMode::resize) {
        int new_x = interaction_.start_window_x;
        int new_y = interaction_.start_window_y;
        int new_width = interaction_.start_window_width;
        int new_height = interaction_.start_window_height;

        if ((interaction_.resize_edges & resize_left) != 0) {
            new_width = std::max(min_width, interaction_.start_window_width - delta_x);
            new_x = interaction_.start_window_x + interaction_.start_window_width - new_width;
        } else if ((interaction_.resize_edges & resize_right) != 0) {
            new_width = std::max(min_width, interaction_.start_window_width + delta_x);
        }

        if ((interaction_.resize_edges & resize_top) != 0) {
            new_height = std::max(min_height, interaction_.start_window_height - delta_y);
            new_y = interaction_.start_window_y + interaction_.start_window_height - new_height;
        } else if ((interaction_.resize_edges & resize_bottom) != 0) {
            new_height = std::max(min_height, interaction_.start_window_height + delta_y);
        }

        set_bounds(new_x, new_y, new_width, new_height);
    }

    interaction_.was_left_mouse_down = true;
}

void WindowFrame::update()
{
    update_interaction();
    update_cursor();
}

bool WindowFrame::draw_title_bar(bool dark_mode)
{
    ImGui::BeginChild(
        "title_bar",
        ImVec2(0.0F, title_bar_height),
        false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    const ImVec2 max = ImVec2(min.x + width, min.y + title_bar_height);
    const ImU32 title_bg = dark_mode ? IM_COL32(20, 23, 27, 255) : IM_COL32(235, 238, 242, 255);
    const ImU32 title_text = dark_mode ? IM_COL32(236, 240, 245, 255) : IM_COL32(26, 31, 38, 255);
    const float control_y = (title_bar_height - title_button_size) * 0.5F;
    bool theme_toggled = false;

    draw_list->AddRectFilled(min, max, title_bg);
    draw_list->AddText(ImVec2(min.x + 18.0F, min.y + control_y + 3.0F), title_text, "codewizard");

    const float controls_width = title_controls_width();
    const float drag_width = width > controls_width ? width - controls_width : 0.0F;

    ImGui::SetCursorPos(ImVec2(0.0F, 0.0F));
    ImGui::InvisibleButton("##title_drag", ImVec2(drag_width, title_bar_height));

    ImGui::SameLine(0.0F, title_button_spacing);
    ImGui::SetCursorPosY(control_y);
    if (ImGui::Button(dark_mode ? "light" : "dark", ImVec2(theme_button_width, title_button_size))) {
        theme_toggled = true;
    }

    ImGui::SameLine(0.0F, title_button_spacing);
    ImGui::SetCursorPosY(control_y);
    if (ImGui::Button("-", ImVec2(title_button_size, title_button_size))) {
        glfwIconifyWindow(window_);
    }

    ImGui::SameLine(0.0F, title_button_spacing);
    ImGui::SetCursorPosY(control_y);
    if (ImGui::Button("X", ImVec2(title_button_size, title_button_size))) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

    ImGui::EndChild();
    return theme_toggled;
}

void WindowFrame::destroy_cursors()
{
    if (cursors_.arrow != nullptr) {
        glfwDestroyCursor(cursors_.arrow);
        cursors_.arrow = nullptr;
    }
    if (cursors_.resize_horizontal != nullptr) {
        glfwDestroyCursor(cursors_.resize_horizontal);
        cursors_.resize_horizontal = nullptr;
    }
    if (cursors_.resize_vertical != nullptr) {
        glfwDestroyCursor(cursors_.resize_vertical);
        cursors_.resize_vertical = nullptr;
    }
    if (cursors_.resize_nwse != nullptr) {
        glfwDestroyCursor(cursors_.resize_nwse);
        cursors_.resize_nwse = nullptr;
    }
    if (cursors_.resize_nesw != nullptr) {
        glfwDestroyCursor(cursors_.resize_nesw);
        cursors_.resize_nesw = nullptr;
    }
}

} // namespace codewizard::app
