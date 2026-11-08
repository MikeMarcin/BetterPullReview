# [[BPR-1942]]. Only writable test data moves; build/source inputs stay put.
include_guard(GLOBAL)

# STRING deliberately preserves a relative input so validation can refuse it
# before CMake's PATH cache conversion silently makes it absolute.
set(BPR_TEST_RUNTIME_ROOT "" CACHE STRING
    "Existing canonical owner-private POSIX directory for writable E2E test roots")

function(bpr_test_runtime_directory out_variable default_directory)
    if(BPR_TEST_RUNTIME_ROOT STREQUAL "")
        set("${out_variable}" "${default_directory}" PARENT_SCOPE)
        return()
    endif()
    get_target_property(bpr_runtime_python Python3::Interpreter IMPORTED_LOCATION)
    execute_process(
        COMMAND "${bpr_runtime_python}" -I -S
            "${PROJECT_SOURCE_DIR}/scripts/validate_test_runtime_root.py"
            --root "${BPR_TEST_RUNTIME_ROOT}"
            --source-root "${PROJECT_SOURCE_DIR}"
            --build-root "${PROJECT_BINARY_DIR}"
        RESULT_VARIABLE bpr_runtime_result
        ERROR_VARIABLE bpr_runtime_error
        OUTPUT_QUIET
    )
    if(NOT bpr_runtime_result STREQUAL "0")
        message(FATAL_ERROR "BPR_TEST_RUNTIME_ROOT refused: ${bpr_runtime_error}")
    endif()
    set("${out_variable}" "${BPR_TEST_RUNTIME_ROOT}" PARENT_SCOPE)
endfunction()
