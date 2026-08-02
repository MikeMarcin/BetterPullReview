include_guard(GLOBAL)

set(BPR_QT_MINIMUM_VERSION "6.8")
set(BPR_LIBSECRET_MINIMUM_VERSION "0.20")
set(BPR_PYTHON_MINIMUM_VERSION "3.10")
set(BPR_GIT_MINIMUM_VERSION "2.39")
set(BPR_CPS_GENERATOR_REVISION "1")

function(bpr_assert_empty_redirects)
    file(GLOB redirect_entries LIST_DIRECTORIES TRUE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/*")
    if(redirect_entries)
        list(JOIN redirect_entries "\n  " redirect_listing)
        message(
            FATAL_ERROR
            "Dependency redirection is forbidden in the product graph. Remove:\n  ${redirect_listing}"
        )
    endif()
endfunction()

function(bpr_validate_overlay_artifacts overlay)
    set(manifest "${overlay}/artifacts.cmake")
    set(fingerprint "${overlay}/fingerprint.txt")
    if(NOT EXISTS "${manifest}" OR NOT EXISTS "${fingerprint}")
        message(FATAL_ERROR "CPS overlay is incomplete or stale: '${overlay}'")
    endif()
    file(GLOB_RECURSE overlay_cmake_files LIST_DIRECTORIES FALSE "${overlay}/*.cmake")
    list(REMOVE_ITEM overlay_cmake_files "${manifest}")
    if(overlay_cmake_files)
        list(JOIN overlay_cmake_files "\n  " overlay_cmake_listing)
        message(
            FATAL_ERROR
            "CMake-script package metadata is forbidden in the CPS overlay:\n  "
            "${overlay_cmake_listing}"
        )
    endif()
    unset(BPR_CPS_ARTIFACT_RECORDS)
    unset(BPR_CPS_DOCUMENT_RECORDS)
    unset(BPR_CPS_MANIFEST_GENERATOR_REVISION)
    include("${manifest}")
    if(NOT BPR_CPS_MANIFEST_GENERATOR_REVISION STREQUAL BPR_CPS_GENERATOR_REVISION)
        message(FATAL_ERROR "CPS overlay generator revision is stale")
    endif()
    foreach(record_kind IN ITEMS ARTIFACT DOCUMENT)
        foreach(record IN LISTS BPR_CPS_${record_kind}_RECORDS)
            if(NOT record MATCHES "^(.*)\\|([0-9a-f]+)$")
                message(FATAL_ERROR "Malformed CPS ${record_kind} record: '${record}'")
            endif()
            set(path "${CMAKE_MATCH_1}")
            set(expected "${CMAKE_MATCH_2}")
            string(LENGTH "${expected}" digest_length)
            if(NOT digest_length EQUAL 64)
                message(FATAL_ERROR "Malformed CPS ${record_kind} digest for '${path}'")
            endif()
            if(NOT EXISTS "${path}")
                message(FATAL_ERROR "CPS overlay ${record_kind} is missing: '${path}'")
            endif()
            file(SHA256 "${path}" actual)
            if(NOT actual STREQUAL expected)
                message(FATAL_ERROR "CPS overlay ${record_kind} changed since generation: '${path}'")
            endif()
        endforeach()
    endforeach()
    file(READ "${manifest}" manifest_content)
    string(SHA256 expected_fingerprint "${manifest_content}")
    file(READ "${fingerprint}" actual_fingerprint)
    string(STRIP "${actual_fingerprint}" actual_fingerprint)
    if(NOT actual_fingerprint STREQUAL expected_fingerprint)
        message(FATAL_ERROR "CPS overlay fingerprint does not match its manifest")
    endif()
endfunction()

function(bpr_bootstrap_cps overlay)
    set(bootstrap_source "${PROJECT_SOURCE_DIR}/cmake/dependency-bootstrap")
    set(bootstrap_binary "${CMAKE_BINARY_DIR}/_bpr/dependency-bootstrap")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND BPR_BUILD_QT)
        set(bootstrap_libsecret ON)
    else()
        set(bootstrap_libsecret OFF)
    endif()
    if(BUILD_TESTING AND BPR_BUILD_QT)
        set(bootstrap_git ON)
    else()
        set(bootstrap_git OFF)
    endif()

    set(command
        "${CMAKE_COMMAND}"
        --fresh
        -S "${bootstrap_source}"
        -B "${bootstrap_binary}"
        -G "${CMAKE_GENERATOR}"
        "-DBPR_CPS_OVERLAY:PATH=${overlay}"
        "-DBPR_BOOTSTRAP_QT:BOOL=${BPR_BUILD_QT}"
        "-DBPR_BOOTSTRAP_LIBSECRET:BOOL=${bootstrap_libsecret}"
        "-DBPR_BOOTSTRAP_TEST_TOOLS:BOOL=${BUILD_TESTING}"
        "-DBPR_BOOTSTRAP_GIT:BOOL=${bootstrap_git}"
        "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    )
    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND command -A "${CMAKE_GENERATOR_PLATFORM}")
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND command -T "${CMAKE_GENERATOR_TOOLSET}")
    endif()
    foreach(variable IN ITEMS
        Qt6_ROOT
        Python3_ROOT_DIR
        Git_ROOT
        CMAKE_PREFIX_PATH
        CMAKE_TOOLCHAIN_FILE
        CMAKE_OSX_ARCHITECTURES
        CMAKE_OSX_DEPLOYMENT_TARGET
        CMAKE_OSX_SYSROOT
    )
        if(DEFINED ${variable})
            if(NOT "${${variable}}" STREQUAL "")
                string(REPLACE ";" "\\;" escaped_value "${${variable}}")
                list(APPEND command "-D${variable}:STRING=${escaped_value}")
            endif()
        endif()
    endforeach()
    if(BPR_PRODUCER_QT6_DIR)
        list(APPEND command "-DQt6_DIR:PATH=${BPR_PRODUCER_QT6_DIR}")
    endif()

    execute_process(
        COMMAND ${command}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        COMMAND_ECHO STDOUT
    )
    if(NOT result EQUAL 0)
        message(
            FATAL_ERROR
            "CPS dependency bootstrap failed (${result}).\n${output}${error}"
        )
    endif()
endfunction()

function(bpr_find_cps_package package version)
    set(options)
    set(one_value TARGET)
    set(multi_value COMPONENTS)
    cmake_parse_arguments(PARSE_ARGV 2 arg "${options}" "${one_value}" "${multi_value}")

    set(expected_config "${BPR_CPS_OVERLAY}/cps/${package}/${package}.cps")
    if(NOT EXISTS "${expected_config}")
        message(FATAL_ERROR "Required CPS package is missing: '${expected_config}'")
    endif()
    unset(${package}_DIR CACHE)
    unset(${package}_DIR)
    unset(${package}_CONFIG CACHE)
    unset(${package}_CONFIG)
    find_package(
        ${package}
        ${version}
        CONFIG
        REQUIRED
        COMPONENTS ${arg_COMPONENTS}
        PATHS "${BPR_CPS_OVERLAY}"
        NO_DEFAULT_PATH
    )
    if(NOT DEFINED ${package}_CONFIG OR NOT "${${package}_CONFIG}" MATCHES "\\.cps$")
        message(FATAL_ERROR "${package} did not resolve through CPS: '${${package}_CONFIG}'")
    endif()
    file(REAL_PATH "${${package}_CONFIG}" config_real)
    file(REAL_PATH "${BPR_CPS_OVERLAY}" overlay_real)
    string(FIND "${config_real}" "${overlay_real}/" prefix_index)
    if(NOT prefix_index EQUAL 0)
        message(FATAL_ERROR "${package} CPS config escaped the generated overlay: '${config_real}'")
    endif()
    if(arg_TARGET AND NOT TARGET "${arg_TARGET}")
        message(FATAL_ERROR "${package} CPS config did not define '${arg_TARGET}'")
    endif()
    set_property(
        GLOBAL
        APPEND
        PROPERTY BPR_CPS_RESOLUTION_RECORDS
        "${package}|${config_real}|${arg_TARGET}"
    )
    bpr_assert_empty_redirects()
endfunction()

function(bpr_materialize_cps_executable metadata_target executable_target)
    if(NOT TARGET "${metadata_target}")
        message(FATAL_ERROR "Missing CPS executable metadata target '${metadata_target}'")
    endif()
    get_target_property(location "${metadata_target}" INTERFACE_LINK_LIBRARIES)
    if(NOT location)
        message(FATAL_ERROR "CPS executable metadata '${metadata_target}' has no location")
    endif()
    list(LENGTH location location_count)
    if(NOT location_count EQUAL 1 OR NOT EXISTS "${location}")
        message(FATAL_ERROR "CPS executable metadata '${metadata_target}' is invalid: '${location}'")
    endif()
    add_executable("${executable_target}" IMPORTED GLOBAL)
    set_property(TARGET "${executable_target}" PROPERTY IMPORTED_LOCATION "${location}")
endfunction()

function(bpr_configure_dependencies)
    set(BPR_CPS_OVERLAY "${CMAKE_BINARY_DIR}/_bpr/cps-overlay" CACHE INTERNAL "BPR CPS overlay")
    if(Qt6_DIR)
        file(REAL_PATH "${Qt6_DIR}" candidate_qt_dir)
        file(REAL_PATH "${BPR_CPS_OVERLAY}" candidate_overlay)
        string(FIND "${candidate_qt_dir}" "${candidate_overlay}/" overlay_index)
        if(NOT overlay_index EQUAL 0)
            set(
                BPR_PRODUCER_QT6_DIR
                "${Qt6_DIR}"
                CACHE INTERNAL
                "Qt producer package used only by the isolated CPS bootstrap"
                FORCE
            )
        endif()
    endif()
    set(
        CMAKE_FIND_PACKAGE_REDIRECTS_DIR
        "${CMAKE_BINARY_DIR}/_bpr/empty-package-redirects"
        CACHE INTERNAL
        "Empty package redirects directory for strict CPS resolution"
        FORCE
    )
    file(MAKE_DIRECTORY "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}")
    bpr_assert_empty_redirects()

    if(NOT BPR_DEPENDENCY_OFFLINE)
        bpr_bootstrap_cps("${BPR_CPS_OVERLAY}")
    endif()
    bpr_validate_overlay_artifacts("${BPR_CPS_OVERLAY}")

    find_package(Threads REQUIRED)
    if(BUILD_TESTING)
        bpr_find_cps_package(
            Python3
            "${BPR_PYTHON_MINIMUM_VERSION}"
            TARGET Python3::ExecutableMetadata
            COMPONENTS ExecutableMetadata
        )
        bpr_materialize_cps_executable(Python3::ExecutableMetadata Python3::Interpreter)
    endif()
    if(BPR_BUILD_QT)
        bpr_find_cps_package(
            Qt6
            "${BPR_QT_MINIMUM_VERSION}"
            TARGET Qt6::Core
            COMPONENTS Core Gui Widgets Network
        )
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            bpr_find_cps_package(
                LibSecret
                "${BPR_LIBSECRET_MINIMUM_VERSION}"
                TARGET LibSecret::LibSecret
                COMPONENTS LibSecret
            )
        endif()
        if(BUILD_TESTING)
            bpr_find_cps_package(
                Git
                "${BPR_GIT_MINIMUM_VERSION}"
                TARGET Git::ExecutableMetadata
                COMPONENTS ExecutableMetadata
            )
            bpr_materialize_cps_executable(Git::ExecutableMetadata Git::Git)
        endif()
    endif()

    get_property(records GLOBAL PROPERTY BPR_CPS_RESOLUTION_RECORDS)
    set(audit "set(BPR_PRODUCT_CPS_OVERLAY [==[${BPR_CPS_OVERLAY}]==])\n")
    string(APPEND audit "set(BPR_PRODUCT_CPS_RECORDS\n")
    foreach(record IN LISTS records)
        string(APPEND audit "  [==[${record}]==]\n")
    endforeach()
    string(APPEND audit ")\n")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_bpr")
    set(audit_path "${CMAKE_BINARY_DIR}/_bpr/product-dependency-audit.cmake")
    set(audit_temporary "${audit_path}.tmp")
    file(WRITE "${audit_temporary}" "${audit}")
    configure_file("${audit_temporary}" "${audit_path}" COPYONLY)
    file(REMOVE "${audit_temporary}")

    set(BPR_CPS_OVERLAY "${BPR_CPS_OVERLAY}" PARENT_SCOPE)
endfunction()
