#include "wayland.hpp"

#include <cstring>
#include <iostream>
#include <sys/mman.h>

void handler_registry_global(void* data, wl_registry* registry, uint32_t name, char const* interface, [[maybe_unused]] uint32_t version)
{
    auto* window = static_cast<Window*>(data);
    // printf("interface: '%s', version: %u, name: %u\n", interface, version, name);

    if (std::strcmp(interface, "wl_compositor") == 0) {
        window->compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 4));
    } else if (std::strcmp(interface, "wl_shm") == 0) {
        window->shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, "wl_seat") == 0) {
        window->seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
    } else if (std::strcmp(interface, "xdg_wm_base") == 0) {
        window->wm_base = static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
    } else if (std::strcmp(interface, "zxdg_decoration_manager_v1") == 0) {
        window->zxdg_decoration_manager = static_cast<zxdg_decoration_manager_v1*>(wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
    }
}

void handler_registry_global_remove(void*, wl_registry*, uint32_t name)
{
    printf("removed: %u\n", name);
}

auto registry_listener = wl_registry_listener{
    .global = handler_registry_global,
    .global_remove = handler_registry_global_remove,
};

void handler_xdg_wm_base_ping(void*, xdg_wm_base* wm_base, uint32_t serial)
{
    xdg_wm_base_pong(wm_base, serial);
}

auto const base_listener = xdg_wm_base_listener{
    .ping = handler_xdg_wm_base_ping,
};

void handler_xdg_surface_configure(void* data, xdg_surface* surface, uint32_t serial)
{
    auto* window = static_cast<Window*>(data);

    xdg_surface_ack_configure(surface, serial);

    window->is_configured = true;
}

auto const surface_listener = xdg_surface_listener{
    .configure = handler_xdg_surface_configure,
};

void handler_xdg_toplevel_configure(void* data, xdg_toplevel*, int32_t width, int32_t height, wl_array*)
{
    auto* window = static_cast<Window*>(data);

    window->width = width;
    window->height = height;

    if (window->width == 0) {
        window->width = window->initial_width;
    }

    if (window->height == 0) {
        window->height = window->initial_height;
    }

    if (window->mapped_data) {
        munmap(window->mapped_data, window->mapped_data_size);
    }

    int stride = window->width * 4;
    window->mapped_data_size = stride * window->height;

    // Resize fd and remap data
    if (ftruncate(window->fd, window->mapped_data_size) != 0) {
        perror("ftruncate failed");
        std::abort();
    }
    window->mapped_data = static_cast<uint32_t*>(mmap(NULL, window->mapped_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, window->fd, 0));

    // Recreate wl_buffer
    auto* shm_pool = wl_shm_create_pool(window->shm, window->fd, window->mapped_data_size);

    window->buffer = wl_shm_pool_create_buffer(shm_pool, 0, window->width, window->height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(shm_pool);

    if (window->callback_window_resize) {
        window->callback_window_resize(width, height);
    }
}

void handler_xdg_toplevel_close(void* data, xdg_toplevel*)
{
    auto* window = static_cast<Window*>(data);
    window->is_open = false;
}

void handler_xdg_toplevel_configure_bounds(void*, xdg_toplevel*, int, int) { }

void handler_xdg_toplevel_wm_capabilities(void*, xdg_toplevel*, wl_array*) { }

auto const toplevel_listener = xdg_toplevel_listener{
    .configure = handler_xdg_toplevel_configure,
    .close = handler_xdg_toplevel_close,
    .configure_bounds = handler_xdg_toplevel_configure_bounds,
    .wm_capabilities = handler_xdg_toplevel_wm_capabilities,
};

void handler_pointer_enter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface*, wl_fixed_t, wl_fixed_t)
{
    auto* window = static_cast<Window*>(data);
    wl_pointer_set_cursor(pointer, serial, window->cursor_surface, window->cursor_image->hotspot_x, window->cursor_image->hotspot_y);
}

void handler_pointer_leave(void*, wl_pointer*, uint32_t, wl_surface*) { }

void handler_pointer_motion(void* data, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y)
{
    auto* window = static_cast<Window*>(data);
    if (window->callback_pointer_motion) {
        window->callback_pointer_motion(x, y);
    }
}

void handler_pointer_button(void* data, wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t state)
{
    auto* window = static_cast<Window*>(data);
    if (window->callback_pointer_button) {
        window->callback_pointer_button(button, static_cast<wl_pointer_button_state>(state));
    }
}

void handler_pointer_axis(void* data, wl_pointer*, uint32_t, uint32_t axis, wl_fixed_t value)
{
    auto* window = static_cast<Window*>(data);
    if (window->callback_pointer_axis) {
        window->callback_pointer_axis(static_cast<wl_pointer_axis>(axis), value);
    }
}

void handler_pointer_frame(void*, wl_pointer*) { }
void handler_pointer_axis_source(void*, wl_pointer*, uint32_t) { }
void handler_pointer_axis_stop(void*, wl_pointer*, uint32_t, uint32_t) { }
void handler_pointer_axis_discrete(void*, wl_pointer*, uint32_t, int32_t) { }
void handler_pointer_axis_value120(void*, wl_pointer*, uint32_t, int32_t) { }
void handler_pointer_axis_relative_direction(void*, wl_pointer*, uint32_t, uint32_t) { }

