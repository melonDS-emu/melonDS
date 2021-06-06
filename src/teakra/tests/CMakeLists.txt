add_executable(teakra_tests
    dma.cpp
    btdmp.cpp
    interpreter.cpp
    main.cpp
    timer.cpp
)

target_link_libraries(teakra_tests PRIVATE teakra catch)
target_compile_options(teakra_tests PRIVATE ${TEAKRA_CXX_FLAGS})

add_test(teakra_tests teakra_tests)
