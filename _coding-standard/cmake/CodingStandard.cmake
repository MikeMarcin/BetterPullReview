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
set(CODING_STANDARD_TIDY_SCRIPT "${CODING_STANDARD_ROOT}/scripts/tidy.py")

add_custom_target(coding-format
    COMMAND Python3::Interpreter "${CODING_STANDARD_FORMAT_SCRIPT}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Formatting project C/C++ files with clang-format"
    VERBATIM
)

add_custom_target(coding-format-check
    COMMAND Python3::Interpreter "${CODING_STANDARD_FORMAT_CHECK_SCRIPT}"
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
