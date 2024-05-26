include(FindPackageMessage)

find_program(CCACHE "ccache")

cmake_dependent_option(USE_CCACHE "Use CCache to speed up repeated builds." ON CCACHE OFF)

if (NOT CCACHE OR NOT USE_CCACHE)
    return()
endif()

# Fedora, and probably also Red Hat-based distros in general, use CCache by default if it's installed on the system.
# We'll try to detect this here, and exit if that's the case.
# Trying to launch ccache with ccache as we'd otherwise do seems to cause build issues.
if (CMAKE_C_COMPILER MATCHES "ccache" OR CMAKE_CXX_COMPILER MATCHES "ccache")
    return()
endif()

find_package_message(CCache "Using CCache to speed up compilation" "${USE_CCACHE}")
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE}")