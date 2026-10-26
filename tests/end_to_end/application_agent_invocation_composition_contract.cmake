if(
    NOT DEFINED BPR_APPLICATION
    OR NOT DEFINED BPR_APPLICATION_SOURCE
    OR NOT DEFINED BPR_GIT_WRAPPER
    OR NOT DEFINED BPR_CANARY
    OR NOT DEFINED BPR_ROOT
)
    message(FATAL_ERROR "agent invocation contract inputs are required")
endif()

set(seed_marker "agent-invocation-composition-seed:pass")
set(reopen_marker "agent-invocation-composition-reopen:pass")
if(seed_marker STREQUAL reopen_marker)
    message(FATAL_ERROR "agent invocation phase markers must be distinct")
endif()

file(READ "${BPR_APPLICATION_SOURCE}" application_source)
foreach(
    required_source
    IN ITEMS
        "course == \"positive-seed\""
        "if (course == \"positive-reopen\")"
        "${seed_marker}"
        "${reopen_marker}"
)
    string(FIND "${application_source}" "${required_source}" source_position)
    if(source_position EQUAL -1)
        message(
            FATAL_ERROR
            "agent invocation public source is missing ${required_source}"
        )
    endif()
endforeach()

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")
# [[BPR-0622]]. The application creates its agent workspace neighbourhood
# beside its data root only under a parent no other principal can write. A
# build tree inherits whatever its checkout grants (on Windows, commonly
# Authenticated Users modify), so the data root sits in a narrowed parent.
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_narrow_owner_private("${BPR_ROOT}")
set(application_data_root "${BPR_ROOT}/data")

execute_process(
    COMMAND
        "${BPR_APPLICATION}"
        positive-seed
        "${BPR_CANARY}"
        "${application_data_root}"
    RESULT_VARIABLE seed_result
    OUTPUT_VARIABLE seed_output
    ERROR_VARIABLE seed_error
)
message("${seed_output}")
if(NOT seed_result EQUAL 0)
    message(FATAL_ERROR "agent invocation seed failed: ${seed_error}")
endif()
string(FIND "${seed_output}" "${seed_marker}" seed_marker_position)
string(FIND "${seed_output}" "${reopen_marker}" seed_reopen_position)
if(seed_marker_position EQUAL -1 OR NOT seed_reopen_position EQUAL -1)
    message(FATAL_ERROR "agent invocation seed emitted no distinct seed proof")
endif()

execute_process(
    COMMAND
        "${BPR_APPLICATION}"
        positive-reopen
        "${BPR_CANARY}"
        "${application_data_root}"
    RESULT_VARIABLE reopen_result
    OUTPUT_VARIABLE reopen_output
    ERROR_VARIABLE reopen_error
)
message("${reopen_output}")
if(NOT reopen_result EQUAL 0)
    message(FATAL_ERROR "agent invocation reopen failed: ${reopen_error}")
endif()
string(FIND "${reopen_output}" "${reopen_marker}" reopen_marker_position)
string(FIND "${reopen_output}" "${seed_marker}" reopen_seed_position)
if(reopen_marker_position EQUAL -1 OR NOT reopen_seed_position EQUAL -1)
    message(FATAL_ERROR "agent invocation reopen emitted no distinct reopen proof")
endif()

# [[BPR-1769]]. Real first-run UI, saved preset wrapper, supervised
# availability, workspace preparation, and independent failing-row disclosures.
execute_process(
    COMMAND "${BPR_APPLICATION}" setup-wrapper "${BPR_CANARY}" "${BPR_GIT_WRAPPER}"
    RESULT_VARIABLE setup_result
    OUTPUT_VARIABLE setup_output
    ERROR_VARIABLE setup_error
)
message("${setup_output}")
if(NOT setup_result EQUAL 0 OR NOT setup_output MATCHES "agent-invocation-setup-wrapper:pass")
    message(FATAL_ERROR "wrapper setup public path failed: ${setup_error}")
endif()

message("AGENT_INVOCATION_COMPOSITION_CONTRACT_OK")
