# The entire codebase quite reasonably does things like #include <SDL2/SDL.h> or <epoxy/gl.h>
# CMake apparently doesn't think you should be doing this, so just includes $PREFIX/include/packagename for a given
# package as include directories when using `target_link_libraries` with an imported target, this hacky function fixes
# that up so includes can keep working as they always did but we can still use fancy imported targets.
# This is stupid.

function(fix_interface_includes)
    foreach (target ${ARGN})
        set(NEW_DIRS)
        get_target_property(DIRS "${target}" INTERFACE_INCLUDE_DIRECTORIES)

        if (NOT DIRS)
            continue()
        endif()

        foreach (DIR ${DIRS})
            get_filename_component(PARENT_DIR "${DIR}" DIRECTORY)

            if (PARENT_DIR MATCHES "include$")
                list(APPEND NEW_DIRS "${PARENT_DIR}")
            endif()

            # HACK
            # The libarchive pkg-config file in MSYS2 seems to include a UNIX-style path for its
            # include directory and CMake doesn't like that.
            if (WIN32 AND MINGW AND target STREQUAL PkgConfig::LibArchive)
                list(FILTER DIRS EXCLUDE REGEX "^/[^.]+64/.*")
            endif()
        endforeach()

        list(APPEND DIRS ${NEW_DIRS})
        set_target_properties("${target}" PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${DIRS}")
    endforeach()
endfunction()

