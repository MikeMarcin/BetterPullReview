add_test(
    NAME application_public_path
    COMMAND
        $<TARGET_FILE:better_pull_review>
        --self-test
)
set_tests_properties(
    application_public_path
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "PUBLIC_PATH_OK"
        TIMEOUT 15
)

add_test(
    NAME application_render
    COMMAND
        $<TARGET_FILE:better_pull_review>
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/foundation-window.png"
)
set_tests_properties(
    application_render
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25"
        PASS_REGULAR_EXPRESSION "RENDER_OK"
        TIMEOUT 15
)
