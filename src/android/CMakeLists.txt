project(melonDS-lib)

set(ANDROID_SOURCES
        InputAndroid.cpp
        LAN_PCap.cpp
        LAN_Socket.cpp
        MelonDS.cpp
        OboeCallback.cpp
        PlatformAndroid.cpp
        PlatformConfig.cpp
        ifaddrs.c
)

add_subdirectory(${OBOE} ./oboe)
# Include root dir so that pcap can be found. Probably a bad idea
include_directories(${OBOE}/include ../)
add_library(melonDS-lib SHARED ${ANDROID_SOURCES})
target_link_libraries(melonDS-lib core oboe)