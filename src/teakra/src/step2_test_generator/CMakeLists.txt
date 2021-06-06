include(CreateDirectoryGroups)

add_executable(step2_test_generator
    main.cpp
)
create_target_directory_groups(step2_test_generator)
target_link_libraries(step2_test_generator PRIVATE teakra)
target_include_directories(step2_test_generator PRIVATE .)
target_compile_options(step2_test_generator PRIVATE ${TEAKRA_CXX_FLAGS})
