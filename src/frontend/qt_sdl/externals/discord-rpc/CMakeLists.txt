# A lot of the changes I made were shamelessly copied from Dolphin, thanks y'all :)

project(discord-rpc LANGUAGES CXX)

if(NOT MSVC)
    set(CMAKE_CXX_STANDARD 14)
endif()

set(INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SOURCE_DIR  ${CMAKE_CURRENT_LIST_DIR}/src)

add_library(discord-rpc STATIC
    ${INCLUDE_DIR}/discord_rpc.h
    ${INCLUDE_DIR}/discord_register.h

    ${SOURCE_DIR}/rpc_connection.h
    ${SOURCE_DIR}/backoff.h
    ${SOURCE_DIR}/connection.h
    ${SOURCE_DIR}/msg_queue.h
    ${SOURCE_DIR}/serialization.h

    ${SOURCE_DIR}/discord_rpc.cpp
    ${SOURCE_DIR}/rpc_connection.cpp
    ${SOURCE_DIR}/serialization.cpp
)

target_include_directories(discord-rpc
    PUBLIC  ${INCLUDE_DIR}
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/thirdparty
)

if (WIN32)
    target_sources(discord-rpc PRIVATE
        ${SOURCE_DIR}/connection_win.cpp
        ${SOURCE_DIR}/discord_register_win.cpp
    )

    target_compile_definitions(discord-rpc PRIVATE -DDISCORD_WINDOWS)
    
    if (MSVC)
        target_compile_options(discord-rpc PRIVATE
            /EHsc

            /Wall
            /wd4100 # unreferenced formal parameter
            /wd4514 # unreferenced inline
            /wd4625 # copy constructor deleted
            /wd5026 # move constructor deleted
            /wd4626 # move assignment operator deleted
            /wd4668 # not defined preprocessor macro
            /wd4710 # function not inlined
            /wd4711 # function was inlined
            /wd4820 # structure padding
            /wd4946 # reinterpret_cast used between related classes
            /wd5027 # move assignment operator was implicitly defined as deleted
        )
    endif()

    target_link_libraries(discord-rpc PRIVATE psapi advapi32)
elseif (UNIX)
    target_sources(discord-rpc PRIVATE ${SOURCE_DIR}/connection_unix.cpp)

    if (APPLE)
        enable_language(OBJC)

        target_sources(discord-rpc PRIVATE ${SOURCE_DIR}/discord_register_osx.m)

        target_compile_definitions(discord-rpc PRIVATE -DDISCORD_OSX)
        target_link_libraries(discord-rpc PRIVATE "-framework AppKit, -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        target_sources(discord-rpc PRIVATE ${SOURCE_DIR}/discord_register_linux.cpp)

        target_compile_definitions(discord-rpc PRIVATE -DDISCORD_LINUX)
    endif()

    target_link_libraries(discord-rpc PRIVATE pthread)

    target_compile_options(discord-rpc PRIVATE
        -g

        -Wall
        -Wextra
        -Wpedantic
        -Wno-unknown-pragmas # pragma push thing doesn't work on clang
        -Wno-old-style-cast # it's fine
        -Wno-c++98-compat # that was almost 2 decades ago
        -Wno-c++98-compat-pedantic
        -Wno-missing-noreturn
        -Wno-padded # structure padding
        -Wno-covered-switch-default
        -Wno-exit-time-destructors # not sure about these
        -Wno-global-constructors
    )
endif()
