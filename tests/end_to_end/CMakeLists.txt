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
