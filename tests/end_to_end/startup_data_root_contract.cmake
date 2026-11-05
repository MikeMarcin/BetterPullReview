cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1868]]. Two data roots nothing was wrong with closed the application at
# startup before any window, and both said only
# `STARTUP_FAILED_CAUSE: startup_failure=registry` and
# `STARTUP_PROFILE_REFUSAL:registry_unavailable`:
#
#   - a root typed in another letter case than its folders on a
#     case-insensitive volume (`.../BPR-attended/data` over `.../bpr-attended`),
#     whose canonical spelling then differed from the one given;
#   - a root that already existed before the first launch.
#
# This contract drives the shipped entry point with `--self-test` and
# `--test-application-data-root` and checks, in order:
#   1. A root made beforehand owner-private (mode 0700) starts.
#   2. [[BPR-1870]]. A root made beforehand with a shared mode (what
#      `mkdir -p` gives) and nothing in it is the application's own: it is
#      narrowed to 0700 and starts, and says so on its own line. One that
#      holds another program's file keeps its mode, and the data goes into a
#      private `BetterPullReview/data` inside it. A root that is a link is
#      refused and its target unchanged.
#   3. On a case-insensitive volume, a missing root under a folder spelled in
#      another letter case starts, and is created under the folder's own
#      spelling; then the same root spelled in a third case starts again.
#      On a case-sensitive volume the step reports that it did not apply.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_run_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

function(run_start label root output_name error_name result_name)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --self-test
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
            --test-application-data-root
            "${root}"
        RESULT_VARIABLE run_result
        OUTPUT_VARIABLE run_output
        ERROR_VARIABLE run_error
        TIMEOUT ${bpr_run_timeout}
    )
    set(${output_name} "${run_output}" PARENT_SCOPE)
    set(${error_name} "${run_error}" PARENT_SCOPE)
    set(${result_name} "${run_result}" PARENT_SCOPE)
endfunction()

function(require_started label root)
    run_start("${label}" "${root}" output error result)
    string(FIND "${output}" "STARTUP_FAILED" failed)
    if(NOT result EQUAL 0 OR NOT failed EQUAL -1)
        message(FATAL_ERROR
            "${label} did not start (${result})\n"
            "stdout:\n${output}\n"
            "stderr:\n${error}")
    endif()
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")
bpr_narrow_owner_private("${BPR_ROOT}")

# 1. Made beforehand, owner-private.
file(MAKE_DIRECTORY "${BPR_ROOT}/premade-private")
bpr_narrow_owner_private("${BPR_ROOT}/premade-private")
require_started("a root made beforehand with mode 0700" "${BPR_ROOT}/premade-private")

function(require_mode label path expected)
    execute_process(
        COMMAND stat "${bpr_stat_format}" "${bpr_stat_mode}" "${path}"
        RESULT_VARIABLE stat_result
        OUTPUT_VARIABLE stat_output
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT stat_result EQUAL 0 OR NOT stat_output STREQUAL "${expected}")
        message(FATAL_ERROR "${label}: mode '${stat_output}', expected '${expected}'")
    endif()
endfunction()

function(require_line label text expected)
    string(FIND "${text}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${label} did not say ${expected}\nstderr:\n${text}")
    endif()
endfunction()

if(CMAKE_HOST_APPLE)
    set(bpr_stat_format "-f")
    set(bpr_stat_mode "%Lp")
else()
    set(bpr_stat_format "-c")
    set(bpr_stat_mode "%a")
endif()

