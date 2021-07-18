#pragma once

#include <cstdint>
#include <unordered_set>

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
        std::unordered_set<std::uint32_t> frame_windows;

        void configure() const;

        void handle_create_notify(xcb_create_notify_event_t* event);
        void handle_map_request(xcb_map_request_event_t* event) const;
        void handle_button_release(xcb_button_release_event_t* event) const;
        void handle_focus_in(xcb_focus_in_event_t* event) const;
        void handle_focus_out(xcb_focus_out_event_t* event) const;

    };

}
