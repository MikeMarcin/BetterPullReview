cmake_minimum_required(VERSION 4.4.2)

project(BetterPullReviewDependencyBootstrap LANGUAGES C CXX)

if(NOT DEFINED BPR_CPS_OVERLAY OR BPR_CPS_OVERLAY STREQUAL "")
    message(FATAL_ERROR "BPR_CPS_OVERLAY is required")
endif()

set(BPR_CPS_GENERATOR_REVISION "2")
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

function(bpr_validate_usage_property_coverage target)
    foreach(property IN ITEMS
        INTERFACE_AUTOUIC_OPTIONS
        INTERFACE_LINK_DEPENDS
        INTERFACE_LINK_LIBRARIES_DIRECT
        INTERFACE_LINK_LIBRARIES_DIRECT_EXCLUDE
        INTERFACE_POSITION_INDEPENDENT_CODE
        INTERFACE_PRECOMPILE_HEADERS
    )
        bpr_target_property_list(values "${target}" "${property}")
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

function(bpr_target_location output_location output_link_location target configuration)
    string(TOUPPER "${configuration}" config_upper)
    bpr_target_property_list(imported_configs "${target}" IMPORTED_CONFIGURATIONS)
    get_property(
        map_is_set
        TARGET "${target}"
        PROPERTY "MAP_IMPORTED_CONFIG_${config_upper}"
        SET
    )
    if(map_is_set)
        bpr_target_property_list(
            candidates
            "${target}"
            "MAP_IMPORTED_CONFIG_${config_upper}"
        )
    else()
        set(candidates "${config_upper}")
        list(APPEND candidates ${imported_configs})
    endif()
    list(REMOVE_DUPLICATES candidates)

    set(has_import_library FALSE)
    bpr_target_property_list(general_implib "${target}" IMPORTED_IMPLIB)
    if(NOT "${general_implib}" STREQUAL "")
        set(has_import_library TRUE)
    endif()
    set(artifact_configurations ${candidates} ${imported_configs})
    list(REMOVE_DUPLICATES artifact_configurations)
    foreach(candidate IN LISTS artifact_configurations)
        string(TOUPPER "${candidate}" candidate_upper)
        bpr_target_property_list(
            candidate_implib
            "${target}"
            "IMPORTED_IMPLIB_${candidate_upper}"
        )
        if(NOT "${candidate_implib}" STREQUAL "")
            set(has_import_library TRUE)
        endif()
    endforeach()

    set(location "")
    set(link_location "")
    foreach(candidate IN LISTS candidates)
        string(TOUPPER "${candidate}" candidate_upper)
        bpr_target_property_list(
            candidate_location
            "${target}"
            "IMPORTED_LOCATION_${candidate_upper}"
        )
        bpr_target_property_list(
            candidate_implib
            "${target}"
            "IMPORTED_IMPLIB_${candidate_upper}"
        )
        if(has_import_library)
            set(has_candidate_location FALSE)
            set(has_candidate_implib FALSE)
            if(NOT "${candidate_location}" STREQUAL "")
                set(has_candidate_location TRUE)
            endif()
            if(NOT "${candidate_implib}" STREQUAL "")
                set(has_candidate_implib TRUE)
            endif()
            if(
                (has_candidate_location AND NOT has_candidate_implib)
                OR (has_candidate_implib AND NOT has_candidate_location)
            )
                message(
                    FATAL_ERROR
                    "Incomplete paired imported artifact for ${target} ${candidate_upper}"
                )
            endif()
            if(NOT "${candidate_location}" STREQUAL "")
                set(location "${candidate_location}")
                set(link_location "${candidate_implib}")
                break()
            endif()
        elseif(NOT "${candidate_location}" STREQUAL "")
            set(location "${candidate_location}")
            break()
        endif()
    endforeach()
    if(location STREQUAL "")
        bpr_target_property_list(general_location "${target}" IMPORTED_LOCATION)
        if(has_import_library)
            set(has_general_location FALSE)
            set(has_general_implib FALSE)
            if(NOT "${general_location}" STREQUAL "")
                set(has_general_location TRUE)
            endif()
            if(NOT "${general_implib}" STREQUAL "")
                set(has_general_implib TRUE)
            endif()
            if(
                (has_general_location AND NOT has_general_implib)
                OR (has_general_implib AND NOT has_general_location)
            )
                message(FATAL_ERROR "Incomplete paired imported artifact for ${target}")
            endif()
            set(location "${general_location}")
            set(link_location "${general_implib}")
        else()
            set(location "${general_location}")
        endif()
    endif()
    set(${output_location} "${location}" PARENT_SCOPE)
    set(${output_link_location} "${link_location}" PARENT_SCOPE)
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
                if(NOT configuration STREQUAL "Debug")
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
            bpr_target_location(location link_location "${target}" "${configuration}")
            if(link_location)
                bpr_append_metadata("${key}" LINK_LIBRARIES "${link_location}")
                set_property(GLOBAL APPEND PROPERTY BPR_CPS_ARTIFACTS "${link_location}")
            elseif(location)
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
    bpr_target_location(location link_location "${target}" "${configuration}")
    get_target_property(root_type "${target}" TYPE)
    if(
        NOT root_type STREQUAL "INTERFACE_LIBRARY"
        AND NOT location
        AND NOT link_location
    )
        message(FATAL_ERROR "CPS component '${target}' has no artifact for ${configuration}")
    endif()

    set(result "{")
    set(separator "")
    foreach(pair IN ITEMS "location;${location}" "link_location;${link_location}")
        list(GET pair 0 field)
        list(GET pair 1 value)
        if(value)
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
        if(values)
            list(REMOVE_DUPLICATES values)
            bpr_json_array(array ${values})
            string(APPEND result "${separator}\"${field}\":${array}")
            set(separator ",")
        endif()
    endforeach()

    get_property(definitions GLOBAL PROPERTY "BPR_CPS_${key}_DEFINITIONS")
    if(definitions)
        list(REMOVE_DUPLICATES definitions)
        bpr_json_definitions(definitions_json ${definitions})
        string(APPEND result "${separator}\"definitions\":${definitions_json}")
    endif()
    string(APPEND result "}")
    set(${output} "${result}" PARENT_SCOPE)
endfunction()

function(bpr_library_component output component target)
    bpr_cps_type(type "${target}")
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
