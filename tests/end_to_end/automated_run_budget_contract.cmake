cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS BPR_APPLICATION BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR process_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 5")
execute_process(
    COMMAND "${BPR_APPLICATION}" --self-test --composition-capability-probe
        --automated-run-budget-ms "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE success_result OUTPUT_VARIABLE success_output
    ERROR_VARIABLE success_error TIMEOUT ${process_timeout}
)
if(NOT "${success_result}" STREQUAL "0"
        OR NOT success_output MATCHES "PUBLIC_PATH_BUDGET:expired=0 budget_ms=${BPR_RUN_BUDGET_MS} elapsed_ms=[0-9]+ stage_ticks=[0-9]+[\r\n]+PUBLIC_PATH_OK"
        OR success_output MATCHES "RUN_BUDGET_EXPIRED:")
    message(FATAL_ERROR "passing run omitted honest timing (${success_result})\n${success_output}\n${success_error}")
endif()

execute_process(
    COMMAND "${BPR_APPLICATION}" --github-fixture github-com
        --automated-stall-probe --automated-run-budget-ms "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE expiry_result OUTPUT_VARIABLE expiry_output
    ERROR_VARIABLE expiry_error TIMEOUT ${process_timeout}
)
# Stage 2 is the deliberately stalled `open` stage. Keep both its actual
# failure and the clock cause; neither a successful run nor a harness kill
# can satisfy this course.
if("${expiry_result}" STREQUAL "0"
        OR NOT expiry_output MATCHES "RUN_BUDGET_EXPIRED:elapsed_ms=[0-9]+ budget_ms=${BPR_RUN_BUDGET_MS} over_ms=[0-9]+ stage=2[^\r\n]*[\r\n]+PUBLIC_PATH_BUDGET:expired=1 budget_ms=${BPR_RUN_BUDGET_MS} elapsed_ms=[0-9]+ stage_ticks=[0-9]+[\r\n]+PUBLIC_PATH_FAILED"
        OR NOT expiry_output MATCHES "PUBLIC_PATH_STAGE:2[\r\n]+PUBLIC_PATH_VERDICT:[^\r\n]+[\r\n]+PUBLIC_PATH_GROUPS:")
    message(FATAL_ERROR "stalled run omitted its expiry cause (${expiry_result})\n${expiry_output}\n${expiry_error}")
endif()
message(STATUS "AUTOMATED_RUN_BUDGET_CONTRACT_OK")
