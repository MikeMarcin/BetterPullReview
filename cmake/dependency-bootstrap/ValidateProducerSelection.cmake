cmake_minimum_required(VERSION 4.4.2)

if(
    NOT DEFINED BPR_CPS_DOCUMENT
    OR NOT DEFINED BPR_POLICY_ORACLE_DIRECTORY
)
    message(FATAL_ERROR "CPS producer-selection validation inputs are incomplete")
endif()

file(GLOB policy_oracles "${BPR_POLICY_ORACLE_DIRECTORY}/*.txt")
if(NOT policy_oracles)
    message(STATUS "CPS_PRODUCER_SELECTION_OK:0")
    return()
endif()
if(NOT EXISTS "${BPR_CPS_DOCUMENT}")
    message(FATAL_ERROR "CPS producer-selection document is missing")
endif()
file(READ "${BPR_CPS_DOCUMENT}" cps_document)
set(validated_count 0)
foreach(policy_oracle IN LISTS policy_oracles)
    get_filename_component(oracle_name "${policy_oracle}" NAME_WE)
    if(NOT oracle_name MATCHES "^([A-Za-z0-9_]+)-(.+)$")
        message(FATAL_ERROR "Malformed CPS producer-selection oracle: '${policy_oracle}'")
    endif()
    set(component "${CMAKE_MATCH_1}")
    string(TOLOWER "${CMAKE_MATCH_2}" configuration)
    file(READ "${policy_oracle}" producer_location)
    string(STRIP "${producer_location}" producer_location)
    string(
        JSON cps_location
        ERROR_VARIABLE location_error
        GET
            "${cps_document}"
            components "${component}" configurations "${configuration}" location
    )
    if(location_error)
        message(
            FATAL_ERROR
            "CPS producer-selection oracle has no matching document location: "
            "${component}/${configuration}"
        )
    endif()
    if(
        producer_location STREQUAL ""
        OR producer_location MATCHES "-NOTFOUND$"
        OR NOT EXISTS "${producer_location}"
    )
        message(
            FATAL_ERROR
            "Producer target has no valid native artifact for "
            "${component}/${configuration}: '${producer_location}'"
        )
    endif()
    file(REAL_PATH "${producer_location}" producer_location_real)
    file(REAL_PATH "${cps_location}" cps_location_real)
    if(NOT producer_location_real STREQUAL cps_location_real)
        message(
            FATAL_ERROR
            "CPS producer selection differs from native CMake for "
            "${component}/${configuration}: producer='${producer_location}', "
            "cps='${cps_location}'"
        )
    endif()
    math(EXPR validated_count "${validated_count} + 1")
endforeach()

message(STATUS "CPS_PRODUCER_SELECTION_OK:${validated_count}")
