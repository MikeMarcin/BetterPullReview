include_guard(GLOBAL)

option(BPR_ENABLE_CODING_TARGETS "Enable shared coding-standard utility targets" ON)
if(NOT BPR_ENABLE_CODING_TARGETS)
  return()
endif()

if(NOT TARGET Python3::Interpreter)
  message(
    FATAL_ERROR
    "BetterPullReview coding targets require the centrally discovered Python3::Interpreter target"
  )
endif()

get_filename_component(CODING_STANDARD_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CODING_STANDARD_FORMAT_SCRIPT "${CODING_STANDARD_ROOT}/scripts/format.py")
set(CODING_STANDARD_FORMAT_CHECK_SCRIPT "${CODING_STANDARD_ROOT}/scripts/format_check.py")
set(CODING_STANDARD_FORMAT_CHECK_OR_SKIP_SCRIPT "${CODING_STANDARD_ROOT}/scripts/format_check_or_skip.py")
set(CODING_STANDARD_TIDY_SCRIPT "${CODING_STANDARD_ROOT}/scripts/tidy.py")

include("${CMAKE_CURRENT_LIST_DIR}/CodingStandardClangFormat.cmake")
get_target_property(coding_standard_python Python3::Interpreter IMPORTED_LOCATION)
coding_standard_find_clang_format("${coding_standard_python}")
# The format targets run the tool discovery found. With none, they keep asking
# PATH, so an explicit build of the gate still stops with the gate's own
# refusal instead of doing nothing.
set(CODING_STANDARD_CLANG_FORMAT_ARGUMENTS)
if(BPR_CLANG_FORMAT_EXECUTABLE)
  set(CODING_STANDARD_CLANG_FORMAT_ARGUMENTS --clang-format "${BPR_CLANG_FORMAT_EXECUTABLE}")
  message(STATUS "Coding standard clang-format: ${BPR_CLANG_FORMAT_EXECUTABLE}")
else()
  message(STATUS "Coding standard clang-format: no clang-format 22 found; coding_format_check will be skipped")
endif()

add_custom_target(coding-format
    COMMAND Python3::Interpreter "${CODING_STANDARD_FORMAT_SCRIPT}" ${CODING_STANDARD_CLANG_FORMAT_ARGUMENTS}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Formatting project C/C++ files with clang-format"
    VERBATIM
)

add_custom_target(coding-format-check
    COMMAND Python3::Interpreter "${CODING_STANDARD_FORMAT_CHECK_SCRIPT}" ${CODING_STANDARD_CLANG_FORMAT_ARGUMENTS}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Checking project C/C++ formatting with clang-format"
    VERBATIM
)

add_custom_target(coding-tidy
    COMMAND Python3::Interpreter "${CODING_STANDARD_TIDY_SCRIPT}" --build-dir "${CMAKE_BINARY_DIR}" --warnings-as-errors
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Running clang-tidy using the current CMake build directory"
    VERBATIM
)
