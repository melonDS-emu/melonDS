include(CreateDirectoryGroups)

add_library(teakra
    ../include/teakra/disassembler.h
    ../include/teakra/teakra.h
    ahbm.cpp
    ahbm.h
    apbp.cpp
    apbp.h
    bit.h
    btdmp.cpp
    btdmp.h
    common_types.h
    crash.h
    decoder.h
    disassembler.cpp
    dma.cpp
    dma.h
    timer.cpp
    timer.h
    icu.h
    interpreter.h
    matcher.h
    memory_interface.cpp
    memory_interface.h
    mmio.cpp
    mmio.h
    operand.h
    parser.cpp
    processor.cpp
    processor.h
    register.h
    shared_memory.h
    teakra.cpp
)

if (TEAKRA_BUILD_UNIT_TESTS)
    target_sources(teakra PUBLIC
        test.h
        test_generator.cpp
        test_generator.h
    )
endif()

create_target_directory_groups(teakra)

target_link_libraries(teakra PRIVATE Threads::Threads)
target_include_directories(teakra
                           PUBLIC ../include
                           PRIVATE .)
target_compile_options(teakra PRIVATE ${TEAKRA_CXX_FLAGS})

add_library(teakra_c
    ../include/teakra/disassembler_c.h
    ../include/teakra/teakra_c.h
    disassembler_c.cpp
    teakra_c.cpp
)
target_link_libraries(teakra_c PRIVATE teakra)

if (TEAKRA_BUILD_TOOLS)
    add_subdirectory(coff_reader)
    add_subdirectory(dsp1_reader)
    add_subdirectory(test_generator)
    add_subdirectory(test_verifier)
    add_subdirectory(mod_test_generator)
    add_subdirectory(step2_test_generator)
    add_subdirectory(makedsp1)
endif()
