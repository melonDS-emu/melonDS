include(CreateDirectoryGroups)

add_executable(test_verifier
    main.cpp
)
create_target_directory_groups(test_verifier)
target_link_libraries(test_verifier PRIVATE teakra)
target_include_directories(test_verifier PRIVATE .)
target_compile_options(test_verifier PRIVATE ${TEAKRA_CXX_FLAGS})


# Test related stuff
set(ASSET_SHA256SUM "baffcd4f805a7480d969401792443a34aa39f813b4f0ae49c6365f1d1f3ce120")
if(TEAKRA_RUN_TESTS)
  message(STATUS "Will run Teakra accuracy tests")
  # download fixtures if there is none
  if(NOT EXISTS "${TEAKRA_TEST_ASSETS_DIR}/teaklite2_tests_result")
    message(STATUS "Downloading required samples...")
    file(DOWNLOAD
      "https://liushuyu.b-cdn.net/teaklite2_tests_result_20181208"
      "${TEAKRA_TEST_ASSETS_DIR}/teaklite2_tests_result"
      EXPECTED_HASH SHA256=${ASSET_SHA256SUM}
      SHOW_PROGRESS
  )
  else()
    # check if provided fixtures are good
    file(SHA256 "${TEAKRA_TEST_ASSETS_DIR}/teaklite2_tests_result" ASSET_CHECKSUM)
    if(ASSET_SHA256SUM STREQUAL ASSET_CHECKSUM)
      message(STATUS "Unit test sample looks good.")
    else()
      message(FATAL_ERROR "Unit test sample broken. Please remove the file and re-run CMake.")
    endif()
  endif()

  add_test(NAME tests COMMAND test_verifier "${TEAKRA_TEST_ASSETS_DIR}/teaklite2_tests_result")
endif(TEAKRA_RUN_TESTS)
