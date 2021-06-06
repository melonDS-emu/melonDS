include(CreateDirectoryGroups)

add_executable(mod_test_generator
    main.cpp
)
create_target_directory_groups(mod_test_generator)
target_link_libraries(mod_test_generator PRIVATE teakra)
target_include_directories(mod_test_generator PRIVATE .)
target_compile_options(mod_test_generator PRIVATE ${TEAKRA_CXX_FLAGS})
