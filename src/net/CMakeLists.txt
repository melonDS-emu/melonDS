include(FixInterfaceIncludes)

add_library(net-utils STATIC
    Net.cpp
    Net_PCap.cpp
    Net_Slirp.cpp
    PacketDispatcher.cpp
    LocalMP.cpp
    LAN.cpp
    Netplay.cpp
    MPInterface.cpp
)

target_include_directories(net-utils PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/..")

option(USE_SYSTEM_LIBSLIRP "Use system libslirp instead of the bundled version" OFF)
if (USE_SYSTEM_LIBSLIRP)
    pkg_check_modules(Slirp REQUIRED IMPORTED_TARGET slirp)
    fix_interface_includes(PkgConfig::Slirp)
    target_link_libraries(net-utils PUBLIC PkgConfig::Slirp)
else()
    add_subdirectory(libslirp EXCLUDE_FROM_ALL)
    target_link_libraries(net-utils PUBLIC slirp)
endif()

if (USE_VCPKG)
    find_package(unofficial-enet CONFIG REQUIRED)
    target_link_libraries(net-utils PRIVATE unofficial::enet::enet)
else()
    pkg_check_modules(ENet REQUIRED IMPORTED_TARGET libenet)
    fix_interface_includes(PkgConfig::ENet)
    target_link_libraries(net-utils PUBLIC PkgConfig::ENet)
endif()
