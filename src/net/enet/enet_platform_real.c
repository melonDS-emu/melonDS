/*
    Builds ENet's platform socket layer under enet_real_* names.

    The public enet_socket_* functions are provided by VNet.cpp, which either
    forwards to these real implementations or handles virtual relay sockets.
*/

#define enet_socket_create      enet_real_socket_create
#define enet_socket_bind        enet_real_socket_bind
#define enet_socket_get_address enet_real_socket_get_address
#define enet_socket_listen      enet_real_socket_listen
#define enet_socket_accept      enet_real_socket_accept
#define enet_socket_connect     enet_real_socket_connect
#define enet_socket_send        enet_real_socket_send
#define enet_socket_receive     enet_real_socket_receive
#define enet_socket_wait        enet_real_socket_wait
#define enet_socket_set_option  enet_real_socket_set_option
#define enet_socket_get_option  enet_real_socket_get_option
#define enet_socket_shutdown    enet_real_socket_shutdown
#define enet_socket_destroy     enet_real_socket_destroy
#define enet_socketset_select   enet_real_socketset_select

#ifdef _WIN32
#include "win32.c"
#else
#include "unix.c"
#endif
