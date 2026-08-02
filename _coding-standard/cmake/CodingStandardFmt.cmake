include_guard(GLOBAL)

function(bpr_ensure_fmt fetch_option_var)
  if(TARGET fmt::fmt)
    return()
  endif()

  find_package(fmt CONFIG QUIET)
  if(TARGET fmt::fmt)
    return()
  endif()

  if(NOT DEFINED "${fetch_option_var}")
    message(FATAL_ERROR "${fetch_option_var} must be defined before requesting fmt")
  endif()

  if(NOT ${${fetch_option_var}})
    message(FATAL_ERROR "fmt was not found and ${fetch_option_var} is OFF")
  endif()

  if(NOT COMMAND CPMAddPackage)
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CodingStandardCpm.cmake")
  endif()
  CPMAddPackage(
    NAME fmt
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG 1be298e1bd68957e4cd352e1f676f00e07dcfb57
    OPTIONS
      "FMT_DOC OFF"
      "FMT_TEST OFF"
      "FMT_INSTALL OFF"
  )

  if(NOT TARGET fmt::fmt)
    message(FATAL_ERROR "fmt was fetched but did not expose fmt::fmt")
  endif()
endfunction()
