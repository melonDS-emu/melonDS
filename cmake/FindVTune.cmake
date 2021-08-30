
find_path(VTUNE_PATH "")

include_directories("${VTUNE_PATH}/include")
link_directories("${VTUNE_PATH}/lib64")
