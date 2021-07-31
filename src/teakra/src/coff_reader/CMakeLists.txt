include(CreateDirectoryGroups)

add_executable(coff_reader
    coff.h
    main.cpp
)
create_target_directory_groups(coff_reader)
target_link_libraries(coff_reader PRIVATE teakra)
target_include_directories(coff_reader PRIVATE .)
target_compile_options(coff_reader PRIVATE ${TEAKRA_CXX_FLAGS})
