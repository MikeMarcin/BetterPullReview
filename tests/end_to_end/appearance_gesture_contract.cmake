cmake_minimum_required(VERSION 4.4.2)

# Drives the composed appearance settings surface across separate processes.
#
# The claim RFC-BPR-0006 makes is two claims: a setting previews immediately,
# and it is still there after a restart. Neither can be shown inside one
# process, and neither can be shown by a probe that only reports what is set
# now — a run that defaulted to the value it was checking for would pass.
#
# So three phases. The first establishes what a composition with no durable
# record starts from, which is what makes the third phase's answer mean
# something. The second makes the gesture. The third is a new process against
# the same application data root, making no gesture at all.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# A backstop, not the bound. Each phase bounds itself at `BPR_RUN_BUDGET_MS`
# and reports what it was doing; this only catches a process that never reached
# its own limit, and is derived from that limit so it can never fire first.
math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")

# [[BPR-0875]]. The data root moves inside an owned parent, because the build
# tree this course roots in is writable by other principals and the agent
# workspace neighbourhood refuses such a parent.
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_private_data_root(BPR_ROOT)

function(run_phase label expected)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --github-fixture
            github-com
            --epoch-fixture-root
            "${BPR_ROOT}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
            ${ARGN}
        RESULT_VARIABLE phase_result
        OUTPUT_VARIABLE phase_output
        ERROR_VARIABLE phase_error
        TIMEOUT ${bpr_phase_timeout}
    )
    if(NOT phase_result EQUAL 0)
        # [[BPR-0555]]. The child dumps its whole marker block below, and
        # `RUN_BUDGET_EXPIRED` is in it, but a reader sees the headline. A run
        # that merely ran out of time on a busy host said "label process
        # failed" and read as a defect in the phase; say which it was.
        set(bpr_budget_headline "")
        if(phase_output MATCHES "RUN_BUDGET_EXPIRED:[^\n]*")
            set(bpr_budget_headline "${CMAKE_MATCH_0}\n")
        endif()
        message(FATAL_ERROR
            "${label} process failed (${phase_result})\n"
            "${bpr_budget_headline}"
            "stdout:\n${phase_output}\n"
            "stderr:\n${phase_error}")
    endif()
    foreach(marker IN LISTS expected)
        string(FIND "${phase_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "${label} omitted ${marker}\n"
                "stdout:\n${phase_output}\n"
                "stderr:\n${phase_error}")
        endif()
    endforeach()
    # The two surfaces validate typography with the same core function, so any
    # disagreement between them is a defect rather than a preference. Checked
    # on every phase because the divergence it names would otherwise be silent.
    string(FIND "${phase_output}" "APPEARANCE_DIVERGENCE:0" divergence)
    if(divergence EQUAL -1)
        message(FATAL_ERROR
            "${label} reported a presentation/host disagreement about "
            "typography legality\nstdout:\n${phase_output}")
    endif()
endfunction()

# Phase one. A fresh root, and no gesture: this is what the composition starts
# from. Without it, phase three proves nothing, because a build whose default
# happened to be dark with a tab width of eight would satisfy it.
file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")
bpr_narrow_owner_private("${BPR_ROOT}")
run_phase(
    "fresh composition"
    "PUBLIC_PATH_OK;APPEARANCE_SETTINGS:system:4"
    --epoch-fixture-phase
    seed
)

# Phase two. A fresh root again, so the gesture is the only thing that could
# have moved the settings off the default phase one just established.
file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")
bpr_narrow_owner_private("${BPR_ROOT}")
run_phase(
    "settings gesture"
    "PUBLIC_PATH_OK;APPEARANCE_GESTURE_OK;APPEARANCE_REFUSAL_OK;APPEARANCE_SETTINGS:dark:8"
    --epoch-fixture-phase
    seed
    --appearance-gesture-probe
)

# Phase three. A separate execution of the real binary against the root phase
# two wrote, making no gesture. This is the restart claim: a new process, not a
# reopened handle.
run_phase(
    "restart"
    "PUBLIC_PATH_OK;APPEARANCE_SETTINGS:dark:8"
    --epoch-fixture-phase
    reuse
)

file(REMOVE_RECURSE "${BPR_ROOT}")
message("APPEARANCE_GESTURE_CONTRACT_OK")
