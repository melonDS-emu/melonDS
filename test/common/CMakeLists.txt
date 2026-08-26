include(FixInterfaceIncludes)

add_library(melonDS-tests-common STATIC
    Platform.cpp
        TestCommon.cpp
        TestCommon.h
)

# The test program is built with C++20 so we can use std::counting_semaphore
set_target_properties(melonDS-tests-common PROPERTIES CXX_STANDARD 20)

target_link_libraries(melonDS-tests-common PRIVATE core)
target_include_directories(melonDS-tests-common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../src")