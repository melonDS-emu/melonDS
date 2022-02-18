project(qt_sdl)

SET(SOURCES_QT_SDL
    main.cpp
    main_shaders.h
    CheatsDialog.cpp
	Config.cpp
    EmuSettingsDialog.cpp
    PowerManagement/PowerManagementDialog.cpp
    PowerManagement/resources/battery.qrc
    InputConfig/InputConfigDialog.cpp
    InputConfig/MapButton.h
    InputConfig/resources/ds.qrc
    VideoSettingsDialog.cpp
    AudioSettingsDialog.cpp
    FirmwareSettingsDialog.cpp
    PathSettingsDialog.cpp
    WifiSettingsDialog.cpp
    InterfaceSettingsDialog.cpp
    ROMInfoDialog.cpp
    RAMInfoDialog.cpp
    TitleManagerDialog.cpp
    Input.cpp
    LAN_PCap.cpp
    LAN_Socket.cpp
    OSD.cpp
    OSD_shaders.h
    font.h
    Platform.cpp
    QPathInput.h
	ROMManager.cpp
	SaveManager.cpp
    
    ArchiveUtil.h
    ArchiveUtil.cpp

    ../Util_Video.cpp
    ../Util_Audio.cpp
    ../FrontendUtil.h
    ../mic_blow.h

    ${CMAKE_SOURCE_DIR}/res/melon.qrc
)

if (APPLE)
    option(USE_QT6 "Build using Qt 6 instead of 5" ON)
else()
    option(USE_QT6 "Build using Qt 6 instead of 5" OFF)
endif()


if (WIN32)
    set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -i <SOURCE> -o <OBJECT>")
endif()

if (USE_QT6)
    if (BUILD_STATIC AND QT6_STATIC_DIR)
        set(QT6_STATIC_BASE ${QT6_STATIC_DIR}/lib/cmake/Qt6)
        set(Qt6_DIR ${QT6_STATIC_BASE})
        set(Qt6Core_DIR ${QT6_STATIC_BASE}Core)
        set(Qt6Gui_DIR ${QT6_STATIC_BASE}Gui)
        set(Qt6Widgets_DIR ${QT6_STATIC_BASE}Widgets)
        set(Qt6Network_DIR ${QT6_STATIC_BASE}Network)
        set(Qt6OpenGL_DIR ${QT6_STATIC_BASE}OpenGL)
        set(Qt6OpenGLWidgets_DIR ${QT6_STATIC_BASE}OpenGLWidgets)
    endif()
    find_package(Qt6 COMPONENTS Core Gui Widgets Network OpenGL OpenGLWidgets REQUIRED)
    set(QT_LINK_LIBS Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets)
else()
    if (BUILD_STATIC AND QT5_STATIC_DIR)
        set(QT5_STATIC_BASE ${QT5_STATIC_DIR}/lib/cmake/Qt5)
        set(Qt5_DIR ${QT5_STATIC_BASE})
        set(Qt5Core_DIR ${QT5_STATIC_BASE}Core)
        set(Qt5Gui_DIR ${QT5_STATIC_BASE}Gui)
        set(Qt5Widgets_DIR ${QT5_STATIC_BASE}Widgets)
        set(Qt5Network_DIR ${QT5_STATIC_BASE}Network)
    endif()
    find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)
    set(QT_LINK_LIBS Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Iconv REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SLIRP REQUIRED slirp)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
add_compile_definitions(ARCHIVE_SUPPORT_ENABLED)

if (WIN32 AND (CMAKE_BUILD_TYPE STREQUAL Release))
    add_executable(melonDS WIN32 ${SOURCES_QT_SDL})
else()
    add_executable(melonDS ${SOURCES_QT_SDL})
endif()

target_link_libraries(melonDS ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(melonDS PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_PREFIX}/include ${SLIRP_INCLUDE_DIRS} ${LIBARCHIVE_INCLUDE_DIRS})
target_link_directories(melonDS PRIVATE ${SDL2_LIBRARY_DIRS} ${SLIRP_LIBRARY_DIRS})
target_link_directories(melonDS PRIVATE ${LIBARCHIVE_LIBRARY_DIRS})

target_include_directories(melonDS PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(melonDS PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_include_directories(melonDS PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(melonDS core)

if (BUILD_STATIC)
    target_link_libraries(melonDS -static ${SDL2_STATIC_LIBRARIES} ${SLIRP_STATIC_LIBRARIES} ${LIBARCHIVE_STATIC_LIBRARIES})
    qt_import_plugins(melonDS INCLUDE Qt::QSvgPlugin)
else()
    target_link_libraries(melonDS ${SDL2_LIBRARIES} ${SLIRP_LIBRARIES} ${LIBARCHIVE_LIBRARIES})
endif()

if (NOT Iconv_IS_BUILT_IN)
    target_link_libraries(melonDS ${Iconv_LIBRARIES})
endif()

if (UNIX)
    option(PORTABLE "Make a portable build that looks for its configuration in the current directory" OFF)
    target_link_libraries(melonDS ${QT_LINK_LIBS})
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(melonDS dl)
    endif()
elseif (WIN32)
    option(PORTABLE "Make a portable build that looks for its configuration in the current directory" ON)
    configure_file("${CMAKE_SOURCE_DIR}/res/melon.rc.in" "${CMAKE_SOURCE_DIR}/melon.rc")
    target_sources(melonDS PUBLIC "${CMAKE_SOURCE_DIR}/melon.rc")

    target_link_libraries(melonDS comctl32 d2d1 dwrite uxtheme ws2_32 iphlpapi gdi32)
    if (BUILD_STATIC)
        target_link_libraries(melonDS imm32 winmm version setupapi -static z zstd ${QT_LINK_LIBS})
    else()
        target_link_libraries(melonDS ${QT_LINK_LIBS})
    endif()
endif()

if (PORTABLE)
    add_definitions(-DPORTABLE)
endif()

if (APPLE)
    # Copy icon into the bundle
    set(RESOURCE_FILES "${CMAKE_SOURCE_DIR}/res/melon.icns")
    target_sources(melonDS PUBLIC "${RESOURCE_FILES}")

    set_target_properties(melonDS PROPERTIES
        MACOSX_BUNDLE true
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/melon.plist.in
        OUTPUT_NAME melonDS
        RESOURCE "${RESOURCE_FILES}")
	

    option(MACOS_BUNDLE_LIBS "Bundle libraries with the app on macOS" OFF)
    option(MACOS_BUILD_DMG "Build DMG image of the macOS application bundle" OFF)

    if (MACOS_BUNDLE_LIBS)
        if (MACOS_BUILD_DMG)
            set(DMGARG "--dmg")
        endif()
        add_custom_command(TARGET melonDS POST_BUILD
            COMMAND ${CMAKE_SOURCE_DIR}/tools/mac-libs.rb ${DMGARG} ${CMAKE_BINARY_DIR}
            COMMENT "Bundling macOS libraries...")
    endif()
endif()

if (UNIX AND NOT APPLE)
    foreach(SIZE 16 32 48 64 128 256)
        install(FILES ${CMAKE_SOURCE_DIR}/res/icon/melon_${SIZE}x${SIZE}.png
                DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/${SIZE}x${SIZE}/apps
                RENAME net.kuribo64.melonDS.png)
    endforeach()

    install(FILES ${CMAKE_SOURCE_DIR}/res/net.kuribo64.melonDS.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
    install(TARGETS melonDS BUNDLE DESTINATION ${CMAKE_BINARY_DIR} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
