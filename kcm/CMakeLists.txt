kcmutils_add_qml_kcm(kcm_harbour SOURCES
    harbourkcm.cpp harbourkcm.h
)

# Same schema as the dock, so both read and write ~/.config/harbourrc.
kconfig_target_kcfg_file(kcm_harbour
    FILE ${CMAKE_SOURCE_DIR}/src/harboursettings.kcfg
    CLASS_NAME HarbourSettings
    MUTATORS
    NOTIFIERS
    DEFAULT_VALUE_GETTERS
    GENERATE_PROPERTIES
    GENERATE_MOC
    PARENT_IN_CONSTRUCTOR
)

target_link_libraries(kcm_harbour PRIVATE
    Qt6::DBus
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::CoreAddons
    KF6::KCMUtilsQuick
)
