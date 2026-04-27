# Test executables must be defined separately from the actual tests,
# as the same test executable might be used for multiple tests
# (with different parameters each time).

add_test_executable(NDSCreated)
add_test_executable(MultipleNDSCreated)
add_test_executable(MultipleNDSCreatedInDifferentOrder)

add_melonds_test(
    NAME "NDS is created and destroyed"
    TARGET NDSCreated
)

add_melonds_test(
    NAME "Multiple NDS systems are created and destroyed"
    TARGET MultipleNDSCreated
)

add_melonds_test(
    NAME "Multiple NDS systems are created and destroyed in different orders"
    TARGET MultipleNDSCreatedInDifferentOrder
)