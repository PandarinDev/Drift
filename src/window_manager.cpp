#include "window_manager.h"

#include <optional>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace drift {

    WindowManager::WindowManager() : grabbed_window(std::nullopt) {
        connection = xcb_connect(nullptr, nullptr);
        if (xcb_connection_has_error(connection)) {
            throw std::runtime_error("Failed to connect to Xorg.");
        }
        const auto setup = xcb_get_setup(connection);
        screen = xcb_setup_roots_iterator(setup).data;
    }

    WindowManager::~WindowManager() {
        xcb_disconnect(connection);
    }

    void WindowManager::start() {
        configure();
        while (true) {
            if (xcb_connection_has_error(connection)) {
                throw std::runtime_error("Connection error happened.");
            }
            const auto event = xcb_wait_for_event(connection);
            if (!event) {
                std::cerr << "Empty event was received." << std::endl;
                continue;
            }
            if (event->response_type == 0) {
                std::cerr << "An error event was received." << std::endl;
                continue;
            }
            // TODO: No idea why we have to mask 0x80
            const auto event_type = event->response_type & ~0x80;
            switch (event_type) {
                case XCB_CREATE_NOTIFY:
                    handle_create_notify(reinterpret_cast<xcb_create_notify_event_t*>(event));
                    break;
                case XCB_MOTION_NOTIFY:
                    handle_motion_notify(reinterpret_cast<xcb_motion_notify_event_t*>(event));
                    break;
                case XCB_DESTROY_NOTIFY:
                    handle_destroy_notify(reinterpret_cast<xcb_destroy_notify_event_t*>(event));
                    break;
                case XCB_MAP_REQUEST:
                    handle_map_request(reinterpret_cast<xcb_map_request_event_t*>(event));
                    break;
                case XCB_BUTTON_PRESS:
                    handle_button_press(reinterpret_cast<xcb_button_press_event_t*>(event));
                    break;
                case XCB_BUTTON_RELEASE:
                    handle_button_release(reinterpret_cast<xcb_button_release_event_t*>(event));
                    break;
                case XCB_FOCUS_IN:
                    handle_focus_in(reinterpret_cast<xcb_focus_in_event_t*>(event));
                    break;
                case XCB_FOCUS_OUT:
                    handle_focus_out(reinterpret_cast<xcb_focus_out_event_t*>(event));
                    break;
            }

            // Always flush after processing an event
            xcb_flush(connection);
        }
    }

    void WindowManager::configure() const {
        // Subscribe to window events
        std::uint32_t values =
            XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
            XCB_EVENT_MASK_PROPERTY_CHANGE |
            XCB_EVENT_MASK_FOCUS_CHANGE;
        const auto window_attribute_cookie = xcb_change_window_attributes_checked(
            connection, screen->root, XCB_CW_EVENT_MASK, &values);
        xcb_generic_error_t* error = nullptr;
        if (error = xcb_request_check(connection, window_attribute_cookie)) {
            free(error);
            throw std::runtime_error("Failed to subscribe to window events. Is a window manager already running?");
        }

        // Subscribe to left-click events
        xcb_grab_button(
            connection, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, XCB_BUTTON_INDEX_1, XCB_MOD_MASK_ANY);

        // Subscribe to right-click events
        xcb_grab_button(
            connection, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, XCB_BUTTON_INDEX_3, XCB_MOD_MASK_ANY);
        xcb_flush(connection);
    }

    void WindowManager::handle_create_notify(xcb_create_notify_event_t* event) {
        // Create a frame for new windows
        // Note that this method will also be called for frames created
        // by us, so the first thing we do is check if this is a frame.
        for (const auto& entry : frame_windows) {
            if (entry.second == event->window) {
                return;
            }
        }

        // Otherwise create a frame window and reparent the original
        const auto window = xcb_generate_id(connection);
        const auto geometry_cookie = xcb_get_geometry(connection, event->window);
        xcb_generic_error_t* error = nullptr;
        const auto geometry_reply = xcb_get_geometry_reply(connection, geometry_cookie, &error);
        if (error) {
            free(error);
            throw std::runtime_error("Failed to query new window geometry");
        }
        constexpr auto border_width = 3;
        constexpr auto window_masks = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        std::array<std::uint32_t, 2> window_values { screen->white_pixel };
        xcb_create_window(
            connection, XCB_COPY_FROM_PARENT, window, screen->root,
            geometry_reply->x, geometry_reply->y, geometry_reply->width,
            geometry_reply->height, border_width, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
            screen->root_visual, window_masks, window_values.data());
        constexpr auto frame_offset_vertical = 10;
        xcb_reparent_window(connection, event->window, window, 0, frame_offset_vertical);
        xcb_map_window(connection, window);
        frame_windows.emplace(event->window, window);
    }

    void WindowManager::handle_destroy_notify(xcb_destroy_notify_event_t* event) {
        // If a child window has been destroyed destroy it's parent too
        const auto it = frame_windows.find(event->window);
        if (it == frame_windows.end()) {
            return;
        }
        xcb_destroy_window(connection, it->second);
    }

    void WindowManager::handle_motion_notify(xcb_motion_notify_event_t* event) {
        if (!grabbed_window) {
            return;
        }

        auto delta_x = event->root_x - last_x;
        auto delta_y = event->root_y - last_y;

        last_x = event->root_x;
        last_y = event->root_y;

        const auto geometry_cookie = xcb_get_geometry(connection, *grabbed_window);
        xcb_generic_error_t* error = nullptr;
        const auto geometry_reply = xcb_get_geometry_reply(connection, geometry_cookie, &error);
        if (error) {
            free(error);
            throw std::runtime_error("Failed to query new window geometry");
        }
        const auto new_x = static_cast<std::uint32_t>(std::max(geometry_reply->x + delta_x, 0));
        const auto new_y = static_cast<std::uint32_t>(std::max(geometry_reply->y + delta_y, 0));

        std::uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
        std::array<std::uint32_t, 2> values { new_x, new_y };
        xcb_configure_window(connection, *grabbed_window, mask, values.data());
    }

    void WindowManager::handle_map_request(xcb_map_request_event_t* event) const {
        // Query the proposed size of the window
        std::uint32_t width, height;
        const auto size_cookie = xcb_get_geometry(connection, event->window);
        xcb_generic_error_t* error = nullptr;
        const auto size_reply = xcb_get_geometry_reply(connection, size_cookie, &error);
        if (error) {
            free(error);
            throw std::runtime_error("Failed to query size of mapped window.");
        }
        width = size_reply->width;
        height = size_reply->height;
        xcb_map_window(connection, event->window);
        // Note: Value mask entries are required to be in ascending order of enum value.
        std::uint16_t value_mask =
            XCB_CONFIG_WINDOW_X |
            XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT |
            XCB_CONFIG_WINDOW_BORDER_WIDTH;
        // Note: Values need to be in the same order as value masks. All values are in pixels.
        std::array<std::uint32_t, 5> values {
            static_cast<std::uint32_t>(size_reply->x),
            static_cast<std::uint32_t>(size_reply->y),
            width, height, 3
        };
        xcb_configure_window(connection, event->window, value_mask, values.data());
        xcb_flush(connection);
        std::uint32_t event_masks = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
        xcb_change_window_attributes(connection, event->window, XCB_CW_EVENT_MASK, &event_masks);
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, event->window, XCB_CURRENT_TIME);
    }

    void WindowManager::handle_button_press(xcb_button_press_event_t* event) {
        // Destroy windows on right click
        if (event->detail == XCB_BUTTON_INDEX_3 && event->child != XCB_NONE) {
            xcb_destroy_window(connection, event->child);
            return;
        }

        // Otherwise we only care about left clicks
        if (event->detail != XCB_BUTTON_INDEX_1) {
            return;
        }

        const auto cookie = xcb_query_pointer(connection, screen->root);
        xcb_generic_error_t* error = nullptr;
        const auto pointer_reply = xcb_query_pointer_reply(connection, cookie, &error);
        if (error != nullptr) {
            free(error);
            throw std::runtime_error("Failed to query pointer.");
        }
        const auto target = pointer_reply->child != XCB_NONE
            ? pointer_reply->child
            : screen->root;
        // If the target is a frame grab the pointer
        for (const auto& entry : frame_windows) {
            if (entry.second == target) {
                grabbed_window = target;
                xcb_grab_pointer(
                    connection, 0, screen->root, XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, XCB_CURRENT_TIME);
                last_x = pointer_reply->root_x;
                last_y = pointer_reply->root_y;
                return;
            }
        }
    }

    void WindowManager::handle_button_release(xcb_button_release_event_t* event) {
        grabbed_window = std::nullopt;
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
    }

    void WindowManager::handle_focus_in(xcb_focus_in_event_t* event) const {
        const auto window = event->event;
        std::uint32_t border_color = 0xffffff;
        xcb_change_window_attributes(connection, window, XCB_CW_BORDER_PIXEL, &border_color);
        std::uint32_t stack_mode = XCB_STACK_MODE_ABOVE;
        xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
    }

    void WindowManager::handle_focus_out(xcb_focus_out_event_t* event) const {
        const auto window = event->event;
        std::uint32_t border_color = 0xcccccc;
        xcb_change_window_attributes(connection, window, XCB_CW_BORDER_PIXEL, &border_color);
        std::uint32_t stack_mode = XCB_STACK_MODE_BELOW;
        xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
    }

}
