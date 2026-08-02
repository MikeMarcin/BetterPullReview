cmake_minimum_required(VERSION 4.4.2)

if(
    NOT DEFINED BPR_CPS_DOCUMENT
    OR NOT DEFINED BPR_GENERATED_CPS_DOCUMENT
    OR NOT DEFINED BPR_POLICY_ORACLE_DIRECTORY
)
    message(FATAL_ERROR "CPS producer-selection validation inputs are incomplete")
endif()

file(GLOB_RECURSE policy_oracles "${BPR_POLICY_ORACLE_DIRECTORY}/*.txt")
if(NOT policy_oracles)
    message(STATUS "CPS_PRODUCER_SELECTION_OK:0")
    return()
endif()
if(NOT EXISTS "${BPR_CPS_DOCUMENT}")
    message(FATAL_ERROR "CPS producer-selection document is missing")
endif()
if(NOT EXISTS "${BPR_GENERATED_CPS_DOCUMENT}")
    message(FATAL_ERROR "Generated CPS producer metadata is missing")
endif()
file(SHA256 "${BPR_CPS_DOCUMENT}" reference_document_sha256)
file(SHA256 "${BPR_GENERATED_CPS_DOCUMENT}" generated_document_sha256)
if(NOT generated_document_sha256 STREQUAL reference_document_sha256)
    message(
        FATAL_ERROR
        "CPS producer metadata changes across native consumer configurations"
    )
endif()
file(READ "${BPR_CPS_DOCUMENT}" cps_document)
set(validated_count 0)
foreach(field IN ITEMS location link_location)
    file(GLOB field_oracles "${BPR_POLICY_ORACLE_DIRECTORY}/${field}/*.txt")
    foreach(policy_oracle IN LISTS field_oracles)
        get_filename_component(oracle_name "${policy_oracle}" NAME_WE)
        if(NOT oracle_name MATCHES "^([A-Za-z0-9_]+)-(.+)$")
            message(FATAL_ERROR "Malformed CPS producer-selection oracle: '${policy_oracle}'")
        endif()
        set(component "${CMAKE_MATCH_1}")
        string(TOLOWER "${CMAKE_MATCH_2}" configuration)
        file(READ "${policy_oracle}" producer_artifact)
        string(STRIP "${producer_artifact}" producer_artifact)
        string(
            JSON cps_artifact
            ERROR_VARIABLE artifact_error
            GET
                "${cps_document}"
                components "${component}" configurations "${configuration}"
                "${field}"
        )
        if(artifact_error)
            message(
                FATAL_ERROR
                "CPS producer-selection oracle has no matching document artifact: "
                "${component}/${configuration}/${field}"
            )
        endif()
        if(
            producer_artifact STREQUAL ""
            OR producer_artifact MATCHES "-NOTFOUND$"
            OR NOT EXISTS "${producer_artifact}"
        )
            message(
                FATAL_ERROR
                "Producer target has no valid native artifact for "
                "${component}/${configuration}/${field}: '${producer_artifact}'"
            )
        endif()
        file(REAL_PATH "${producer_artifact}" producer_artifact_real)
        file(REAL_PATH "${cps_artifact}" cps_artifact_real)
        if(NOT producer_artifact_real STREQUAL cps_artifact_real)
            message(
                FATAL_ERROR
                "CPS producer selection differs from native CMake for "
                "${component}/${configuration}/${field}: "
                "producer='${producer_artifact}', cps='${cps_artifact}'"
            )
        endif()
        math(EXPR validated_count "${validated_count} + 1")
    endforeach()
endforeach()

file(GLOB closure_oracles "${BPR_POLICY_ORACLE_DIRECTORY}/closure/*.txt")
foreach(policy_oracle IN LISTS closure_oracles)
    file(STRINGS "${policy_oracle}" oracle_fields)
    list(LENGTH oracle_fields oracle_field_count)
    if(NOT oracle_field_count EQUAL 5)
        message(FATAL_ERROR "Malformed transitive CPS selection oracle: '${policy_oracle}'")
    endif()
    list(GET oracle_fields 0 component)
    list(GET oracle_fields 1 configuration)
    list(GET oracle_fields 2 producer_target)
    list(GET oracle_fields 3 cps_artifact)
    list(GET oracle_fields 4 producer_artifact)
    string(TOLOWER "${configuration}" configuration)
    if(
        producer_artifact STREQUAL ""
        OR producer_artifact MATCHES "-NOTFOUND$"
        OR NOT EXISTS "${producer_artifact}"
    )
        message(
            FATAL_ERROR
            "Transitive producer target has no valid native artifact for "
            "${component}/${configuration}/${producer_target}: '${producer_artifact}'"
        )
    endif()
    file(REAL_PATH "${producer_artifact}" producer_artifact_real)
    file(REAL_PATH "${cps_artifact}" cps_artifact_real)
    if(NOT producer_artifact_real STREQUAL cps_artifact_real)
        message(
            FATAL_ERROR
            "Transitive CPS producer selection differs from native CMake for "
            "${component}/${configuration}/${producer_target}: "
            "producer='${producer_artifact}', cps='${cps_artifact}'"
        )
    endif()

    string(
        JSON link_library_count
        ERROR_VARIABLE link_library_error
        LENGTH
            "${cps_document}"
            components "${component}" configurations "${configuration}"
            link_libraries
    )
    if(link_library_error OR link_library_count EQUAL 0)
        message(
            FATAL_ERROR
            "CPS document has no transitive link artifact for "
            "${component}/${configuration}/${producer_target}"
        )
    endif()
    set(cps_artifact_found FALSE)
    math(EXPR link_library_last "${link_library_count} - 1")
    foreach(link_library_index RANGE 0 ${link_library_last})
        string(
            JSON link_library
            GET
                "${cps_document}"
                components "${component}" configurations "${configuration}"
                link_libraries ${link_library_index}
        )
        if(link_library STREQUAL cps_artifact)
            set(cps_artifact_found TRUE)
            break()
        endif()
    endforeach()
    if(NOT cps_artifact_found)
        message(
            FATAL_ERROR
            "Validated transitive producer artifact is absent from CPS: "
            "${component}/${configuration}/${producer_target}"
        )
    endif()
    math(EXPR validated_count "${validated_count} + 1")
endforeach()

message(STATUS "CPS_PRODUCER_SELECTION_OK:${validated_count}")
