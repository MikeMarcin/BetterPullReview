if(NOT DEFINED BPR_APPLICATION OR NOT DEFINED BPR_ROOT OR NOT DEFINED BPR_SOURCE)
    message(FATAL_ERROR "native notification contract requires application, root, and source")
endif()

file(READ "${BPR_SOURCE}/src/app/main.cpp" course_source)
foreach(forbidden IN ITEMS "deliver\\(" "select_queue_item\\(" "Notifications->activate")
    if(course_source MATCHES "${forbidden}")
        message(FATAL_ERROR "native notification course bypasses a production wire: ${forbidden}")
    endif()
endforeach()

file(READ
    "${BPR_SOURCE}/src/app/src/github_application.cpp"
    application_source
)
string(FIND
    "${application_source}"
    "activate_platform_notification("
    activation_start
)
string(FIND
    "${application_source}"
    "maintenance_evidence("
    activation_end
)
if(activation_start LESS 0 OR activation_end LESS_EQUAL activation_start)
    message(FATAL_ERROR "native notification helper boundary is missing")
endif()
math(EXPR activation_length "${activation_end} - ${activation_start}")
string(SUBSTRING
    "${application_source}"
    ${activation_start}
    ${activation_length}
    activation_helper
)
if(NOT activation_helper MATCHES
        "SystemNotificationSink->tray_icon\\(\\)->messageClicked\\(\\);"
    OR NOT activation_helper MATCHES "== activations \\+ 1U;"
)
    message(FATAL_ERROR
        "native notification helper does not emit and attribute the tray signal"
    )
endif()
foreach(forbidden IN ITEMS "Notifications->activate\\(" "select_queue_item\\(")
    if(activation_helper MATCHES "${forbidden}")
        message(FATAL_ERROR
            "native notification helper bypasses a production wire: ${forbidden}"
        )
    endif()
endforeach()

foreach(course IN ITEMS positive request-cut activation-cut)
    string(REPLACE "-" "_" course_name "${course}")
    set(course_root "${BPR_ROOT}_${course_name}")
    file(REMOVE_RECURSE "${course_root}")
    file(MAKE_DIRECTORY "${course_root}")
    set(course_arguments)
    if(course STREQUAL "request-cut")
        list(APPEND course_arguments "--native-notification-cut-request-wire")
    elseif(course STREQUAL "activation-cut")
        list(APPEND course_arguments "--native-notification-cut-activation-wire")
    endif()
    execute_process(
        COMMAND
            "${BPR_APPLICATION}"
            --github-fixture github-com
            --epoch-fixture-root "${course_root}"
            --epoch-fixture-phase seed
            --native-notification-probe
            ${course_arguments}
            --automated-run-budget-ms 25000
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 25
    )
    if(course STREQUAL "positive")
        set(marker "NATIVE_NOTIFICATION_ATTENDED_CONTRACT_OK")
    elseif(course STREQUAL "request-cut")
        set(marker "NATIVE_NOTIFICATION_REQUEST_CUT_OK")
    else()
        set(marker "NATIVE_NOTIFICATION_ACTIVATION_CUT_OK")
    endif()
    if(NOT result EQUAL 0 OR NOT output MATCHES "${marker}")
        message(FATAL_ERROR "native notification ${course} failed: ${output}${error}")
    endif()
    if(output MATCHES "NATIVE_NOTIFICATION_REFUSED:")
        message(FATAL_ERROR "native notification ${course} refused: ${output}${error}")
    endif()
    if(course STREQUAL "request-cut")
        if(output MATCHES "NATIVE_NOTIFICATION_QT_ACTIVE_WINDOW_OK")
            message(FATAL_ERROR "native notification request cut reached activation")
        endif()
    elseif(NOT output MATCHES "NATIVE_NOTIFICATION_QT_ACTIVE_WINDOW_OK")
        message(FATAL_ERROR "native notification ${course} omitted Qt activation")
    endif()
    if(course STREQUAL "activation-cut"
        AND output MATCHES "NATIVE_NOTIFICATION_EXACT_LANDING_SELECTION_OK"
    )
        message(FATAL_ERROR "native notification activation cut reached selection")
    endif()
    if(error MATCHES "QSystemTrayIcon::setVisible: No Icon set")
        message(FATAL_ERROR "native notification ${course} showed a tray without an icon")
    endif()
endforeach()

message("NATIVE_NOTIFICATION_ATTENDED_CONTRACT_OK")
