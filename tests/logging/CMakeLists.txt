if(BPR_BUILD_QT)
    bpr_add_executable(test_quill_logging quill_logging_tests.cpp)
    target_link_libraries(test_quill_logging PRIVATE BetterPullReview::AdapterLogging BetterPullReview::AdapterGitHub BetterPullReview::AdapterSQLite Qt6::Core)
    target_include_directories(test_quill_logging PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
    bpr_enable_hardening(test_quill_logging)
    bpr_enable_warnings(test_quill_logging)
    # Uncontained on Windows for the measured dos-volume-name cause ([[BPR-0885]]):
    # contained, the test's own private root setup is refused before the logger runs.
    bpr_add_network_isolated_test(NAME quill_logging WINDOWS_UNCONTAINED COMMAND test_quill_logging)
    if(UNIX)
        bpr_add_network_isolated_test(NAME quill_logging_backpressure COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/verify_logging_backpressure.py" "$<TARGET_FILE:test_quill_logging>")
    endif()
endif()
