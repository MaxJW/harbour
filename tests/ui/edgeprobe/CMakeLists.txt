qt_add_executable(edgeprobe main.cpp)
qt6_generate_wayland_protocol_client_sources(edgeprobe PRIVATE_CODE FILES ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml)
target_link_libraries(edgeprobe PRIVATE Qt6::Gui Qt6::WaylandClient)
