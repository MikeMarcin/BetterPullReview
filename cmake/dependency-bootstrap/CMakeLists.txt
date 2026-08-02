cmake_minimum_required(VERSION 4.4.2)

project(BetterPullReviewDependencyBootstrap LANGUAGES C CXX)

if(
    NOT CMAKE_POLICY_DEFAULT_CMP0200 STREQUAL "NEW"
    OR CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS "4.4.2"
)
    message(
        FATAL_ERROR
        "The CPS bootstrap requires externally forced CMake 4.4.2/CMP0200 NEW producer semantics"
    )
endif()

if(NOT DEFINED BPR_CPS_OVERLAY OR BPR_CPS_OVERLAY STREQUAL "")
    message(FATAL_ERROR "BPR_CPS_OVERLAY is required")
endif()

file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/policy-oracles")

set(BPR_CPS_GENERATOR_REVISION "7")
set(BPR_QT_MINIMUM_VERSION "6.8")
set(BPR_LIBSECRET_MINIMUM_VERSION "0.20")
set(BPR_PYTHON_MINIMUM_VERSION "3.10")
set(BPR_GIT_MINIMUM_VERSION "2.39")
set(BPR_CPS_CONFIGURATIONS Debug Release RelWithDebInfo MinSizeRel)

function(bpr_json_quote output value)
    if(value MATCHES ";")
        message(FATAL_ERROR "CPS metadata contains an unsupported semicolon: '${value}'")
    endif()
    string(REPLACE "\\" "\\\\" escaped "${value}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    string(REPLACE "\n" "\\n" escaped "${escaped}")
    string(REPLACE "\r" "\\r" escaped "${escaped}")
    string(REPLACE "\t" "\\t" escaped "${escaped}")
    set(${output} "\"${escaped}\"" PARENT_SCOPE)
endfunction()

function(bpr_json_array output)
    set(result "[")
    set(separator "")
    foreach(value IN LISTS ARGN)
        bpr_json_quote(quoted "${value}")
        string(APPEND result "${separator}${quoted}")
        set(separator ",")
    endforeach()
    string(APPEND result "]")
    set(${output} "${result}" PARENT_SCOPE)
endfunction()

function(bpr_json_definitions output)
    set(result "{\"*\":{")
    set(separator "")
    foreach(definition IN LISTS ARGN)
        string(REGEX REPLACE "^-D" "" definition "${definition}")
        if(definition MATCHES "^([^=]+)=(.*)$")
            set(name "${CMAKE_MATCH_1}")
            set(value "${CMAKE_MATCH_2}")
            bpr_json_quote(json_value "${value}")
        else()
            set(name "${definition}")
            set(json_value "null")
        endif()
        if(name STREQUAL "")
            message(FATAL_ERROR "CPS compile definition has an empty name")
        endif()
        bpr_json_quote(json_name "${name}")
        string(APPEND result "${separator}${json_name}:${json_value}")
        set(separator ",")
    endforeach()
    string(APPEND result "}}")
    set(${output} "${result}" PARENT_SCOPE)
endfunction()

function(bpr_write_if_different path content)
    get_filename_component(directory "${path}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    set(temporary "${path}.tmp")
    file(WRITE "${temporary}" "${content}")
    configure_file("${temporary}" "${path}" COPYONLY)
    file(REMOVE "${temporary}")
endfunction()

function(bpr_target_property_list output target property)
    get_property(property_is_set TARGET "${target}" PROPERTY "${property}" SET)
    if(property_is_set)
        get_target_property(value "${target}" "${property}")
    else()
        set(value "")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(bpr_target_property output output_is_set target property)
    get_property(property_is_set TARGET "${target}" PROPERTY "${property}" SET)
    if(property_is_set)
        get_target_property(value "${target}" "${property}")
    else()
        set(value "")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
    set(${output_is_set} "${property_is_set}" PARENT_SCOPE)
endfunction()

function(
    bpr_target_config_artifacts
    output_location
    output_link_location
    target
    configuration
    general_location
    general_link_location
)
    string(TOUPPER "${configuration}" config_upper)
    bpr_target_property(
        location
        location_is_set
        "${target}"
        "IMPORTED_LOCATION_${config_upper}"
    )
    bpr_target_property(
        link_location
        link_location_is_set
        "${target}"
        "IMPORTED_IMPLIB_${config_upper}"
    )
    if(NOT location_is_set)
        set(location "${general_location}")
    endif()
    if(NOT link_location_is_set)
        set(link_location "${general_link_location}")
    endif()
    set(${output_location} "${location}" PARENT_SCOPE)
    set(${output_link_location} "${link_location}" PARENT_SCOPE)
endfunction()

function(bpr_validate_usage_property_coverage target)
    foreach(property IN ITEMS
        INTERFACE_AUTOMOC_MACRO_NAMES
        INTERFACE_AUTOUIC_OPTIONS
        INTERFACE_CXX_MODULE_SETS
        INTERFACE_HEADER_SETS
        INTERFACE_HEADER_SETS_TO_VERIFY
        INTERFACE_LINK_DEPENDS
        INTERFACE_LINK_LIBRARIES_DIRECT
        INTERFACE_LINK_LIBRARIES_DIRECT_EXCLUDE
        INTERFACE_POSITION_INDEPENDENT_CODE
        INTERFACE_PRECOMPILE_HEADERS
        INTERFACE_SOURCE_SETS
        TRANSITIVE_COMPILE_PROPERTIES
        TRANSITIVE_LINK_PROPERTIES
    )
        bpr_target_property_list(values "${target}" "${property}")
        if(
            property STREQUAL "TRANSITIVE_COMPILE_PROPERTIES"
            AND target MATCHES "^Qt6::"
            AND "${values}" STREQUAL "QT_PLUGIN_TARGETS"
        )
            get_target_property(qt_target_type "${target}" TYPE)
            if(qt_target_type STREQUAL "SHARED_LIBRARY")
                # Qt's CMake-only static-plugin finalizer queries this custom
                # property. The CPS graph uses shared Qt targets and never
                # loads or calls that finalizer, so it is not a usage
                # requirement of the selected product graph.
                continue()
            endif()
        endif()
        if(NOT "${values}" STREQUAL "")
            message(
                FATAL_ERROR
                "Unsupported CPS usage requirement in ${target} ${property}: '${values}'"
            )
        endif()
    endforeach()

    bpr_target_property_list(interface_sources "${target}" INTERFACE_SOURCES)
    foreach(source IN LISTS interface_sources)
        if(
            target MATCHES "^Qt6::"
            AND source MATCHES
                "^\\$<\\$<BOOL:\\$<TARGET_PROPERTY:QT_CONSUMES_METATYPES>>:.*qt6.*_metatypes\\.json>$"
        )
            continue()
        endif()
        message(
            FATAL_ERROR
            "Unsupported CPS usage requirement in ${target} INTERFACE_SOURCES: '${source}'"
        )
    endforeach()

    foreach(property IN ITEMS
        COMPATIBLE_INTERFACE_BOOL
        COMPATIBLE_INTERFACE_NUMBER_MAX
        COMPATIBLE_INTERFACE_NUMBER_MIN
    )
        bpr_target_property_list(values "${target}" "${property}")
        if(NOT "${values}" STREQUAL "")
            message(
                FATAL_ERROR
                "Unsupported CPS compatibility requirement in ${target} ${property}: '${values}'"
            )
        endif()
    endforeach()
    bpr_target_property_list(
        compatible_strings
        "${target}"
        COMPATIBLE_INTERFACE_STRING
    )
    foreach(compatible_string IN LISTS compatible_strings)
        get_target_property(interface_value "${target}" "INTERFACE_${compatible_string}")
        if(
            target MATCHES "^Qt6::"
            AND compatible_string STREQUAL "QT_MAJOR_VERSION"
            AND interface_value STREQUAL "6"
        )
            continue()
        endif()
        if(
            target STREQUAL "Qt6::Core"
            AND compatible_string STREQUAL "QT_COORD_TYPE"
            AND NOT interface_value MATCHES "-NOTFOUND$"
        )
            continue()
        endif()
        message(
            FATAL_ERROR
            "Unsupported CPS compatibility requirement in ${target} "
            "${compatible_string}: '${interface_value}'"
        )
    endforeach()
endfunction()

function(
    bpr_target_location
    output_location
    output_link_location
    output_configuration
    target
    configuration
)
    string(TOUPPER "${configuration}" config_upper)
    bpr_target_property(
        imported_configs
        imported_configs_is_set
        "${target}"
        IMPORTED_CONFIGURATIONS
    )
    get_property(
        map_is_set
        TARGET "${target}"
        PROPERTY "MAP_IMPORTED_CONFIG_${config_upper}"
        SET
    )
    bpr_target_property_list(general_location "${target}" IMPORTED_LOCATION)
    bpr_target_property_list(general_implib "${target}" IMPORTED_IMPLIB)
    set(general_artifact_available FALSE)
    if(
        NOT "${general_location}" STREQUAL ""
        OR NOT "${general_implib}" STREQUAL ""
    )
        set(general_artifact_available TRUE)
    endif()

    # Mirror CMP0200 NEW: select exactly one authoritative producer
    # configuration, then resolve each artifact property independently.
    set(selection_found FALSE)
    set(selected_configless FALSE)
    set(selected_config "")
    if(map_is_set)
        bpr_target_property_list(
            mapped_configs
            "${target}"
            "MAP_IMPORTED_CONFIG_${config_upper}"
        )
        list(LENGTH mapped_configs mapped_config_count)
        if(mapped_config_count EQUAL 0)
            if(NOT imported_configs_is_set AND general_artifact_available)
                set(selection_found TRUE)
                set(selected_configless TRUE)
            endif()
        else()
            math(EXPR mapped_config_last "${mapped_config_count} - 1")
            foreach(mapped_config_index RANGE 0 ${mapped_config_last})
                list(GET mapped_configs ${mapped_config_index} mapped_config)
                if(mapped_config STREQUAL "")
                    if(NOT imported_configs_is_set AND general_artifact_available)
                        set(selection_found TRUE)
                        set(selected_configless TRUE)
                    endif()
                elseif(imported_configs_is_set)
                    string(TOUPPER "${mapped_config}" mapped_config_upper)
                    foreach(imported_config IN LISTS imported_configs)
                        string(TOUPPER "${imported_config}" imported_config_upper)
                        if(mapped_config_upper STREQUAL imported_config_upper)
                            set(selection_found TRUE)
                            set(selected_config "${imported_config}")
                            break()
                        endif()
                    endforeach()
                else()
                    bpr_target_config_artifacts(
                        mapped_location
                        mapped_implib
                        "${target}"
                        "${mapped_config}"
                        "${general_location}"
                        "${general_implib}"
                    )
                    if(
                        NOT "${mapped_location}" STREQUAL ""
                        OR NOT "${mapped_implib}" STREQUAL ""
                    )
                        set(selection_found TRUE)
                        set(selected_config "${mapped_config}")
                    endif()
                endif()
                if(selection_found)
                    break()
                endif()
            endforeach()
        endif()
    elseif(imported_configs_is_set AND NOT "${imported_configs}" STREQUAL "")
        foreach(imported_config IN LISTS imported_configs)
            string(TOUPPER "${imported_config}" imported_config_upper)
            if(config_upper STREQUAL imported_config_upper)
                set(selection_found TRUE)
                set(selected_config "${imported_config}")
                break()
            endif()
        endforeach()
        if(NOT selection_found)
            list(GET imported_configs 0 selected_config)
            set(selection_found TRUE)
        endif()
    else()
        bpr_target_config_artifacts(
            requested_location
            requested_implib
            "${target}"
            "${configuration}"
            "${general_location}"
            "${general_implib}"
        )
        if(
            NOT "${requested_location}" STREQUAL ""
            OR NOT "${requested_implib}" STREQUAL ""
        )
            set(selection_found TRUE)
            set(selected_config "${configuration}")
        endif()
    endif()

    set(location "")
    set(link_location "")
    if(selection_found AND NOT selected_configless)
        bpr_target_config_artifacts(
            location
            link_location
            "${target}"
            "${selected_config}"
            "${general_location}"
            "${general_implib}"
        )
    elseif(selection_found)
        set(location "${general_location}")
        set(link_location "${general_implib}")
    endif()

    get_target_property(target_type "${target}" TYPE)
    set(requires_windows_linker_artifact FALSE)
    if(
        target_type STREQUAL "SHARED_LIBRARY"
        AND (
            CMAKE_SYSTEM_NAME STREQUAL "Windows"
            OR location MATCHES "\\.[dD][lL][lL]$"
        )
    )
        set(requires_windows_linker_artifact TRUE)
    endif()
    if(
        requires_windows_linker_artifact
        AND NOT "${location}" STREQUAL ""
        AND "${link_location}" STREQUAL ""
    )
        message(FATAL_ERROR "Incomplete Windows imported artifact for ${target}")
    endif()
    set(${output_location} "${location}" PARENT_SCOPE)
    set(${output_link_location} "${link_location}" PARENT_SCOPE)
    set(${output_configuration} "${selected_config}" PARENT_SCOPE)
endfunction()

function(bpr_append_metadata key property)
    foreach(value IN LISTS ARGN)
        if(NOT value STREQUAL "")
            set_property(GLOBAL APPEND PROPERTY "BPR_CPS_${key}_${property}" "${value}")
        endif()
    endforeach()
endfunction()

function(bpr_collect_plain_property key target property output_property)
    bpr_target_property_list(values "${target}" "${property}")
    foreach(value IN LISTS values)
        if(value MATCHES "\\$<")
            if(
                property STREQUAL "INTERFACE_COMPILE_DEFINITIONS"
                AND value STREQUAL "$<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG>"
            )
                get_property(configuration GLOBAL PROPERTY "BPR_CPS_${key}_CONFIGURATION")
                bpr_target_location(
                    ignored_location
                    ignored_link_location
                    imported_configuration
                    "${target}"
                    "${configuration}"
                )
                string(TOUPPER "${configuration}" requested_config_upper)
                string(TOUPPER "${imported_configuration}" imported_config_upper)
                if(
                    NOT requested_config_upper STREQUAL "DEBUG"
                    AND NOT imported_config_upper STREQUAL "DEBUG"
                )
                    bpr_append_metadata("${key}" "${output_property}" QT_NO_DEBUG)
                endif()
            else()
                message(
                    FATAL_ERROR
                    "Unsupported generator expression in ${target} ${property}: '${value}'"
                )
            endif()
        else()
            if(
                property MATCHES "^INTERFACE_(SYSTEM_)?INCLUDE_DIRECTORIES$"
                OR property STREQUAL "INTERFACE_LINK_DIRECTORIES"
            )
                if(NOT IS_ABSOLUTE "${value}" OR NOT EXISTS "${value}")
                    message(
                        FATAL_ERROR
                        "Incomplete CPS path metadata in ${target} ${property}: '${value}'"
                    )
                endif()
            endif()
            bpr_append_metadata("${key}" "${output_property}" "${value}")
        endif()
    endforeach()
endfunction()

function(bpr_collect_target key target root)
    string(SHA256 target_hash "${target}")
    get_property(visited GLOBAL PROPERTY "BPR_CPS_${key}_VISITED_${target_hash}")
    if(visited)
        return()
    endif()
    set_property(GLOBAL PROPERTY "BPR_CPS_${key}_VISITED_${target_hash}" TRUE)

    if(NOT TARGET "${target}")
        message(FATAL_ERROR "CPS metadata references missing target '${target}'")
    endif()
    bpr_validate_usage_property_coverage("${target}")

    if(target STREQUAL "Threads::Threads")
        if(THREADS_HAVE_PTHREAD_ARG)
            bpr_append_metadata("${key}" COMPILE_FLAGS -pthread)
        endif()
        if(CMAKE_THREAD_LIBS_INIT)
            bpr_append_metadata("${key}" LINK_LIBRARIES "${CMAKE_THREAD_LIBS_INIT}")
        endif()
        return()
    endif()

    if(
        target MATCHES "^Qt6::(Core|Gui|Widgets|Network)$"
        AND NOT target STREQUAL root
    )
        string(REGEX REPLACE "^Qt6::" ":" requirement "${target}")
        bpr_append_metadata("${key}" REQUIRES "${requirement}")
        return()
    endif()

    bpr_collect_plain_property("${key}" "${target}" INTERFACE_INCLUDE_DIRECTORIES INCLUDES)
    bpr_collect_plain_property("${key}" "${target}" INTERFACE_SYSTEM_INCLUDE_DIRECTORIES INCLUDES)
    bpr_collect_plain_property("${key}" "${target}" INTERFACE_COMPILE_DEFINITIONS DEFINITIONS)
    bpr_collect_plain_property("${key}" "${target}" INTERFACE_COMPILE_OPTIONS COMPILE_FLAGS)
    bpr_collect_plain_property("${key}" "${target}" INTERFACE_LINK_DIRECTORIES LINK_DIRECTORIES)
    bpr_collect_plain_property("${key}" "${target}" INTERFACE_LINK_OPTIONS LINK_FLAGS)

    bpr_target_property_list(features "${target}" INTERFACE_COMPILE_FEATURES)
    foreach(feature IN LISTS features)
        if(feature MATCHES "^cxx_std_([0-9]+)$")
            bpr_append_metadata("${key}" COMPILE_FEATURES "c++${CMAKE_MATCH_1}")
        elseif(feature MATCHES "^c_std_([0-9]+)$")
            bpr_append_metadata("${key}" COMPILE_FEATURES "c${CMAKE_MATCH_1}")
        elseif(feature STREQUAL "cxx_decltype")
            bpr_append_metadata("${key}" COMPILE_FEATURES "c++11")
        elseif(feature MATCHES "\\$<")
            message(FATAL_ERROR "Unsupported compile-feature generator expression in ${target}: '${feature}'")
        else()
            message(FATAL_ERROR "Unsupported compile feature in ${target}: '${feature}'")
        endif()
    endforeach()

    if(NOT target STREQUAL root)
        get_target_property(target_type "${target}" TYPE)
        if(NOT target_type STREQUAL "INTERFACE_LIBRARY")
            get_property(configuration GLOBAL PROPERTY "BPR_CPS_${key}_CONFIGURATION")
            bpr_target_location(
                location
                link_location
                imported_configuration
                "${target}"
                "${configuration}"
            )
            if(NOT "${link_location}" STREQUAL "")
                bpr_append_metadata("${key}" LINK_LIBRARIES "${link_location}")
                set_property(GLOBAL APPEND PROPERTY BPR_CPS_ARTIFACTS "${link_location}")
            elseif(NOT "${location}" STREQUAL "")
                bpr_append_metadata("${key}" LINK_LIBRARIES "${location}")
                set_property(GLOBAL APPEND PROPERTY BPR_CPS_ARTIFACTS "${location}")
            else()
                message(FATAL_ERROR "Imported dependency target '${target}' has no usable artifact")
            endif()
        endif()
    endif()

    bpr_target_property_list(link_libraries "${target}" INTERFACE_LINK_LIBRARIES)
    foreach(library IN LISTS link_libraries)
        if(TARGET "${library}")
            bpr_collect_target("${key}" "${library}" "${root}")
        elseif(library MATCHES "^\\$<TARGET_NAME_IF_EXISTS:([^>]+)>$")
            if(TARGET "${CMAKE_MATCH_1}")
                bpr_collect_target("${key}" "${CMAKE_MATCH_1}" "${root}")
            endif()
        elseif(library MATCHES "QT_IS_PLUGIN_GENEX")
            get_target_property(root_type "${root}" TYPE)
            if(NOT root_type STREQUAL "SHARED_LIBRARY")
                message(
                    FATAL_ERROR
                    "Qt plugin metadata is unsupported for non-shared target '${root}'"
                )
            endif()
        elseif(library MATCHES "\\$<")
            message(
                FATAL_ERROR
                "Unsupported generator expression in ${target} INTERFACE_LINK_LIBRARIES: '${library}'"
            )
        elseif(NOT library STREQUAL "")
            if(IS_ABSOLUTE "${library}")
                if(NOT EXISTS "${library}" OR IS_DIRECTORY "${library}")
                    message(
                        FATAL_ERROR
                        "Incomplete CPS link artifact in ${target}: '${library}'"
                    )
                endif()
                set_property(GLOBAL APPEND PROPERTY BPR_CPS_ARTIFACTS "${library}")
            endif()
            bpr_append_metadata("${key}" LINK_LIBRARIES "${library}")
        endif()
    endforeach()
endfunction()

function(bpr_cps_type output target)
    get_target_property(target_type "${target}" TYPE)
    if(target_type STREQUAL "SHARED_LIBRARY")
        set(type dylib)
    elseif(target_type STREQUAL "STATIC_LIBRARY")
        set(type archive)
    elseif(target_type STREQUAL "MODULE_LIBRARY")
        set(type module)
    elseif(target_type STREQUAL "INTERFACE_LIBRARY")
        set(type interface)
    else()
        message(FATAL_ERROR "Unsupported CPS target type '${target_type}' for '${target}'")
    endif()
    set(${output} "${type}" PARENT_SCOPE)
endfunction()

function(bpr_component_configuration output component target configuration)
    string(MAKE_C_IDENTIFIER "${component}_${configuration}" key)
    string(TOUPPER "${key}" key)
    foreach(property IN ITEMS INCLUDES DEFINITIONS COMPILE_FLAGS COMPILE_FEATURES LINK_DIRECTORIES LINK_FLAGS LINK_LIBRARIES REQUIRES)
        set_property(GLOBAL PROPERTY "BPR_CPS_${key}_${property}" "")
    endforeach()
    set_property(GLOBAL PROPERTY "BPR_CPS_${key}_CONFIGURATION" "${configuration}")
    bpr_collect_target("${key}" "${target}" "${target}")
    bpr_target_location(
        location
        link_location
        imported_configuration
        "${target}"
        "${configuration}"
    )
    get_target_property(root_type "${target}" TYPE)
    if(
        NOT root_type STREQUAL "INTERFACE_LIBRARY"
        AND location STREQUAL ""
        AND link_location STREQUAL ""
    )
        message(FATAL_ERROR "CPS component '${target}' has no artifact for ${configuration}")
    endif()

    set(result "{")
    set(separator "")
    foreach(pair IN ITEMS "location;${location}" "link_location;${link_location}")
        list(GET pair 0 field)
        list(GET pair 1 value)
        if(NOT "${value}" STREQUAL "")
            bpr_json_quote(quoted "${value}")
            string(APPEND result "${separator}\"${field}\":${quoted}")
            set(separator ",")
            set_property(GLOBAL APPEND PROPERTY BPR_CPS_ARTIFACTS "${value}")
        endif()
    endforeach()

    foreach(mapping IN ITEMS
        "INCLUDES;includes"
        "COMPILE_FLAGS;compile_flags"
        "COMPILE_FEATURES;compile_features"
        "LINK_DIRECTORIES;link_directories"
        "LINK_FLAGS;link_flags"
        "LINK_LIBRARIES;link_libraries"
        "REQUIRES;requires"
    )
        list(GET mapping 0 property)
        list(GET mapping 1 field)
        get_property(values GLOBAL PROPERTY "BPR_CPS_${key}_${property}")
        if(NOT "${values}" STREQUAL "")
            list(REMOVE_DUPLICATES values)
            bpr_json_array(array ${values})
            string(APPEND result "${separator}\"${field}\":${array}")
            set(separator ",")
        endif()
    endforeach()

    get_property(definitions GLOBAL PROPERTY "BPR_CPS_${key}_DEFINITIONS")
    if(NOT "${definitions}" STREQUAL "")
        list(REMOVE_DUPLICATES definitions)
        bpr_json_definitions(definitions_json ${definitions})
        string(APPEND result "${separator}\"definitions\":${definitions_json}")
    endif()
    string(APPEND result "}")
    set(${output} "${result}" PARENT_SCOPE)
endfunction()

function(bpr_library_component output component target)
    bpr_cps_type(type "${target}")
    if(target MATCHES "^Qt6::" AND NOT type STREQUAL "interface")
        set(can_validate_native_location TRUE)
        set(can_validate_native_link_location FALSE)
        get_target_property(native_target_type "${target}" TYPE)
        if(
            CMAKE_SYSTEM_NAME STREQUAL "Windows"
            AND native_target_type STREQUAL "SHARED_LIBRARY"
        )
            set(can_validate_native_link_location TRUE)
        endif()
        foreach(configuration IN LISTS BPR_CPS_CONFIGURATIONS)
            bpr_target_location(
                oracle_location
                oracle_link_location
                oracle_configuration
                "${target}"
                "${configuration}"
            )
            if("${oracle_location}" STREQUAL "")
                set(can_validate_native_location FALSE)
            endif()
            if("${oracle_link_location}" STREQUAL "")
                set(can_validate_native_link_location FALSE)
            endif()
        endforeach()
        if(can_validate_native_location)
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/policy-oracles/location")
            file(
                GENERATE
                OUTPUT
                    "${CMAKE_BINARY_DIR}/policy-oracles/location/${component}-$<CONFIG>.txt"
                CONTENT "$<TARGET_FILE:${target}>"
            )
        endif()
        if(can_validate_native_link_location)
            file(
                MAKE_DIRECTORY
                "${CMAKE_BINARY_DIR}/policy-oracles/link_location"
            )
            file(
                GENERATE
                OUTPUT
                    "${CMAKE_BINARY_DIR}/policy-oracles/link_location/${component}-$<CONFIG>.txt"
                CONTENT "$<TARGET_LINKER_FILE:${target}>"
            )
        endif()
    endif()
    set(result "{\"type\":\"${type}\",\"configurations\":{")
    set(separator "")
    foreach(configuration IN LISTS BPR_CPS_CONFIGURATIONS)
        bpr_component_configuration(config_json "${component}" "${target}" "${configuration}")
        string(TOLOWER "${configuration}" config_name)
        string(APPEND result "${separator}\"${config_name}\":${config_json}")
        set(separator ",")
    endforeach()
    string(APPEND result "}}")
    set(${output} "${result}" PARENT_SCOPE)
endfunction()

function(bpr_executable_package package version compat_version location)
    if(NOT EXISTS "${location}")
        message(FATAL_ERROR "${package} executable does not exist: '${location}'")
    endif()
    bpr_json_quote(location_json "${location}")
    set(content
        "{\n  \"cps_version\":\"0.14.1\",\n  \"name\":\"${package}\",\n  \"version\":\"${version}\",\n  \"compat_version\":\"${compat_version}\",\n  \"cps_path\":\"@prefix@/cps/${package}\",\n  \"components\":{\"ExecutableMetadata\":{\"type\":\"interface\",\"link_libraries\":[${location_json}]}}\n}\n"
    )
    set(document "${BPR_CPS_OVERLAY}/cps/${package}/${package}.cps")
    bpr_write_if_different("${document}" "${content}")
    set_property(GLOBAL APPEND PROPERTY BPR_CPS_DOCUMENTS "${document}")
    set_property(GLOBAL APPEND PROPERTY BPR_CPS_ARTIFACTS "${location}")
endfunction()

if(BPR_BOOTSTRAP_TEST_TOOLS)
    find_package(Python3 ${BPR_PYTHON_MINIMUM_VERSION} REQUIRED COMPONENTS Interpreter)
    bpr_executable_package(
        Python3
        "${Python3_VERSION}"
        "${BPR_PYTHON_MINIMUM_VERSION}"
        "${Python3_EXECUTABLE}"
    )
endif()

if(BPR_BOOTSTRAP_GIT)
    find_package(Git ${BPR_GIT_MINIMUM_VERSION} REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" --version
        OUTPUT_VARIABLE git_version_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE git_version_result
    )
    if(NOT git_version_result EQUAL 0 OR NOT git_version_output MATCHES "([0-9]+\\.[0-9]+(\\.[0-9]+)?)")
        message(FATAL_ERROR "Unable to determine the selected Git version")
    endif()
    set(git_version "${CMAKE_MATCH_1}")
    bpr_executable_package(
        Git
        "${git_version}"
        "${BPR_GIT_MINIMUM_VERSION}"
        "${GIT_EXECUTABLE}"
    )
endif()

if(BPR_BOOTSTRAP_QT)
    find_package(
        Qt6
        ${BPR_QT_MINIMUM_VERSION}
        CONFIG
        QUIET
        COMPONENTS Core Gui Widgets Network
    )
    if(NOT Qt6_FOUND)
        message(
            FATAL_ERROR
            "BetterPullReview requires caller-installed Qt >= ${BPR_QT_MINIMUM_VERSION} "
            "with Core, Gui, Widgets, and Network. Set Qt6_ROOT, Qt6_DIR, or "
            "CMAKE_PREFIX_PATH for this isolated producer bootstrap. Qt is never fetched."
        )
    endif()
    find_package(Threads REQUIRED)

    set(qt_components "")
    foreach(component IN ITEMS Core Gui Widgets Network)
        if(NOT TARGET "Qt6::${component}")
            message(FATAL_ERROR "Qt producer did not define Qt6::${component}")
        endif()
        bpr_library_component(component_json "${component}" "Qt6::${component}")
        if(qt_components)
            string(APPEND qt_components ",")
        endif()
        string(APPEND qt_components "\"${component}\":${component_json}")
    endforeach()
    set(qt_content
        "{\n  \"cps_version\":\"0.14.1\",\n  \"name\":\"Qt6\",\n  \"version\":\"${Qt6_VERSION}\",\n  \"compat_version\":\"${BPR_QT_MINIMUM_VERSION}\",\n  \"cps_path\":\"@prefix@/cps/Qt6\",\n  \"configurations\":[\"debug\",\"release\",\"relwithdebinfo\",\"minsizerel\"],\n  \"components\":{${qt_components}}\n}\n"
    )
    set(qt_document "${BPR_CPS_OVERLAY}/cps/Qt6/Qt6.cps")
    bpr_write_if_different("${qt_document}" "${qt_content}")
    set_property(GLOBAL APPEND PROPERTY BPR_CPS_DOCUMENTS "${qt_document}")
endif()

if(BPR_BOOTSTRAP_LIBSECRET)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "libsecret bootstrap is Linux-only")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(
        BPR_LIBSECRET
        REQUIRED
        IMPORTED_TARGET
        "libsecret-1>=${BPR_LIBSECRET_MINIMUM_VERSION}"
    )
    if(NOT TARGET PkgConfig::BPR_LIBSECRET)
        message(FATAL_ERROR "pkg-config did not define PkgConfig::BPR_LIBSECRET")
    endif()
    set(BPR_CPS_CONFIGURATIONS NoConfig)
    bpr_library_component(libsecret_component LibSecret PkgConfig::BPR_LIBSECRET)
    set(libsecret_content
        "{\n  \"cps_version\":\"0.14.1\",\n  \"name\":\"LibSecret\",\n  \"version\":\"${BPR_LIBSECRET_VERSION}\",\n  \"compat_version\":\"${BPR_LIBSECRET_MINIMUM_VERSION}\",\n  \"cps_path\":\"@prefix@/cps/LibSecret\",\n  \"components\":{\"LibSecret\":${libsecret_component}}\n}\n"
    )
    bpr_write_if_different(
        "${BPR_CPS_OVERLAY}/cps/LibSecret/LibSecret.cps"
        "${libsecret_content}"
    )
    set_property(
        GLOBAL
        APPEND
        PROPERTY BPR_CPS_DOCUMENTS
        "${BPR_CPS_OVERLAY}/cps/LibSecret/LibSecret.cps"
    )
endif()

get_property(artifacts GLOBAL PROPERTY BPR_CPS_ARTIFACTS)
if(artifacts)
    list(REMOVE_DUPLICATES artifacts)
endif()
set(
    overlay_manifest
    "{\n  \"generator_revision\":\"${BPR_CPS_GENERATOR_REVISION}\",\n  \"artifacts\":["
)
set(separator "")
foreach(artifact IN LISTS artifacts)
    if(NOT EXISTS "${artifact}")
        message(FATAL_ERROR "CPS artifact does not exist: '${artifact}'")
    endif()
    file(SHA256 "${artifact}" artifact_sha256)
    bpr_json_quote(artifact_json "${artifact}")
    string(
        APPEND overlay_manifest
        "${separator}{\"path\":${artifact_json},\"sha256\":\"${artifact_sha256}\"}"
    )
    set(separator ",")
endforeach()
string(APPEND overlay_manifest "],\n  \"documents\":[")
get_property(documents GLOBAL PROPERTY BPR_CPS_DOCUMENTS)
if(documents)
    list(REMOVE_DUPLICATES documents)
endif()
file(
    GLOB_RECURSE existing_documents
    LIST_DIRECTORIES FALSE
    "${BPR_CPS_OVERLAY}/cps/*.cps"
)
foreach(existing_document IN LISTS existing_documents)
    if(NOT existing_document IN_LIST documents)
        file(REMOVE "${existing_document}")
    endif()
endforeach()
set(separator "")
foreach(document IN LISTS documents)
    if(NOT EXISTS "${document}")
        message(FATAL_ERROR "Generated CPS document does not exist: '${document}'")
    endif()
    file(SHA256 "${document}" document_sha256)
    file(RELATIVE_PATH relative_document "${BPR_CPS_OVERLAY}" "${document}")
    if(relative_document MATCHES "^\\.\\./" OR IS_ABSOLUTE "${relative_document}")
        message(FATAL_ERROR "Generated CPS document escaped its overlay: '${document}'")
    endif()
    bpr_json_quote(document_json "${relative_document}")
    string(
        APPEND overlay_manifest
        "${separator}{\"path\":${document_json},\"sha256\":\"${document_sha256}\"}"
    )
    set(separator ",")
endforeach()
string(APPEND overlay_manifest "]\n}\n")
bpr_write_if_different("${BPR_CPS_OVERLAY}/artifacts.json" "${overlay_manifest}")
file(REMOVE "${BPR_CPS_OVERLAY}/artifacts.cmake")
string(SHA256 fingerprint "${overlay_manifest}")
bpr_write_if_different("${BPR_CPS_OVERLAY}/fingerprint.txt" "${fingerprint}\n")

add_custom_target(
    bpr_validate_cps_producer_selection
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_CPS_DOCUMENT=${BPR_CPS_OVERLAY}/cps/Qt6/Qt6.cps"
        "-DBPR_POLICY_ORACLE_DIRECTORY=${CMAKE_BINARY_DIR}/policy-oracles"
        -P "${CMAKE_CURRENT_LIST_DIR}/ValidateProducerSelection.cmake"
    VERBATIM
)
