cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1478]]. A reread of the shown pull request lands just after the
# reviewer picks another file, in the composed application over the
# constructed conventional-comments-v1 pull request. Review shows
# src/engine/renderer.cpp with a line chosen. The read starts from the public
# Refresh (refresh) or from the window's own reread after a publication
# (reread), and its answer is held. The reviewer picks src/engine/renderer.h
# in the files drawer while the window's reflection of that pick into the
# active tab is held too, then the read lands, then the reflection runs. The
# window must settle on renderer.h, not go back to renderer.cpp.
#
# Evidence: the read landed between the pick and the tab taking it; Review
# then shows renderer.h, chosen, in the active tab, with its diff; twenty
# ticks later it still does. Both courses failed `pick-lost` before BPR-1478.

foreach(required IN ITEMS BPR_APPLICATION BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

foreach(course IN ITEMS refresh reread)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --github-fixture
            conventional-comments-v1
            --refresh-pick-course
            "${course}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "refresh pick ${course} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        REFRESH_PICK_OK
        "REFRESH_PICK_EVIDENCE:111\n"
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "refresh pick ${course} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "REFRESH_PICK_CONTRACT_OK")
