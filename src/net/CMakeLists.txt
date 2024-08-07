include(FixInterfaceIncludes)

add_library(net-utils STATIC
    Net.cpp
    Net_PCap.cpp
    Net_Slirp.cpp
    PacketDispatcher.cpp
    LocalMP.cpp
)

target_include_directories(net-utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(net-utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")


option(USE_SYSTEM_LIBSLIRP "Use system libslirp instead of the bundled version" OFF)
if (USE_SYSTEM_LIBSLIRP)
    pkg_check_modules(Slirp REQUIRED IMPORTED_TARGET slirp)
    fix_interface_includes(PkgConfig::Slirp)
    target_link_libraries(net-utils PRIVATE PkgConfig::Slirp)
else()
    add_subdirectory(libslirp EXCLUDE_FROM_ALL)
    target_include_directories(net-utils SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libslirp/glib")
    target_link_libraries(net-utils PRIVATE slirp)
endif()
