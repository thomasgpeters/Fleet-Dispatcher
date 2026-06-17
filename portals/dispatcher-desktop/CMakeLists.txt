cmake_minimum_required(VERSION 3.16)
project(fleet_dispatcher_desktop CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Wt — the desktop/web UI framework. Provides Wt::Wt and Wt::HTTP.
find_package(Wt REQUIRED COMPONENTS Wt HTTP)

add_executable(fleet_dispatcher_desktop
    src/main.cpp
    src/Shell.cpp
    src/BoardView.cpp
    src/LoadForm.cpp
    src/HudView.cpp
    src/HudControlBus.cpp
    src/ApiClient.cpp
    src/LoginView.cpp
    src/ProfileView.cpp
    src/CommPanel.cpp
    src/CommBus.cpp
    src/Toaster.cpp
    src/FleetView.cpp
    src/MapView.cpp
    src/SettingsView.cpp
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

# ---------------------------------------------------------------------------
# Mobile module publishing (git subtree)
#
# Exposes the monorepo's `make publish-mobile` to CMake so the mobile app
# (portals/mobile) can be published to the Fleet-Dispatcher-Mobile repo from the
# build:
#
#   * On demand:   cmake --build build --target publish-mobile
#   * As a step:   configure with -DFLEET_PUBLISH_MOBILE=ON to run it
#                  POST_BUILD, after the desktop binary is linked.
#
# Publishing pushes to a remote (outward-facing), so the POST_BUILD step is OFF
# by default. Forward the remote/branch with -DMOBILE_REMOTE_URL / -DMOBILE_BRANCH.
# Gracefully no-ops if built outside the monorepo (no top-level Makefile).
# ---------------------------------------------------------------------------
get_filename_component(FLEET_MONOREPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

option(FLEET_PUBLISH_MOBILE "Run 'make publish-mobile' as a POST_BUILD step" OFF)
set(MOBILE_REMOTE_URL "" CACHE STRING "git URL of Fleet-Dispatcher-Mobile (forwarded to make publish-mobile)")
set(MOBILE_BRANCH "main" CACHE STRING "Target branch for the mobile subtree publish")

find_program(MAKE_EXECUTABLE NAMES make gmake)

if(EXISTS "${FLEET_MONOREPO_ROOT}/Makefile" AND MAKE_EXECUTABLE)
    set(_publish_cmd ${MAKE_EXECUTABLE} -C "${FLEET_MONOREPO_ROOT}" publish-mobile "MOBILE_BRANCH=${MOBILE_BRANCH}")
    if(MOBILE_REMOTE_URL)
        list(APPEND _publish_cmd "MOBILE_REMOTE_URL=${MOBILE_REMOTE_URL}")
    endif()

    # On-demand target.
    add_custom_target(publish-mobile
        COMMAND ${_publish_cmd}
        WORKING_DIRECTORY "${FLEET_MONOREPO_ROOT}"
        COMMENT "Publishing portals/mobile -> Fleet-Dispatcher-Mobile (git subtree)"
        VERBATIM
        USES_TERMINAL
    )

    # Optional automatic step after the desktop build.
    if(FLEET_PUBLISH_MOBILE)
        message(STATUS "Mobile publish: will run 'make publish-mobile' POST_BUILD")
        add_custom_command(TARGET fleet_dispatcher_desktop POST_BUILD
            COMMAND ${_publish_cmd}
            WORKING_DIRECTORY "${FLEET_MONOREPO_ROOT}"
            COMMENT "POST_BUILD: publishing mobile module via git subtree"
            VERBATIM
        )
    endif()
else()
    message(STATUS
        "Mobile publish target unavailable (no Makefile at ${FLEET_MONOREPO_ROOT} "
        "or 'make' not found) — expected when building the desktop app standalone.")
endif()