auto const pointer_listener = wl_pointer_listener{
    .enter = handler_pointer_enter,
    .leave = handler_pointer_leave,
    .motion = handler_pointer_motion,
    .button = handler_pointer_button,
    .axis = handler_pointer_axis,
    .frame = handler_pointer_frame,
    .axis_source = handler_pointer_axis_source,
    .axis_stop = handler_pointer_axis_stop,
    .axis_discrete = handler_pointer_axis_discrete,
    .axis_value120 = handler_pointer_axis_value120,
    .axis_relative_direction = handler_pointer_axis_relative_direction,
};

void handler_keyboard_enter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) { }

void handler_keyboard_leave(void*, wl_keyboard*, uint32_t, wl_surface*) { }

void handler_keyboard_key(void* data, wl_keyboard*, uint32_t, uint32_t, uint32_t key, uint32_t state)
{
    auto* window = static_cast<Window*>(data);
    if (window->callback_keyboard_key) {
        window->callback_keyboard_key(static_cast<Scancodes>(key), static_cast<wl_keyboard_key_state>(state));
    }
}

void handler_keyboard_keymap(void*, wl_keyboard*, uint32_t, int, uint32_t) { }

void handler_keyboard_modifiers(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) { }

void handler_keyboard_repeat_info(void*, wl_keyboard*, int32_t, int32_t) { }

auto const keyboard_listener = wl_keyboard_listener{
    .keymap = handler_keyboard_keymap,
    .enter = handler_keyboard_enter,
    .leave = handler_keyboard_leave,
    .key = handler_keyboard_key,
    .modifiers = handler_keyboard_modifiers,
    .repeat_info = handler_keyboard_repeat_info,
};

void handler_surface_frame_done(void*, wl_callback*, uint32_t);

auto const surface_frame_listener = wl_callback_listener{
    .done = handler_surface_frame_done,
};

void handler_surface_frame_done(void* data, wl_callback* cb, uint32_t time)
{
    /* Destroy this callback */
    if (cb) {
        wl_callback_destroy(cb);
    }

    /* Request another frame */
    auto* window = static_cast<Window*>(data);
    cb = wl_surface_frame(window->surface);
    wl_callback_add_listener(cb, &surface_frame_listener, window);

    if (!window->callback_draw) {
        std::abort();
    }
    window->callback_draw(window->mapped_data, window->width, window->height, time);

    /* Submit a frame for this event */
    wl_surface_attach(window->surface, window->buffer, 0, 0);
    wl_surface_damage_buffer(window->surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(window->surface);

    window->last_frame = time;
}

std::unique_ptr<Window> Window::open(char const* title, int width, int height)
{
    auto window = std::make_unique<Window>();
    window->initial_width = width;
    window->initial_height = height;

    window->display = wl_display_connect(nullptr);
    window->registry = wl_display_get_registry(window->display);

    wl_registry_add_listener(window->registry, &registry_listener, window.get());

    // wait for the "initial" set of globals to appear
    wl_display_roundtrip(window->display);

    if (!window->compositor) {
        std::cout << "Missing wl_compositor\n";
        std::abort();
    }

    if (!window->shm) {
        std::cout << "Missing wl_shm\n";
        std::abort();
    }

    if (!window->seat) {
        std::cout << "Missing wl_seat\n";
        std::abort();
    }

    if (!window->wm_base) {
        std::cout << "Missing xdg_wm_base\n";
        std::abort();
    }

    if (!window->zxdg_decoration_manager) {
        std::cout << "Missing zxdg_decoration_manager_v1";
        std::abort();
    }

    xdg_wm_base_add_listener(window->wm_base, &base_listener, window.get());

    window->surface = wl_compositor_create_surface(window->compositor);

    window->xdg_surface = xdg_wm_base_get_xdg_surface(window->wm_base, window->surface);
    xdg_surface_add_listener(window->xdg_surface, &surface_listener, window.get());

    window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
    xdg_toplevel_set_title(window->xdg_toplevel, title);
    xdg_toplevel_add_listener(window->xdg_toplevel, &toplevel_listener, window.get());

    auto* toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(window->zxdg_decoration_manager, window->xdg_toplevel);
    zxdg_toplevel_decoration_v1_set_mode(toplevel_decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

    wl_surface_commit(window->surface);

    auto* pointer = wl_seat_get_pointer(window->seat);
    wl_pointer_add_listener(pointer, &pointer_listener, window.get());

    auto* keyboard = wl_seat_get_keyboard(window->seat);
    wl_keyboard_add_listener(keyboard, &keyboard_listener, window.get());

    auto* cursor_theme = wl_cursor_theme_load(nullptr, 24, window->shm);
    auto* cursor = wl_cursor_theme_get_cursor(cursor_theme, "left_ptr");
    window->cursor_image = cursor->images[0];
    auto* cursor_buffer = wl_cursor_image_get_buffer(window->cursor_image);

    window->cursor_surface = wl_compositor_create_surface(window->compositor);
    wl_surface_attach(window->cursor_surface, cursor_buffer, 0, 0);
    wl_surface_commit(window->cursor_surface);

    window->fd = memfd_create("buffer", 0);

    return window;
}

void Window::mainloop()
{
    while (wl_display_dispatch(display) && !is_configured) { }

    handler_surface_frame_done(this, nullptr, 0);

    while (is_open) {
        wl_display_dispatch(display);
    }
}
