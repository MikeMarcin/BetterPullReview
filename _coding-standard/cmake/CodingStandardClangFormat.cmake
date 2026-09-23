include_guard(GLOBAL)

# Finding the clang-format the format gate runs ([[BPR-0865]]).
#
# The gate needs a clang-format that reports the style file's minimum version
# or newer. Resolving `clang-format` from PATH is not enough, because a station
# can put an older one first. On the Linux station, PATH gives 21.1.8 from
# ~/.local/bin, and /usr/bin/clang-format is 22.1.8. A gate that stopped at
# the first one refuses to give a verdict on a machine that can give one.
#
# So each candidate CMake's program search finds is asked whether it is
# supported, using the gate's own check (scripts/supported_clang_format.py). An
# older one is passed over and the search goes on. The result is cached in
# BPR_CLANG_FORMAT_EXECUTABLE. Set that variable to choose a tool; a value you
# set is used as given and is not checked here. The gate still checks it when
# it runs.

set(
    CODING_STANDARD_SUPPORTED_CLANG_FORMAT_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/../scripts/supported_clang_format.py"
)

# find_program's VALIDATOR. It reads CODING_STANDARD_CLANG_FORMAT_PYTHON from
# coding_standard_find_clang_format, which calls find_program.
function(coding_standard_clang_format_is_supported result candidate)
    execute_process(
        COMMAND
            "${CODING_STANDARD_CLANG_FORMAT_PYTHON}" -B
            "${CODING_STANDARD_SUPPORTED_CLANG_FORMAT_SCRIPT}" "${candidate}"
        RESULT_VARIABLE supported_status
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT supported_status EQUAL 0)
        set(${result} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Sets BPR_CLANG_FORMAT_EXECUTABLE to the first supported clang-format on
# CMake's program search path, or to BPR_CLANG_FORMAT_EXECUTABLE-NOTFOUND.
# `python` runs the supported-version check.
function(coding_standard_find_clang_format python)
    set(CODING_STANDARD_CLANG_FORMAT_PYTHON "${python}")
    find_program(
        BPR_CLANG_FORMAT_EXECUTABLE
        NAMES clang-format
        VALIDATOR coding_standard_clang_format_is_supported
        DOC "clang-format for the coding-format targets and the coding_format_check test"
    )
endfunction()
