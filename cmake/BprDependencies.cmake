include_guard(GLOBAL)

set(BPR_QT_MINIMUM_VERSION "6.8")
set(BPR_LIBSECRET_MINIMUM_VERSION "0.20")
set(BPR_PYTHON_MINIMUM_VERSION "3.10")
set(BPR_GIT_MINIMUM_VERSION "2.39")
set(BPR_CPS_GENERATOR_REVISION "8")

function(bpr_collect_directory_targets output directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(imported_targets DIRECTORY "${directory}" PROPERTY IMPORTED_TARGETS)
    list(APPEND targets ${imported_targets})
    get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(subdirectory IN LISTS subdirectories)
        bpr_collect_directory_targets(nested_targets "${subdirectory}")
        list(APPEND targets ${nested_targets})
    endforeach()
    set(${output} "${targets}" PARENT_SCOPE)
endfunction()

function(bpr_validate_qt_compatibility_nonparticipation)
    bpr_collect_directory_targets(product_targets "${CMAKE_SOURCE_DIR}")
    list(REMOVE_DUPLICATES product_targets)
    foreach(target IN LISTS product_targets)
        if(target MATCHES "^Qt6::")
            continue()
        endif()
        foreach(property IN ITEMS QT_MAJOR_VERSION QT_COORD_TYPE)
            foreach(candidate IN ITEMS "${property}" "INTERFACE_${property}")
                get_property(is_set TARGET "${target}" PROPERTY "${candidate}" SET)
                if(is_set)
                    get_target_property(value "${target}" "${candidate}")
                    message(
                        FATAL_ERROR
                        "Qt's CMake-only ${property} compatibility constraint cannot "
                        "be represented by CPS, but product target '${target}' sets "
                        "${candidate}='${value}'"
                    )
                endif()
            endforeach()
        endforeach()
    endforeach()
endfunction()

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
    set(manifest "${overlay}/artifacts.json")
    set(fingerprint "${overlay}/fingerprint.txt")
    set(validation_seal "${overlay}/validated.txt")
    if(
        NOT EXISTS "${manifest}"
        OR NOT EXISTS "${fingerprint}"
        OR NOT EXISTS "${validation_seal}"
    )
        message(FATAL_ERROR "CPS overlay is incomplete or stale: '${overlay}'")
    endif()
    file(GLOB_RECURSE overlay_cmake_files LIST_DIRECTORIES FALSE "${overlay}/*.cmake")
    if(overlay_cmake_files)
        list(JOIN overlay_cmake_files "\n  " overlay_cmake_listing)
        message(
            FATAL_ERROR
            "CMake-script package metadata is forbidden in the CPS overlay:\n  "
            "${overlay_cmake_listing}"
        )
    endif()
    file(READ "${manifest}" manifest_content)
    string(SHA256 expected_fingerprint "${manifest_content}")
    file(READ "${fingerprint}" actual_fingerprint)
    string(STRIP "${actual_fingerprint}" actual_fingerprint)
    if(NOT actual_fingerprint STREQUAL expected_fingerprint)
        message(FATAL_ERROR "CPS overlay fingerprint does not match its manifest")
    endif()
    file(READ "${validation_seal}" actual_validation_seal)
    string(STRIP "${actual_validation_seal}" actual_validation_seal)
    set(expected_validation_seal "${BPR_CPS_GENERATOR_REVISION}:${expected_fingerprint}")
    if(NOT actual_validation_seal STREQUAL expected_validation_seal)
        message(FATAL_ERROR "CPS overlay was not validated after generation")
    endif()

    string(
        JSON manifest_revision
        ERROR_VARIABLE manifest_error
        GET "${manifest_content}" generator_revision
    )
    if(manifest_error OR NOT manifest_revision STREQUAL BPR_CPS_GENERATOR_REVISION)
        message(FATAL_ERROR "CPS overlay generator revision is stale or malformed")
    endif()

    file(REAL_PATH "${overlay}" overlay_real)
    set(manifest_artifacts)
    set(manifest_documents)
    foreach(record_kind IN ITEMS artifacts documents)
        string(
            JSON record_count
            ERROR_VARIABLE manifest_error
            LENGTH "${manifest_content}" "${record_kind}"
        )
        if(manifest_error)
            message(FATAL_ERROR "CPS overlay ${record_kind} manifest is malformed")
        endif()
        if(record_count EQUAL 0)
            continue()
        endif()
        math(EXPR last_record "${record_count} - 1")
        foreach(index RANGE 0 ${last_record})
            string(JSON path GET "${manifest_content}" "${record_kind}" ${index} path)
            string(JSON expected GET "${manifest_content}" "${record_kind}" ${index} sha256)
            if(record_kind STREQUAL "documents")
                if(IS_ABSOLUTE "${path}")
                    message(FATAL_ERROR "CPS overlay DOCUMENT path must be relative: '${path}'")
                endif()
                file(REAL_PATH "${overlay}/${path}" resolved_path)
                string(FIND "${resolved_path}" "${overlay_real}/" prefix_index)
                if(NOT prefix_index EQUAL 0)
                    message(FATAL_ERROR "CPS overlay DOCUMENT escaped the overlay: '${path}'")
                endif()
                set(kind "DOCUMENT")
                list(APPEND manifest_documents "${resolved_path}")
            else()
                if(NOT IS_ABSOLUTE "${path}")
                    message(FATAL_ERROR "CPS overlay ARTIFACT path must be absolute: '${path}'")
                endif()
                set(resolved_path "${path}")
                set(kind "ARTIFACT")
            endif()
            string(LENGTH "${expected}" digest_length)
            if(NOT expected MATCHES "^[0-9a-f]+$" OR NOT digest_length EQUAL 64)
                message(FATAL_ERROR "Malformed CPS ${kind} digest for '${path}'")
            endif()
            if(NOT EXISTS "${resolved_path}")
                message(FATAL_ERROR "CPS overlay ${kind} is missing: '${resolved_path}'")
            endif()
            if(kind STREQUAL "ARTIFACT")
                file(REAL_PATH "${resolved_path}" authenticated_artifact)
                list(APPEND manifest_artifacts "${authenticated_artifact}")
            endif()
            file(SHA256 "${resolved_path}" actual)
            if(NOT actual STREQUAL expected)
                message(
                    FATAL_ERROR
                    "CPS overlay ${kind} changed since generation: '${resolved_path}'"
                )
            endif()
        endforeach()
    endforeach()

    file(
        GLOB_RECURSE overlay_documents
        LIST_DIRECTORIES FALSE
        "${overlay}/cps/*.cps"
    )
    set(canonical_overlay_documents)
    foreach(overlay_document IN LISTS overlay_documents)
        file(REAL_PATH "${overlay_document}" canonical_overlay_document)
        list(APPEND canonical_overlay_documents "${canonical_overlay_document}")
    endforeach()
    list(SORT manifest_documents)
    list(SORT canonical_overlay_documents)
    if(NOT "${canonical_overlay_documents}" STREQUAL "${manifest_documents}")
        message(FATAL_ERROR "CPS overlay contains unverified or missing package documents")
    endif()
    set_property(GLOBAL PROPERTY BPR_CPS_VALIDATED_ARTIFACTS "${manifest_artifacts}")
    set_property(GLOBAL PROPERTY BPR_CPS_VALIDATED_DOCUMENTS "${manifest_documents}")
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
        "-DCMAKE_POLICY_DEFAULT_CMP0200:STRING=NEW"
        "-DCMAKE_POLICY_VERSION_MINIMUM:STRING=4.4.2"
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

    set(validation_command
        "${CMAKE_COMMAND}"
        --build "${bootstrap_binary}"
        --target bpr_validate_cps_producer_selection
    )
    if(CMAKE_BUILD_TYPE)
        list(APPEND validation_command --config "${CMAKE_BUILD_TYPE}")
    endif()
    execute_process(
        COMMAND ${validation_command}
        RESULT_VARIABLE validation_result
        OUTPUT_VARIABLE validation_output
        ERROR_VARIABLE validation_error
        COMMAND_ECHO STDOUT
    )
    if(NOT validation_result EQUAL 0)
        message(
            FATAL_ERROR
            "CPS producer-selection validation failed (${validation_result}).\n"
            "${validation_output}${validation_error}"
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
    file(REAL_PATH "${expected_config}" expected_config_real)
    file(REAL_PATH "${BPR_CPS_OVERLAY}" overlay_real)
    string(FIND "${config_real}" "${overlay_real}/" prefix_index)
    if(NOT prefix_index EQUAL 0)
        message(FATAL_ERROR "${package} CPS config escaped the generated overlay: '${config_real}'")
    endif()
    get_property(validated_documents GLOBAL PROPERTY BPR_CPS_VALIDATED_DOCUMENTS)
    if(
        NOT config_real STREQUAL expected_config_real
        OR NOT config_real IN_LIST validated_documents
    )
        message(FATAL_ERROR "${package} resolved an unauthenticated CPS config: '${config_real}'")
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
    file(REAL_PATH "${location}" location_real)
    get_property(validated_artifacts GLOBAL PROPERTY BPR_CPS_VALIDATED_ARTIFACTS)
    if(NOT location_real IN_LIST validated_artifacts)
        message(
            FATAL_ERROR
            "CPS executable metadata '${metadata_target}' references an unauthenticated artifact"
        )
    endif()
    add_executable("${executable_target}" IMPORTED GLOBAL)
    set_property(TARGET "${executable_target}" PROPERTY IMPORTED_LOCATION "${location_real}")
endfunction()

function(bpr_configure_dependencies)
    set(BPR_CPS_OVERLAY "${CMAKE_BINARY_DIR}/_bpr/cps-overlay" CACHE INTERNAL "BPR CPS overlay")
    set(qt6_root_hint "")
    if(DEFINED Qt6_ROOT)
        set(qt6_root_hint "${Qt6_ROOT}")
    endif()
    set(prefix_path_hint "")
    if(DEFINED CMAKE_PREFIX_PATH)
        set(prefix_path_hint "${CMAKE_PREFIX_PATH}")
    endif()
    set(environment_qt6_root_hint "")
    if(DEFINED ENV{Qt6_ROOT})
        set(environment_qt6_root_hint "$ENV{Qt6_ROOT}")
    endif()
    set(environment_prefix_path_hint "")
    if(DEFINED ENV{CMAKE_PREFIX_PATH})
        set(environment_prefix_path_hint "$ENV{CMAKE_PREFIX_PATH}")
    endif()
    set(
        producer_hint_material
        "Qt6_ROOT=${qt6_root_hint}\n"
        "CMAKE_PREFIX_PATH=${prefix_path_hint}\n"
        "ENV_Qt6_ROOT=${environment_qt6_root_hint}\n"
        "ENV_CMAKE_PREFIX_PATH=${environment_prefix_path_hint}\n"
    )
    string(SHA256 producer_hint_fingerprint "${producer_hint_material}")
    set(current_external_qt_dir FALSE)
    if(Qt6_DIR)
        file(REAL_PATH "${Qt6_DIR}" candidate_qt_dir)
        cmake_path(
            ABSOLUTE_PATH BPR_CPS_OVERLAY
            NORMALIZE
            OUTPUT_VARIABLE candidate_overlay
        )
        string(FIND "${candidate_qt_dir}" "${candidate_overlay}/" overlay_index)
        if(NOT overlay_index EQUAL 0)
            set(current_external_qt_dir TRUE)
            set(
                BPR_PRODUCER_QT6_DIR
                "${Qt6_DIR}"
                CACHE INTERNAL
                "Qt producer package used only by the isolated CPS bootstrap"
                FORCE
            )
        endif()
    endif()
    if(current_external_qt_dir)
        set(
            BPR_PRODUCER_HINT_FINGERPRINT
            "${producer_hint_fingerprint}"
            CACHE INTERNAL
            "Producer hints associated with the retained Qt package directory"
            FORCE
        )
    elseif(BPR_PRODUCER_QT6_DIR)
        set(stale_producer_hint FALSE)
        if(NOT DEFINED BPR_PRODUCER_HINT_FINGERPRINT)
            set(stale_producer_hint TRUE)
        elseif(
            NOT "${BPR_PRODUCER_HINT_FINGERPRINT}"
                STREQUAL "${producer_hint_fingerprint}"
        )
            set(stale_producer_hint TRUE)
        endif()
        if(stale_producer_hint)
            unset(BPR_PRODUCER_QT6_DIR CACHE)
            unset(BPR_PRODUCER_HINT_FINGERPRINT CACHE)
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
        cmake_language(DEFER CALL bpr_validate_qt_compatibility_nonparticipation)
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
