cmake_minimum_required(VERSION 3.24)

project(harbour VERSION 0.1.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(FeatureSummary)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus Gui GuiPrivate Quick WaylandClient Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS Config GlobalAccel KIO Service)
find_package(LayerShellQt 6.0 REQUIRED)
find_package(PlasmaWaylandProtocols 1.10 REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(src)

find_package(KF6 6.0 REQUIRED COMPONENTS KCMUtils CoreAddons)
add_subdirectory(kcm)

option(HARBOUR_BUILD_KWIN_EFFECT "Build the Genie minimise effect for KWin" ON)
if(HARBOUR_BUILD_KWIN_EFFECT)
    find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons)
    add_subdirectory(kwin-effect)
endif()

# The fake-input pointer driver used by tests/ui/harness.sh; never installed.
option(HARBOUR_BUILD_UI_TESTS "Build the UI test probe (tests/ui)" OFF)
if(HARBOUR_BUILD_UI_TESTS)
    add_subdirectory(tests/ui/edgeprobe)
endif()

install(FILES data/dev.harbour.dock.desktop DESTINATION ${KDE_INSTALL_APPDIR})
# Started with the Plasma session once enabled: systemctl --user enable harbour.service
if(NOT KDE_INSTALL_SYSTEMDUSERUNITDIR)
    set(KDE_INSTALL_SYSTEMDUSERUNITDIR ${CMAKE_INSTALL_PREFIX}/lib/systemd/user)
endif()
install(FILES data/harbour.service DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
