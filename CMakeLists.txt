cmake_minimum_required(VERSION 3.12)
project(Drift)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE DRIFT_SOURCES "src/*.cpp")
file(GLOB_RECURSE DRIFT_HEADERS "include/*.h")

add_executable(Drift ${DRIFT_SOURCES} ${DRIFT_HEADERS})
target_link_libraries(Drift PRIVATE xcb)
target_include_directories(Drift PRIVATE "include")