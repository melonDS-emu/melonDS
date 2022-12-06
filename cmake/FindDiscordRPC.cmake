include(FindPackageHandleStandardArgs)

find_path   (DiscordRPC_INCLUDE_DIR NAMES discord_rpc.h)
find_library(DiscordRPC_LIBRARY     NAMES discord-rpc)

find_package_handle_standard_args(DiscordRPC DEFAULT_MSG DiscordRPC_LIBRARY DiscordRPC_INCLUDE_DIR)

if(DiscordRPC_FOUND)
	set(DiscordRPC_LIBRARIES    "${DiscordRPC_LIBRARY}")
	set(DiscordRPC_INCLUDE_DIRS "${DiscordRPC_INCLUDE_DIR}")
endif()

mark_as_advanced(DiscordRPC_INCLUDE_DIR DiscordRPC_LIBRARY)
