cmake_policy(SET CMP0110 NEW)

if (NOT NDS_ROM)
    message(WARNING "NDS_ROM must be set to the path of an NDS ROM; tests that require one won't run.")
    set(NDS_ROM "NDS_ROM-NOTFOUND" CACHE FILEPATH "Path to an NDS ROM" FORCE)
else()
    message(DEBUG "NDS_ROM: ${NDS_ROM}")
endif()

include(CMakePrintHelpers)

function(add_melonds_test)
    set(options WILL_FAIL ARM7_BIOS ARM9_BIOS ARM7_DSI_BIOS ARM9_DSI_BIOS NDS_FIRMWARE DSI_FIRMWARE DSI_NAND DISABLED)
    set(oneValueArgs TARGET NAME ROM)
    set(multiValueArgs ARGS LABEL)
    cmake_parse_arguments(PARSE_ARGV 0 MELONDS_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}")

    add_test(
        NAME "${MELONDS_TEST_NAME}"
        COMMAND "${MELONDS_TEST_TARGET}"
        ${MELONDS_TEST_ARGS}
        COMMAND_EXPAND_LISTS
    )

    if (MELONDS_TEST_ROM)
        list(APPEND REQUIRED_FILES "${MELONDS_TEST_ROM}")
    endif()

    macro(expose_system_file SYSFILE)
        if (MELONDS_TEST_${SYSFILE})
            list(APPEND REQUIRED_FILES "${${SYSFILE}}")
            list(APPEND ENVIRONMENT "${SYSFILE}=${${SYSFILE}}")
        endif()
    endmacro()

    expose_system_file(ARM7_BIOS)
    expose_system_file(ARM9_BIOS)
    expose_system_file(ARM7_DSI_BIOS)
    expose_system_file(ARM9_DSI_BIOS)
    expose_system_file(NDS_FIRMWARE)
    expose_system_file(DSI_FIRMWARE)
    expose_system_file(DSI_NAND)

    set_tests_properties("${MELONDS_TEST_NAME}" PROPERTIES LABELS "${MELONDS_TEST_LABEL}") # This is already a list
    set_tests_properties("${MELONDS_TEST_NAME}" PROPERTIES ENVIRONMENT "${ENVIRONMENT}")
    set_tests_properties("${MELONDS_TEST_NAME}" PROPERTIES REQUIRED_FILES "${REQUIRED_FILES}")

    if (MELONDS_TEST_WILL_FAIL)
        set_tests_properties("${MELONDS_TEST_NAME}" PROPERTIES WILL_FAIL TRUE)
    endif()

    if (MELONDS_TEST_DISABLED)
        set_tests_properties("${MELONDS_TEST_NAME}" PROPERTIES DISABLED TRUE)
    endif()
endfunction()

function(add_test_executable TEST_PROGRAM)
    add_executable("${TEST_PROGRAM}" "${TEST_PROGRAM}.cpp")
    target_link_libraries("${TEST_PROGRAM}" PRIVATE core melonDS-tests-common)
    target_include_directories("${TEST_PROGRAM}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
endfunction()

add_subdirectory(common)
add_subdirectory(cases)
