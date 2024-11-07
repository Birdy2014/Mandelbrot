#pragma once

#include "xdg-decoration-unstable-v1.h"
#include "xdg-shell.h"
#include <functional>
#include <linux/input-event-codes.h>
#include <memory>
#include <wayland-client.h>
#include <wayland-cursor.h>

enum class Scancodes {
    I = 23,
    PLUS = 27,
    S = 31,
    H = 35,
    MINUS = 53,
};

struct Window {
    wl_display* display;
    wl_registry* registry;
    wl_compositor* compositor;
    wl_shm* shm;
    wl_seat* seat;
    xdg_wm_base* wm_base;
    wl_surface* cursor_surface;
    wl_cursor_image* cursor_image;
    wl_surface* surface;
    xdg_surface* xdg_surface;
    xdg_toplevel* xdg_toplevel;
    wl_buffer* buffer;
    zxdg_decoration_manager_v1* zxdg_decoration_manager;

    int initial_width;
    int initial_height;
    int fd;
    int width;
    int height;
    bool is_open = true;
    uint32_t last_frame;
    bool is_configured = false;
    uint32_t* mapped_data = nullptr;
    size_t mapped_data_size;

    std::function<void(int width, int height)> callback_window_resize;
    std::function<void(int x, int y)> callback_pointer_motion;
    std::function<void(uint32_t button, wl_pointer_button_state state)> callback_pointer_button;
    std::function<void(wl_pointer_axis axis, int value)> callback_pointer_axis;
    std::function<void(Scancodes scancode, wl_keyboard_key_state state)> callback_keyboard_key;
    std::function<void(uint32_t* data, int width, int height, uint32_t elapsed)> callback_draw;

    static std::unique_ptr<Window> open(char const* title, int width, int height);
    void mainloop();
};
