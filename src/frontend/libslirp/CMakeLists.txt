cmake_minimum_required(VERSION 3.16)

project(libslirp VERSION 4.8.0 LANGUAGES C)

set(SLIRP_MAJOR_VERSION "${libslirp_VERSION_MAJOR}")
set(SLIRP_MINOR_VERSION "${libslirp_VERSION_MINOR}")
set(SLIRP_MICRO_VERSION "${libslirp_VERSION_PATCH}")
set(SLIRP_VERSION_STRING "\"${libslirp_VERSION}\"")

set(SOURCES
	src/arp_table.c
	src/bootp.c
	src/cksum.c
	src/dhcpv6.c
	src/dnssearch.c
	src/if.c
	src/ip6_icmp.c
	src/ip6_input.c
	src/ip6_output.c
	src/ip_icmp.c
	src/ip_input.c
	src/ip_output.c
	src/mbuf.c
	src/misc.c
	src/ncsi.c
	src/ndp_table.c
	src/sbuf.c
	src/slirp.c
	src/socket.c
	src/state.c
	src/stream.c
	src/tcp_input.c
	src/tcp_output.c
	src/tcp_subr.c
	src/tcp_timer.c
	src/tftp.c
	src/udp6.c
	src/udp.c
	src/util.c
	src/version.c
	src/vmstate.c

	# glib shim
	glib/glib.c
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/libslirp-version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/libslirp-version.h")

add_library(slirp STATIC ${SOURCES})
target_compile_definitions(slirp PUBLIC LIBSLIRP_STATIC_BUILD)

target_include_directories(slirp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/glib")
target_include_directories(slirp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(slirp PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_definitions(slirp PRIVATE BUILDING_LIBSLIRP)
target_compile_definitions(slirp PRIVATE "G_LOG_DOMAIN=\"Slirp\"")

if (WIN32)
	target_link_libraries(slirp PRIVATE ws2_32 iphlpapi)
elseif(HAIKU)
	target_Link_libraries(slirp PRIVATE network)
elseif(APPLE)
	target_link_libraries(slirp PRIVATE resolv)
endif()
