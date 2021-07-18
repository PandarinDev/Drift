#include "window_manager.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace drift {

    WindowManager::WindowManager() {
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
            // TODO: No idea why we have to mask 0x80, 
            const auto event_type = event->response_type & ~0x80;
            std::cout << "Received event of type: " << std::to_string(event_type) << std::endl;
            switch (event_type) {
                case XCB_CREATE_NOTIFY:
                    handle_create_notify(reinterpret_cast<xcb_create_notify_event_t*>(event));
                    break;
                case XCB_MAP_REQUEST:
                    handle_map_request(reinterpret_cast<xcb_map_request_event_t*>(event));
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
        xcb_grab_button(connection, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 1, XCB_MOD_MASK_1);
        xcb_flush(connection);
    }

    void WindowManager::handle_create_notify(xcb_create_notify_event_t* event) {
        // Ignore the window if it's a frame created by us
        const auto it = frame_windows.find(event->window);
        if (it != frame_windows.end()) {
            return;
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
        xcb_create_window(
            connection, XCB_COPY_FROM_PARENT, window, screen->root,
            geometry_reply->x, geometry_reply->y, geometry_reply->width,
            geometry_reply->height, 3, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
            screen->root_visual, 0, nullptr);
        xcb_reparent_window(connection, event->window, window, 0, 10);
        const auto context_id = xcb_generate_id(connection);
        std::uint32_t values[] = { screen->white_pixel, 0 };
        xcb_create_gc(connection, context_id, window, XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES, values);
        xcb_rectangle_t rectangle;
        rectangle.x = geometry_reply->x;
        rectangle.y = geometry_reply->y;
        rectangle.width = geometry_reply->width;
        rectangle.height = 10;
        xcb_poly_fill_rectangle(connection, window, context_id, 1, &rectangle);
        xcb_map_window(connection, window);
        frame_windows.emplace(window);
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
        std::uint32_t values[] = { size_reply->x, size_reply->y, width, height, 3 };
        xcb_configure_window(connection, event->window, value_mask, values);
        xcb_flush(connection);
        std::uint32_t event_masks = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
        xcb_change_window_attributes(connection, event->window, XCB_CW_EVENT_MASK, &event_masks);
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, event->window, XCB_CURRENT_TIME);
    }

    void WindowManager::handle_button_release(xcb_button_release_event_t* event) const {
        const auto cookie = xcb_query_pointer(connection, screen->root);
        xcb_generic_error_t* error = nullptr;
        const auto pointer_reply = xcb_query_pointer_reply(connection, cookie, &error);
        if (error != nullptr) {
            free(error);
            throw std::runtime_error("Failed to query pointer.");
        }
        // Assign focus to the window where the pointer was released
        const auto target = pointer_reply->child != XCB_NONE
            ? pointer_reply->child
            : screen->root;
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, target, XCB_CURRENT_TIME);
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
