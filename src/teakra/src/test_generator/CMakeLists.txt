include(CreateDirectoryGroups)

add_executable(test_generator
    main.cpp
)
create_target_directory_groups(test_generator)
target_link_libraries(test_generator PRIVATE teakra)
target_include_directories(test_generator PRIVATE .)
target_compile_options(test_generator PRIVATE ${TEAKRA_CXX_FLAGS})
