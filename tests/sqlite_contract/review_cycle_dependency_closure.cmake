# RFC-BPR-0053 RC53-06 / RFC-BPR-0060. The local Review Cycles runtime and its
# store must link without any agent, MCP, AI, GitHub, Git or Qt component.
# CLOSURE is the configure-time link closure of the runtime test executable,
# joined with "|".
if(NOT DEFINED CLOSURE OR CLOSURE STREQUAL "")
    message(FATAL_ERROR "REVIEW_CYCLE_DEPENDENCY_CLOSURE_FAILED: no closure supplied")
endif()
string(REPLACE "|" ";" closure "${CLOSURE}")
set(required better_pull_review_core better_pull_review_application better_pull_review_adapter_sqlite)
foreach(target IN LISTS required)
    if(NOT target IN_LIST closure)
        message(FATAL_ERROR "REVIEW_CYCLE_DEPENDENCY_CLOSURE_FAILED: ${target} missing from ${CLOSURE}")
    endif()
endforeach()
foreach(entry IN LISTS closure)
    string(TOLOWER "${entry}" lowered)
    if(lowered MATCHES "(adapter_?agent|adapter_?mcp|adapter_?ai|adapter_?github|adapter_?git([^h]|$)|adapter_?qt|qt[0-9]*::|qt6|composition|tour_?runtime)")
        message(FATAL_ERROR "REVIEW_CYCLE_DEPENDENCY_CLOSURE_FAILED: ${entry} is in the local runtime's link closure")
    endif()
endforeach()
message(STATUS "REVIEW_CYCLE_DEPENDENCY_CLOSURE_OK: ${CLOSURE}")