# 2. [[BPR-1870]]. Made beforehand with what `mkdir -p` gives, empty: the
#    application's own folder, so it is narrowed to 0700 and starts.
file(MAKE_DIRECTORY "${BPR_ROOT}/premade-shared")
file(CHMOD "${BPR_ROOT}/premade-shared"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
run_start("a root made beforehand with mode 0755" "${BPR_ROOT}/premade-shared" shared_output shared_error shared_result)
string(FIND "${shared_output}" "STARTUP_FAILED" shared_failed)
if(NOT shared_result EQUAL 0 OR NOT shared_failed EQUAL -1)
    message(FATAL_ERROR
        "an empty 0755 root made beforehand did not start (${shared_result})\n"
        "stdout:\n${shared_output}\n"
        "stderr:\n${shared_error}")
endif()
require_line("the narrowed root" "${shared_error}" "STORAGE_PRIVATE_REPAIRED:registry-root: ")
require_mode("the narrowed root" "${BPR_ROOT}/premade-shared" "700")
if(NOT EXISTS "${BPR_ROOT}/premade-shared/state/v1")
    message(FATAL_ERROR "the narrowed root holds no catalog")
endif()
# The folder above it is never changed.
require_mode("the folder above the narrowed root" "${BPR_ROOT}" "700")

# 2b. [[BPR-1870]]. A folder other programs use (it holds another file) keeps
#     its mode and its file; the data is kept in BetterPullReview/data inside
#     it, made 0700, and a second start uses the same folder.
file(MAKE_DIRECTORY "${BPR_ROOT}/premade-foreign")
file(WRITE "${BPR_ROOT}/premade-foreign/notes.txt" "another program's file\n")
file(CHMOD "${BPR_ROOT}/premade-foreign"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
file(CHMOD "${BPR_ROOT}/premade-foreign/notes.txt"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
foreach(attempt IN ITEMS first second)
    run_start("a ${attempt} start over a 0755 folder with another file" "${BPR_ROOT}/premade-foreign" foreign_output foreign_error foreign_result)
    string(FIND "${foreign_output}" "STARTUP_FAILED" foreign_failed)
    if(NOT foreign_result EQUAL 0 OR NOT foreign_failed EQUAL -1)
        message(FATAL_ERROR
            "the ${attempt} start over a folder with another file failed (${foreign_result})\n"
            "stdout:\n${foreign_output}\n"
            "stderr:\n${foreign_error}")
    endif()
    require_line("the nested root" "${foreign_error}" "STORAGE_PRIVATE_NESTED:registry-root: ")
endforeach()
require_mode("the folder with another file" "${BPR_ROOT}/premade-foreign" "755")
require_mode("the other program's file" "${BPR_ROOT}/premade-foreign/notes.txt" "644")
require_mode("the nested folder" "${BPR_ROOT}/premade-foreign/BetterPullReview" "700")
require_mode("the nested root" "${BPR_ROOT}/premade-foreign/BetterPullReview/data" "700")
if(NOT EXISTS "${BPR_ROOT}/premade-foreign/BetterPullReview/data/state/v1"
        OR EXISTS "${BPR_ROOT}/premade-foreign/state")
    message(FATAL_ERROR "the data was not kept in the nested root alone")
endif()
file(GLOB foreign_children RELATIVE "${BPR_ROOT}/premade-foreign" "${BPR_ROOT}/premade-foreign/*")
list(SORT foreign_children)
if(NOT foreign_children STREQUAL "BetterPullReview;notes.txt")
    message(FATAL_ERROR "the folder with another file gained entries: ${foreign_children}")
endif()

# 2b'. [[BPR-1870]]. A data folder copied or restored with ordinary modes
#      (folders 0755, files 0644) is still the application's own: every store
#      narrows its folders and files and the second start succeeds.
require_started("a first start over a private root" "${BPR_ROOT}/copied")
execute_process(COMMAND chmod -R go+rX "${BPR_ROOT}/copied" RESULT_VARIABLE loosen_result)
if(NOT loosen_result EQUAL 0)
    message(FATAL_ERROR "the copied root could not be loosened")
endif()
require_mode("the loosened copy" "${BPR_ROOT}/copied/state/v1" "755")
run_start("a start over a loosened copy" "${BPR_ROOT}/copied" copied_output copied_error copied_result)
string(FIND "${copied_output}" "STARTUP_FAILED" copied_failed)
if(NOT copied_result EQUAL 0 OR NOT copied_failed EQUAL -1)
    message(FATAL_ERROR
        "a loosened copy of a data folder did not start (${copied_result})\n"
        "stdout:\n${copied_output}\n"
        "stderr:\n${copied_error}")
endif()
foreach(entry IN ITEMS "" "/state" "/state/v1" "/logs" "/agent-workspace-ownership-v1")
    require_mode("the copy's ${entry}" "${BPR_ROOT}/copied${entry}" "700")
endforeach()
require_mode("the copy's catalog" "${BPR_ROOT}/copied/state/v1/epoch-catalog.sqlite3" "600")
require_mode("the copy's registry store" "${BPR_ROOT}/copied/state/v1/profile-registry.sqlite3" "600")

# 2c. [[BPR-1870]]. A root that is a link is still refused, and what it
#     names is not changed.
file(MAKE_DIRECTORY "${BPR_ROOT}/link-target")
file(CHMOD "${BPR_ROOT}/link-target"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
file(CREATE_LINK "${BPR_ROOT}/link-target" "${BPR_ROOT}/linked-root" SYMBOLIC)
run_start("a root that is a link" "${BPR_ROOT}/linked-root" linked_output linked_error linked_result)
if(NOT linked_result EQUAL 1 OR NOT linked_output STREQUAL "STARTUP_FAILED\n")
    message(FATAL_ERROR
        "a linked root was not refused (${linked_result})\n"
        "stdout:\n${linked_output}\n"
        "stderr:\n${linked_error}")
endif()
require_line("the linked root's refusal" "${linked_error}" "STARTUP_FAILURE_SOURCE:registry-root-link: ")
require_mode("the linked root's target" "${BPR_ROOT}/link-target" "755")
file(GLOB linked_children "${BPR_ROOT}/link-target/*")
if(NOT linked_children STREQUAL "")
    message(FATAL_ERROR "the linked root's target was written to: ${linked_children}")
endif()

# 3. Another letter case, on a case-insensitive volume only.
file(MAKE_DIRECTORY "${BPR_ROOT}/Attended")
if(EXISTS "${BPR_ROOT}/attended")
    require_started("a missing root under a folder spelled in another case" "${BPR_ROOT}/attended/data")
    file(GLOB spelled RELATIVE "${BPR_ROOT}" "${BPR_ROOT}/*ttended")
    file(GLOB created RELATIVE "${BPR_ROOT}/Attended" "${BPR_ROOT}/Attended/data")
    if(NOT spelled STREQUAL "Attended" OR NOT created STREQUAL "data")
        message(FATAL_ERROR
            "the root was not created under the folder's own spelling: "
            "folders '${spelled}', root '${created}'")
    endif()
    require_started("an existing root spelled in a third case" "${BPR_ROOT}/ATTENDED/data")
    set(bpr_case_step "case-insensitive")
else()
    set(bpr_case_step "case-sensitive-not-applicable")
endif()

file(REMOVE_RECURSE "${BPR_ROOT}")
message("STARTUP_DATA_ROOT_OK:${bpr_case_step}")
