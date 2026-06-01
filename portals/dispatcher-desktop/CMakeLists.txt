cmake_minimum_required(VERSION 3.16)
project(fleet_dispatcher_desktop CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Wt — the desktop/web UI framework. Provides Wt::Wt and Wt::HTTP.
find_package(Wt REQUIRED COMPONENTS Wt HTTP)

add_executable(fleet_dispatcher_desktop
    src/main.cpp
)

target_link_libraries(fleet_dispatcher_desktop
    PRIVATE Wt::Wt Wt::HTTP
)
