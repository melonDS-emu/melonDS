set(SANITIZE "" CACHE STRING "Sanitizers to enable.")

string(REGEX MATCHALL "[^,]+" ENABLED_SANITIZERS "${SANITIZE}")

foreach(SANITIZER ${ENABLED_SANITIZERS})
    add_compile_options("-fsanitize=${SANITIZER}")
    add_link_options("-fsanitize=${SANITIZER}")
endforeach()