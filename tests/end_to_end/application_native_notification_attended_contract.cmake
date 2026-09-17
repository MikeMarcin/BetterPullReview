if(NOT DEFINED BPR_APPLICATION OR NOT DEFINED BPR_ROOT OR NOT DEFINED BPR_SOURCE)
    message(FATAL_ERROR "native notification contract requires application, root, and source")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")

file(READ "${BPR_SOURCE}/src/app/main.cpp" course_source)
foreach(forbidden IN ITEMS
    "deliver\\("
    "select_queue_item\\("
    "Notifications->activate"
    "activate_platform_notification"
)
    if(course_source MATCHES "${forbidden}")
        message(FATAL_ERROR "native notification course bypasses a production wire: ${forbidden}")
    endif()
endforeach()
if(NOT course_source MATCHES "PlatformTray->messageClicked\\(\\);")
    message(FATAL_ERROR "native notification course omits the real tray signal")
endif()

# BPR-0392. The one precondition this contract could not check, and the one that
# was false: macOS Notification Center registers applications by bundle
# identity, so an unbundled process cannot request authorization, never gains a
# System Settings entry, and its delivery requests go nowhere. Every marker
# above stayed green from a bare Mach-O that no banner could ever have been
# addressed to.
#
# The expected identifier is exact rather than merely non-`none`. A tests-enabled
# build that reused the shipped identity would also be non-`none`, and it could
# grant or consume the shipped application's Notification Center permission -
# a different defect wearing the same green marker.
#
# The course refuses on its own when the identity is absent, before it arms
# anything, so this is the second of two gates rather than the only one. What it
# adds is the exact identifier, which the course cannot check without hard-coding
# the build's own answer.
if(APPLE)
    set(expected_bundle_identity "io.github.MikeMarcin.BetterPullReview.tests")
else()
    set(expected_bundle_identity "not-applicable")
endif()

# [[BPR-0875]]. The data root moves inside an owned parent, because the build
# tree this course roots in is writable by other principals and the agent
# workspace neighbourhood refuses such a parent.
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_private_data_root(BPR_ROOT)

foreach(course IN ITEMS positive request-cut activation-cut)
    string(REPLACE "-" "_" course_name "${course}")
    set(course_root "${BPR_ROOT}_${course_name}")
    file(REMOVE_RECURSE "${course_root}")
    file(MAKE_DIRECTORY "${course_root}")
    # The registry refuses a pre-created root it did not narrow itself (BPR-0723).
    bpr_narrow_owner_private("${course_root}")
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
    if(NOT output MATCHES "NATIVE_NOTIFICATION_BUNDLE_IDENTITY:([^\r\n]+)")
        message(FATAL_ERROR
            "native notification ${course} reported no bundle identity: "
            "${output}${error}"
        )
    endif()
    set(observed_bundle_identity "${CMAKE_MATCH_1}")
    if(observed_bundle_identity STREQUAL "none")
        message(FATAL_ERROR
            "native notification ${course} ran without bundle identity, so "
            "Notification Center cannot register it and no banner it requests "
            "can appear: ${output}${error}"
        )
    endif()
    if(NOT observed_bundle_identity STREQUAL "${expected_bundle_identity}")
        message(FATAL_ERROR
            "native notification ${course} reported bundle identity "
            "'${observed_bundle_identity}' rather than the expected "
            "'${expected_bundle_identity}': ${output}${error}"
        )
    endif()
    if(course STREQUAL "request-cut")
        if(output MATCHES "NATIVE_NOTIFICATION_QT_ACTIVE_WINDOW_OK")
            message(FATAL_ERROR "native notification request cut reached activation")
        endif()
        if(output MATCHES "NATIVE_NOTIFICATION_TRAY_SIGNAL_OK")
            message(FATAL_ERROR "native notification request cut emitted tray signal")
        endif()
    elseif(NOT output MATCHES "NATIVE_NOTIFICATION_QT_ACTIVE_WINDOW_OK"
        OR NOT output MATCHES "NATIVE_NOTIFICATION_TRAY_SIGNAL_OK"
    )
        message(FATAL_ERROR
            "native notification ${course} omitted Qt or tray-signal activation"
        )
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

set(signal_cut_root "${BPR_ROOT}_signal_observation_cut")
file(REMOVE_RECURSE "${signal_cut_root}")
file(MAKE_DIRECTORY "${signal_cut_root}")
bpr_narrow_owner_private("${signal_cut_root}")
execute_process(
    COMMAND
        "${BPR_APPLICATION}"
        --github-fixture github-com
        --epoch-fixture-root "${signal_cut_root}"
        --epoch-fixture-phase seed
        --native-notification-probe
        --native-notification-cut-signal-observation
        --automated-run-budget-ms 25000
    RESULT_VARIABLE signal_cut_result
    OUTPUT_VARIABLE signal_cut_output
    ERROR_VARIABLE signal_cut_error
    TIMEOUT 25
)
if(signal_cut_result EQUAL 0
    OR NOT signal_cut_output MATCHES
        "NATIVE_NOTIFICATION_REFUSED:tray_signal_count_mismatch"
    OR signal_cut_output MATCHES "NATIVE_NOTIFICATION_TRAY_SIGNAL_OK"
)
    message(FATAL_ERROR
        "native notification signal-evidence cut false-greened: "
        "${signal_cut_output}${signal_cut_error}"
    )
endif()

message("NATIVE_NOTIFICATION_ATTENDED_CONTRACT_OK")
