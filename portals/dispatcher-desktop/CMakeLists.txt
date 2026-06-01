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

# ---------------------------------------------------------------------------
# Wt resources/ deployment
#
# Bootstrap (and every other Wt theme) renders correctly only when the whole
# Wt `resources/` tree — CSS, JS, themes, images — is reachable under the
# docroot at runtime (Wt serves it from <docroot>/resources). We therefore copy
# the entire folder next to the executable on every build, and install it
# alongside the binary.
#
# Override the source location if Wt is in a non-standard prefix:
#   cmake -S . -B build -DWT_RESOURCES_DIR=/path/to/Wt/resources
# ---------------------------------------------------------------------------
find_path(WT_RESOURCES_DIR
    NAMES jquery.min.js themes/bootstrap
    DOC "Path to the Wt resources/ directory (CSS, JS, themes) to deploy with the app"
    HINTS
        ENV WT_RESOURCES_DIR
    PATHS
        /usr/share/Wt/resources
        /usr/local/share/Wt/resources
        /opt/Wt/share/Wt/resources
        /opt/local/share/Wt/resources
        "${CMAKE_INSTALL_PREFIX}/share/Wt/resources"
)

# Where the app expects its static files (docroot) at runtime: the directory
# that holds the executable. Resources land in <exe dir>/resources.
set(_run_dir "$<TARGET_FILE_DIR:fleet_dispatcher_desktop>")

if(WT_RESOURCES_DIR)
    message(STATUS "Wt resources: ${WT_RESOURCES_DIR} -> ${_run_dir}/resources")
    add_custom_command(TARGET fleet_dispatcher_desktop POST_BUILD
        COMMENT "Deploying Wt resources/ and app docroot next to the executable"
        # The entire Wt resources tree (themes/bootstrap, jquery, etc.).
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${WT_RESOURCES_DIR}" "${_run_dir}/resources"
        # The app's own docroot (custom CSS, future static assets).
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/docroot" "${_run_dir}"
        VERBATIM
    )
else()
    message(WARNING
        "Wt resources/ folder not found. Bootstrap styles will NOT render.\n"
        "  Set it explicitly, e.g.:\n"
        "    cmake -S . -B build -DWT_RESOURCES_DIR=/usr/share/Wt/resources")
endif()

# ---------------------------------------------------------------------------
# Install layout: <prefix>/bin/fleet_dispatcher_desktop with resources/ and the
# docroot deployed beside it, so the installed app renders identically.
# ---------------------------------------------------------------------------
install(TARGETS fleet_dispatcher_desktop RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docroot/" DESTINATION bin)
if(WT_RESOURCES_DIR)
    install(DIRECTORY "${WT_RESOURCES_DIR}/" DESTINATION bin/resources)
endif()
