qt_add_executable(harbour
    main.cpp
)

set_source_files_properties(qml/StackMenus.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Loaded into KWin at run time (DockManager), from a copy in the runtime directory.
qt_add_resources(harbour "kwinscripts" PREFIX "/harbour" FILES cursorscreen.js)

qt_add_qml_module(harbour
    URI Harbour
    VERSION 1.0
    QML_FILES
        qml/Dock.qml
        qml/DockBackground.qml
        qml/DockIcon.qml
        qml/DockMenu.qml
        qml/DockModel.qml
        qml/DockSeparator.qml
        qml/DockSlot.qml
        qml/NameLabel.qml
        qml/Poof.qml
        qml/StackIcon.qml
        qml/StackMenus.qml
        qml/StackPopup.qml
        qml/TaskMenu.qml
        qml/TrashIcon.qml
    SOURCES
        autohidecontroller.cpp autohidecontroller.h
        blurbehind.cpp blurbehind.h
        dockmanager.cpp dockmanager.h
        dockservice.cpp dockservice.h
        dockwindow.cpp dockwindow.h
        screenedge.cpp screenedge.h
        docktools.cpp docktools.h
        logging.cpp logging.h
        nativemenu.cpp nativemenu.h
        outputorder.cpp outputorder.h
        launcherentries.cpp launcherentries.h
        settingswatcher.cpp settingswatcher.h
        trash.cpp trash.h
)

kconfig_target_kcfg_file(harbour
    FILE harboursettings.kcfg
    CLASS_NAME HarbourSettings
    SINGLETON
    MUTATORS
    NOTIFIERS
    DEFAULT_VALUE_GETTERS
    GENERATE_PROPERTIES
    GENERATE_MOC
    QML_REGISTRATION
)

qt6_generate_wayland_protocol_client_sources(harbour
    PRIVATE_CODE
    FILES
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/blur.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/protocols/ext-background-effect-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-screen-edge-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-order-v1.xml
)

target_compile_definitions(harbour PRIVATE HARBOUR_VERSION="${PROJECT_VERSION}")

target_link_libraries(harbour PRIVATE
    Qt6::DBus
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::WaylandClient
    Qt6::Widgets
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::KIOGui
    KF6::KIOWidgets
    KF6::Service
    LayerShellQt::Interface
)

install(TARGETS harbour ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
