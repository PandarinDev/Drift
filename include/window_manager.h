#pragma once

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

        void configure() const;

        void handle_map_request(xcb_map_request_event_t* event);
        void handle_button_release(xcb_button_release_event_t* event);

    };

}
