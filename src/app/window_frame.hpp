#pragma once

#include <GLFW/glfw3.h>

namespace codewizard::app {

class WindowFrame {
public:
    static constexpr int min_width = 820;
    static constexpr int min_height = 560;

    explicit WindowFrame(GLFWwindow* window);
    ~WindowFrame();

    WindowFrame(const WindowFrame&) = delete;
    WindowFrame& operator=(const WindowFrame&) = delete;

    void update();
    bool draw_title_bar(bool dark_mode);

private:
    enum ResizeEdge {
        resize_none = 0,
        resize_left = 1 << 0,
        resize_right = 1 << 1,
        resize_top = 1 << 2,
        resize_bottom = 1 << 3
    };

    enum class InteractionMode {
        none,
        move,
        resize
    };

    struct ScreenPoint {
        int x = 0;
        int y = 0;
    };

    struct InteractionState {
        InteractionMode mode = InteractionMode::none;
        int resize_edges = resize_none;
        bool was_left_mouse_down = false;
        ScreenPoint start_cursor;
        int start_window_x = 0;
        int start_window_y = 0;
        int start_window_width = 0;
        int start_window_height = 0;
    };

    struct Cursors {
        GLFWcursor* arrow = nullptr;
        GLFWcursor* resize_horizontal = nullptr;
        GLFWcursor* resize_vertical = nullptr;
        GLFWcursor* resize_nwse = nullptr;
        GLFWcursor* resize_nesw = nullptr;
    };

    ScreenPoint cursor_screen_position() const;
    bool left_mouse_down() const;
    void set_position(int x, int y) const;
    void set_bounds(int x, int y, int width, int height) const;
    int hit_test_resize_edges(int window_width, int window_height, double cursor_x, double cursor_y) const;
    bool is_title_drag_zone(int window_width, double cursor_x, double cursor_y) const;
    void update_cursor();
    void begin_interaction(InteractionMode mode, int resize_edges);
    void update_interaction();
    void destroy_cursors();

    GLFWwindow* window_ = nullptr;
    InteractionState interaction_;
    Cursors cursors_;
};

} // namespace codewizard::app
