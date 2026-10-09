include_guard(GLOBAL)

# Finding the clang-format the format gate runs ([[BPR-0865]], [[BPR-1352]]).
#
# The tree is rendered by clang-format 22, exactly. Another major release
# formats the same file differently: 21 and earlier write `{}` for `{ }`, and 23
# sorts includes, aligns consecutive assignments and indents designated-
# initializer continuations differently. Resolving `clang-format` from PATH is
# not enough, because a station can put another release first. On the Mac,
# Homebrew's default `llvm` is 23 and the 22 is the keg-only `llvm@22`; on the
# Linux station, PATH gives 21.1.8 from ~/.local/bin and /usr/bin/clang-format
# is 22.1.8.
#
# So the search looks for `clang-format-22` first, then `clang-format` in the
# directories where a 22 is installed beside another default (the same list as
# PINNED_CLANG_FORMAT_DIRECTORIES in scripts/_coding_standard.py), then
# `clang-format` on PATH. Each candidate is asked whether it is 22, using the
# gate's own check (scripts/supported_clang_format.py); any other release is
# passed over and the search goes on. The result is cached in
# BPR_CLANG_FORMAT_EXECUTABLE. Set that variable to choose a tool.
#
# A cached value is checked too. A build tree configured before the pin can
# hold Homebrew's 23; it is reported with a warning and searched for again,
# because a tool the gate refuses can never give that tree a verdict.

set(
    CODING_STANDARD_SUPPORTED_CLANG_FORMAT_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/../scripts/supported_clang_format.py"
)

# Searched before PATH. A caller may define this, even empty, to replace it;
# the gate's tests do, so a station's real llvm@22 does not answer for a stub.
if(NOT DEFINED CODING_STANDARD_CLANG_FORMAT_HINTS)
    set(
        CODING_STANDARD_CLANG_FORMAT_HINTS
        /opt/homebrew/opt/llvm@22/bin
        /usr/local/opt/llvm@22/bin
        /usr/lib/llvm-22/bin
    )
endif()

# find_program's VALIDATOR. It reads CODING_STANDARD_CLANG_FORMAT_PYTHON from
# coding_standard_find_clang_format, which calls find_program.
function(coding_standard_clang_format_is_supported result candidate)
    execute_process(
        COMMAND
            "${CODING_STANDARD_CLANG_FORMAT_PYTHON}" -B
            "${CODING_STANDARD_SUPPORTED_CLANG_FORMAT_SCRIPT}" "${candidate}"
        RESULT_VARIABLE supported_status
        OUTPUT_QUIET
        ERROR_VARIABLE supported_diagnostic
    )
    if(NOT supported_status EQUAL 0)
        set(${result} FALSE PARENT_SCOPE)
        string(REGEX MATCH "unsupported clang-format: [^\n]*" supported_reason "${supported_diagnostic}")
        set(CODING_STANDARD_CLANG_FORMAT_REJECTED "${supported_reason}" PARENT_SCOPE)
    endif()
endfunction()

# Sets BPR_CLANG_FORMAT_EXECUTABLE to the first clang-format 22 on the search
# described above, or to BPR_CLANG_FORMAT_EXECUTABLE-NOTFOUND.
# `python` runs the version check.
function(coding_standard_find_clang_format python)
    set(CODING_STANDARD_CLANG_FORMAT_PYTHON "${python}")
    if(BPR_CLANG_FORMAT_EXECUTABLE)
        set(cached_supported TRUE)
        coding_standard_clang_format_is_supported(cached_supported "${BPR_CLANG_FORMAT_EXECUTABLE}")
        if(NOT cached_supported)
            message(
                WARNING
                "BPR_CLANG_FORMAT_EXECUTABLE is ${BPR_CLANG_FORMAT_EXECUTABLE}, but the format gate needs "
                "clang-format 22 exactly (${CODING_STANDARD_CLANG_FORMAT_REJECTED}). Searching for a 22 "
                "instead. Install one with `brew install llvm@22` or `apt install clang-format-22`, or "
                "set -DBPR_CLANG_FORMAT_EXECUTABLE=<a clang-format 22>."
            )
            unset(BPR_CLANG_FORMAT_EXECUTABLE CACHE)
            unset(BPR_CLANG_FORMAT_EXECUTABLE)
        endif()
    endif()
    find_program(
        BPR_CLANG_FORMAT_EXECUTABLE
        NAMES clang-format-22 clang-format
        HINTS ${CODING_STANDARD_CLANG_FORMAT_HINTS}
        VALIDATOR coding_standard_clang_format_is_supported
        DOC "clang-format 22 for the coding-format targets and the coding_format_check test"
    )
endfunction()
