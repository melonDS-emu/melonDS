include(CreateDirectoryGroups)

add_executable(makedsp1
    main.cpp
    sha256.cpp
    sha256.h
)
create_target_directory_groups(makedsp1)
target_link_libraries(makedsp1 PRIVATE teakra)
target_include_directories(makedsp1 PRIVATE .)
target_compile_options(makedsp1 PRIVATE ${TEAKRA_CXX_FLAGS})
