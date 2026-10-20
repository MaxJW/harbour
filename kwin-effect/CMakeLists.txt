# Built against the exact KWin it is installed with; rebuild after KWin updates.
find_package(KWin REQUIRED)

kcoreaddons_add_plugin(harbour_genie INSTALL_NAMESPACE "kwin/effects/plugins")

target_sources(harbour_genie PRIVATE
    genie.cpp genie.h
    main.cpp
)

target_link_libraries(harbour_genie PRIVATE
    KWin::kwin
    KF6::ConfigCore
)
