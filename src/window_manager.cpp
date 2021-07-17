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

    void WindowManager::handle_map_request(xcb_map_request_event_t* event) const {
        xcb_map_window(connection, event->window);
        // Note: Value mask entries are required to be in ascending order of enum value.
        std::uint16_t value_mask =
            XCB_CONFIG_WINDOW_X |
            XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT |
            XCB_CONFIG_WINDOW_BORDER_WIDTH;
        // Note: Values need to be in the same order as value masks. All values are in pixels.
        std::uint32_t values[] = {
            (screen->width_in_pixels / 2) - (300 / 2),  // X position of window
            (screen->height_in_pixels / 2) - (200 / 2), // Y position of window
            300,                                        // Width of window
            200,                                        // Height of window
            3                                           // Border width
        };
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
        std::cout << "Changing window focus to light" << std::endl;
    }

    void WindowManager::handle_focus_out(xcb_focus_out_event_t* event) const {
        const auto window = event->event;
        std::uint32_t border_color = 0xcccccc;
        xcb_change_window_attributes(connection, window, XCB_CW_BORDER_PIXEL, &border_color);
        std::cout << "Changing window focus to dark" << std::endl;
    }

}
