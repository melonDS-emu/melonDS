include(CreateDirectoryGroups)

add_executable(dsp1_reader
    main.cpp
)
create_target_directory_groups(dsp1_reader)
target_link_libraries(dsp1_reader PRIVATE teakra)
target_include_directories(dsp1_reader PRIVATE .)
target_compile_options(dsp1_reader PRIVATE ${TEAKRA_CXX_FLAGS})
