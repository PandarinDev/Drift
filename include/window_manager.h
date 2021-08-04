#pragma once

#include <cstdint>
#include <unordered_map>

extern "C" {
    #include <xcb/xcb.h>
}

namespace drift {

    struct WindowManager {

        WindowManager();
        ~WindowManager();

        void start();

    private:

        xcb_connection_t* connection;
        xcb_screen_t* screen;
        std::unordered_map<std::uint32_t, std::uint32_t> frame_windows;

        void configure() const;

        void handle_create_notify(xcb_create_notify_event_t* event);
        void handle_destroy_notify(xcb_destroy_notify_event_t* event);
        void handle_motion_notify(xcb_motion_notify_event_t* event) const;
        void handle_map_request(xcb_map_request_event_t* event) const;
        void handle_button_release(xcb_button_release_event_t* event) const;
        void handle_focus_in(xcb_focus_in_event_t* event) const;
        void handle_focus_out(xcb_focus_out_event_t* event) const;

    };

}
