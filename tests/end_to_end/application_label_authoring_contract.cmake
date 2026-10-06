cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1083]]. Conventional Comments labels in the composed application over
# the constructed conventional-comments-v1 pull request, signed in with the
# fixture's account: Read with no file chosen shows the direction to open one
# ([[BPR-1287]]), src/engine/renderer.cpp opens in Read with its labelled
# threads marked, Comment opens an editable page on a line no thread is on,
# `:qu:` typed into the empty body becomes `question: `, the chip's menu
# makes it a suggestion and ⌘L's menu an issue, Publish posts it, and after
# a refresh the new thread carries the issue mark.
# [[BPR-1084]]. Then suggested changes on the same path: an issue with a
# multiline suggestion on a free line is marked native before Publish and the
# fixture server receives its exact bytes and line; a suggestion with no patch
# shows no mark and publishes exactly its prose; a patch on a line the
# fixture's GitHub refuses an inline comment for is refused (incompatible_body)
# after one native attempt on that exact line, with no file comment, its text
# kept and the mark then saying text only; and a reply written in place in a
# thread on a current line is marked native in both editors.

foreach(required IN ITEMS BPR_APPLICATION BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "${BPR_APPLICATION}"
        --github-fixture
        conventional-comments-v1
        --label-authoring-probe
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
    TIMEOUT ${bpr_timeout}
)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "label authoring process failed (${probe_result})\n"
        "stdout:\n${probe_output}\n"
        "stderr:\n${probe_error}"
    )
endif()
foreach(marker IN ITEMS
    PUBLIC_PATH_OK
    LABEL_AUTHORING_OK
    LABEL_AUTHORING_EVIDENCE:11111111111111111111
)
    string(FIND "${probe_output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "label authoring process omitted ${marker}\n"
            "stdout:\n${probe_output}"
        )
    endif()
endforeach()

message(STATUS "LABEL_AUTHORING_CONTRACT_OK")
