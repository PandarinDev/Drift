#include "window_manager.h"

#include <iostream>
#include <stdexcept>

using namespace drift;

int main(int argc, char** argv) {
    try {
        WindowManager window_manager;
        window_manager.start();
    } catch (const std::exception& e) {
        std::cerr << "Critical error happened: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}