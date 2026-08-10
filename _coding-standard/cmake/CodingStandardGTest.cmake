include_guard(GLOBAL)

function(bpr_ensure_gtest fetch_option_var)
  if(TARGET GTest::gtest_main)
    return()
  endif()

  find_package(GTest CONFIG QUIET)
  if(NOT TARGET GTest::gtest_main)
    find_package(GTest QUIET)
  endif()
  if(TARGET GTest::gtest_main)
    return()
  endif()

  if(NOT DEFINED "${fetch_option_var}")
    message(FATAL_ERROR "${fetch_option_var} must be defined before requesting GoogleTest")
  endif()

  if(NOT ${${fetch_option_var}})
    message(FATAL_ERROR "GoogleTest was not found and ${fetch_option_var} is OFF")
  endif()

  if(NOT COMMAND CPMAddPackage)
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CodingStandardCpm.cmake")
  endif()
  set(BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "Do not install GoogleTest" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "Use the shared MSVC runtime" FORCE)
  CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723
    OPTIONS
      "BUILD_GMOCK OFF"
      "INSTALL_GTEST OFF"
      "gtest_force_shared_crt ON"
  )

  if(NOT TARGET GTest::gtest_main)
    message(FATAL_ERROR "GoogleTest was fetched but did not expose GTest::gtest_main")
  endif()
endfunction()

function(bpr_add_gtest_suite target)
  set(one_value_args FETCH_OPTION)
  set(multi_value_args SOURCES LIBRARIES TESTS COMPILE_DEFINITIONS COMPILE_OPTIONS)
  cmake_parse_arguments(BPR_GTEST "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT BPR_GTEST_FETCH_OPTION)
    message(FATAL_ERROR "bpr_add_gtest_suite(${target}) requires FETCH_OPTION")
  endif()
  if(NOT BPR_GTEST_SOURCES)
    message(FATAL_ERROR "bpr_add_gtest_suite(${target}) requires at least one source")
  endif()

  bpr_ensure_gtest("${BPR_GTEST_FETCH_OPTION}")

  add_executable("${target}" ${BPR_GTEST_SOURCES})
  target_compile_features("${target}" PRIVATE cxx_std_20)
  target_link_libraries("${target}" PRIVATE ${BPR_GTEST_LIBRARIES} GTest::gtest_main)
  if(BPR_GTEST_COMPILE_DEFINITIONS)
    target_compile_definitions("${target}" PRIVATE ${BPR_GTEST_COMPILE_DEFINITIONS})
  endif()
  if(BPR_GTEST_COMPILE_OPTIONS)
    target_compile_options("${target}" PRIVATE ${BPR_GTEST_COMPILE_OPTIONS})
  endif()

  set(bpr_test_specs ${BPR_GTEST_TESTS})
  while(bpr_test_specs)
    list(LENGTH bpr_test_specs bpr_spec_count)
    if(bpr_spec_count LESS 2)
      message(FATAL_ERROR "TESTS for ${target} must be ctest-name/gtest-filter pairs")
    endif()

    list(POP_FRONT bpr_test_specs bpr_ctest_name)
    list(POP_FRONT bpr_test_specs bpr_gtest_filter)
    bpr_add_network_isolated_test(NAME "${bpr_ctest_name}" COMMAND "$<TARGET_FILE:${target}>" "--gtest_filter=${bpr_gtest_filter}")
  endwhile()
endfunction()
