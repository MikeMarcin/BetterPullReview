cmake_minimum_required(VERSION 4.4.2)

# [[BPR-0843]]. The profile storage root has to be canonical, so a fixture root
# spelled through a symbolic link is refused at startup. The refusal is right,
# but it printed only `STARTUP_FAILED`. On macOS, where `/var` is a link to
# `/private/var`, that cost a series of seed and reuse runs to find.
#
# This contract checks three things, in order:
#   1. The root spelled through a link is refused. The stdout verdict is still
#      exactly the one word, and a developer build names the typed
#      `startup_failure` on stderr.
#   2. The same directory spelled canonically still seeds, so the refusal
#      comes from the spelling and nothing else.
#   3. The spelling really is non-canonical, so check 1 cannot pass on a link
#      that resolved to itself.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# A backstop, not the bound: the application bounds itself at
# `BPR_RUN_BUDGET_MS`, and this only catches a process that never reached it.
math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

set(bpr_expected_cause "registry")

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}/canonical/root")
file(REAL_PATH "${BPR_ROOT}" bpr_root)
set(bpr_canonical_root "${bpr_root}/canonical/root")
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_narrow_owner_private("${bpr_canonical_root}")
file(CREATE_LINK "${bpr_root}/canonical" "${bpr_root}/spelled" SYMBOLIC)
set(bpr_spelled_root "${bpr_root}/spelled/root")
file(REAL_PATH "${bpr_spelled_root}" bpr_spelled_resolved)
if(NOT IS_SYMLINK "${bpr_root}/spelled"
    OR bpr_spelled_resolved STREQUAL bpr_spelled_root
    OR NOT bpr_spelled_resolved STREQUAL bpr_canonical_root)
    message(FATAL_ERROR
        "the linked spelling ${bpr_spelled_root} is not a non-canonical name "
        "for ${bpr_canonical_root} (resolved to ${bpr_spelled_resolved})")
endif()

execute_process(
    COMMAND
        "${BPR_APPLICATION}"
        --github-fixture
        github-com
        --epoch-fixture-root
        "${bpr_spelled_root}"
        --epoch-fixture-phase
        seed
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE refused_result
    OUTPUT_VARIABLE refused_output
    ERROR_VARIABLE refused_error
    TIMEOUT ${bpr_phase_timeout}
)
if(NOT refused_result EQUAL 1 OR NOT refused_output STREQUAL "STARTUP_FAILED\n")
    message(FATAL_ERROR
        "the linked spelling was not refused with the one-word verdict "
        "(${refused_result})\n"
        "stdout:\n${refused_output}\n"
        "stderr:\n${refused_error}")
endif()
string(FIND "${refused_error}"
    "STARTUP_FAILED_CAUSE: startup_failure=${bpr_expected_cause}\n"
    bpr_cause_index
)
if(bpr_cause_index EQUAL -1)
    message(FATAL_ERROR
        "the refusal did not name startup_failure=${bpr_expected_cause}\n"
        "stdout:\n${refused_output}\n"
        "stderr:\n${refused_error}")
endif()

execute_process(
    COMMAND
        "${BPR_APPLICATION}"
        --github-fixture
        github-com
        --epoch-fixture-root
        "${bpr_canonical_root}"
        --epoch-fixture-phase
        seed
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE control_result
    OUTPUT_VARIABLE control_output
    ERROR_VARIABLE control_error
    TIMEOUT ${bpr_phase_timeout}
)
if(NOT control_result EQUAL 0)
    message(FATAL_ERROR
        "the canonical spelling did not seed (${control_result})\n"
        "stdout:\n${control_output}\n"
        "stderr:\n${control_error}")
endif()
foreach(expected IN ITEMS
        "PUBLIC_PATH_OK"
        "EPOCH_FIXTURE_SEED_OK"
        "GITHUB_FIXTURE_OK:github.com")
    string(FIND "${control_output}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "the canonical spelling omitted ${expected}\n"
            "stdout:\n${control_output}\n"
            "stderr:\n${control_error}")
    endif()
endforeach()
file(REMOVE_RECURSE "${BPR_ROOT}")

message("EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:${bpr_expected_cause}")
