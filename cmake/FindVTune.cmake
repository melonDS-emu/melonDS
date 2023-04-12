
find_path(VTUNE_PATH "")

set(VTUNE_INCLUDE_DIR "${VTUNE_PATH}/include")

if (WIN32)
    set(VTUNE_LIBRARY "${VTUNE_PATH}/lib64/jitprofiling.lib")
else()
    set(VTUNE_LIBRARY "${VTUNE_PATH}/lib64/jitprofiling.a")
endif()
