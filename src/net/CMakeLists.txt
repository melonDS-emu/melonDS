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

# bundled ENet, with the socket layer routed through VNet (relay server support)
add_library(enet-vnet STATIC
    enet/callbacks.c
    enet/compress.c
    enet/host.c
    enet/list.c
    enet/packet.c
    enet/peer.c
    enet/protocol.c
    enet/enet_platform_real.c
    VNet.cpp
)
target_include_directories(enet-vnet PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/enet/include")
if (WIN32)
    target_link_libraries(enet-vnet PUBLIC ws2_32 winmm)
else()
    target_compile_definitions(enet-vnet PRIVATE
        HAS_FCNTL=1 HAS_POLL=1 HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1
        HAS_INET_PTON=1 HAS_INET_NTOP=1 HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1)
    find_package(Threads REQUIRED)
    target_link_libraries(enet-vnet PUBLIC Threads::Threads)
endif()
target_link_libraries(net-utils PUBLIC enet-vnet)
