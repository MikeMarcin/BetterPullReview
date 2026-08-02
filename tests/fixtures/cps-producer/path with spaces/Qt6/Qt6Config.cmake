cmake_policy(PUSH)
cmake_policy(VERSION 3.1.0...4.1)
if(BPR_FAKE_QT_FORCE_CMP0200_OLD)
    cmake_policy(SET CMP0200 OLD)
endif()

if(NOT BPR_FAKE_QT_VERSION)
    set(BPR_FAKE_QT_VERSION "6.8.1")
endif()
if(NOT DEFINED BPR_FAKE_QT_IMPORTED_CONFIGURATIONS)
    set(BPR_FAKE_QT_IMPORTED_CONFIGURATIONS "DEBUG;RELEASE")
endif()
set(Qt6_VERSION "${BPR_FAKE_QT_VERSION}")
get_filename_component(_bpr_fake_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

foreach(component IN ITEMS Core Gui Widgets Network)
    if(BPR_FAKE_QT_MODULE_TARGETS)
        add_library("Qt6::${component}" MODULE IMPORTED GLOBAL)
    else()
        add_library("Qt6::${component}" SHARED IMPORTED GLOBAL)
    endif()
    string(TOLOWER "${component}" component_lower)
    string(TOUPPER "${component}" component_upper)
    if(BPR_FAKE_QT_WINDOWS_ARTIFACTS)
        set(debug_location "${_bpr_fake_root}/artifacts/windows-debug.dll")
        set(release_location "${_bpr_fake_root}/artifacts/windows-release.dll")
        set(debug_link_location "${_bpr_fake_root}/artifacts/windows-debug.lib")
        set(release_link_location "${_bpr_fake_root}/artifacts/windows-release.lib")
        if(BPR_FAKE_QT_WINDOWS_INCOMPLETE_DEBUG)
            set(debug_link_location "")
        endif()
    else()
        set(debug_location "${_bpr_fake_root}/artifacts/${component_lower}-debug.dylib")
        set(release_location "${_bpr_fake_root}/artifacts/${component_lower}-release.dylib")
        set(debug_link_location "")
        set(release_link_location "")
    endif()
    set_target_properties(
        "Qt6::${component}"
        PROPERTIES
            IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
            IMPORTED_LOCATION_DEBUG "${debug_location}"
            IMPORTED_LOCATION_RELEASE "${release_location}"
            IMPORTED_IMPLIB_DEBUG "${debug_link_location}"
            IMPORTED_IMPLIB_RELEASE "${release_link_location}"
            INTERFACE_INCLUDE_DIRECTORIES "${_bpr_fake_root}/include path/${component}"
            INTERFACE_COMPILE_DEFINITIONS "QT_${component_upper}_LIB"
    )
    set_property(
        TARGET "Qt6::${component}"
        PROPERTY IMPORTED_CONFIGURATIONS
            "${BPR_FAKE_QT_IMPORTED_CONFIGURATIONS}"
    )
    if(BPR_FAKE_QT_UNSET_IMPORTED_CONFIGURATIONS)
        set_property(
            TARGET "Qt6::${component}"
            PROPERTY IMPORTED_CONFIGURATIONS
        )
    endif()
    if(BPR_FAKE_QT_GENERAL_ARTIFACTS)
        set_target_properties(
            "Qt6::${component}"
            PROPERTIES
                IMPORTED_LOCATION "${_bpr_fake_root}/artifacts/windows-release.dll"
                IMPORTED_IMPLIB "${_bpr_fake_root}/artifacts/windows-release.lib"
        )
    endif()
    if(BPR_FAKE_QT_WINDOWS_MAP_DEBUG_RELEASE)
        set_property(
            TARGET "Qt6::${component}"
            PROPERTY MAP_IMPORTED_CONFIG_DEBUG RELEASE
        )
    endif()
    if(BPR_FAKE_QT_WINDOWS_MAP_MISSING_WITH_GENERAL)
        set_target_properties(
            "Qt6::${component}"
            PROPERTIES
                IMPORTED_LOCATION "${_bpr_fake_root}/artifacts/windows-release.dll"
                IMPORTED_IMPLIB "${_bpr_fake_root}/artifacts/windows-release.lib"
                MAP_IMPORTED_CONFIG_DEBUG MISSING
        )
    endif()
    if(BPR_FAKE_QT_WINDOWS_MAP_EMPTY_WITH_GENERAL)
        set_target_properties(
            "Qt6::${component}"
            PROPERTIES
                IMPORTED_LOCATION "${_bpr_fake_root}/artifacts/windows-release.dll"
                IMPORTED_IMPLIB "${_bpr_fake_root}/artifacts/windows-release.lib"
                MAP_IMPORTED_CONFIG_DEBUG ""
        )
    endif()
    if(BPR_FAKE_QT_WINDOWS_DEBUG_MISSING)
        set_target_properties(
            "Qt6::${component}"
            PROPERTIES
                IMPORTED_LOCATION_DEBUG ""
                IMPORTED_IMPLIB_DEBUG ""
        )
    endif()
    if(BPR_FAKE_QT_WINDOWS_DEBUG_IMPLIB_UNSET)
        set_property(
            TARGET "Qt6::${component}"
            PROPERTY IMPORTED_IMPLIB_DEBUG
        )
    endif()
    if(BPR_FAKE_QT_WINDOWS_DEBUG_LOCATION_UNSET)
        set_property(
            TARGET "Qt6::${component}"
            PROPERTY IMPORTED_LOCATION_DEBUG
        )
    endif()
    if(BPR_FAKE_QT_WINDOWS_IMPLIB_ONLY)
        set_target_properties(
            "Qt6::${component}"
            PROPERTIES
                IMPORTED_LOCATION_DEBUG ""
                IMPORTED_LOCATION_RELEASE ""
        )
    endif()
    if(BPR_FAKE_QT_WINDOWS_DEBUG_IMPLIB_ONLY)
        set_property(
            TARGET "Qt6::${component}"
            PROPERTY IMPORTED_LOCATION_DEBUG ""
        )
    endif()
    if(BPR_FAKE_QT_WINDOWS_LOCATION_ONLY)
        set_target_properties(
            "Qt6::${component}"
            PROPERTIES
                IMPORTED_IMPLIB_DEBUG ""
                IMPORTED_IMPLIB_RELEASE ""
        )
    endif()
    set("Qt6_${component}_FOUND" TRUE)
endforeach()

set_property(
    TARGET Qt6::Core
    APPEND
    PROPERTY INTERFACE_COMPILE_DEFINITIONS
    "$<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG>"
)
set_property(
    TARGET Qt6::Core
    PROPERTY INTERFACE_COMPILE_FEATURES "cxx_std_17;cxx_decltype"
)
set_property(
    TARGET Qt6::Core
    PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads
)
set_property(
    TARGET Qt6::Gui
    PROPERTY INTERFACE_LINK_LIBRARIES "Qt6::Core;fake-platform-library"
)
set_property(
    TARGET Qt6::Widgets
    PROPERTY INTERFACE_LINK_LIBRARIES "Qt6::Core;Qt6::Gui"
)
set_property(
    TARGET Qt6::Network
    PROPERTY INTERFACE_LINK_LIBRARIES Qt6::Core
)

if(BPR_FAKE_QT_UNSUPPORTED)
    set_property(
        TARGET Qt6::Core
        APPEND
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        "$<IF:$<BOOL:1>,unsupported,metadata>"
    )
endif()

if(BPR_FAKE_QT_INCOMPLETE)
    set_property(
        TARGET Qt6::Core
        APPEND
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        "${_bpr_fake_root}/missing include directory"
    )
endif()

if(BPR_FAKE_QT_DIRECT_LINK)
    set_property(
        TARGET Qt6::Core
        PROPERTY INTERFACE_LINK_LIBRARIES_DIRECT fake-direct-library
    )
endif()

if(BPR_FAKE_QT_AUTOMOC_MACRO_NAMES)
    set_property(
        TARGET Qt6::Core
        PROPERTY INTERFACE_AUTOMOC_MACRO_NAMES CUSTOM_QT_OBJECT
    )
endif()

if(BPR_FAKE_QT_CUSTOM_TRANSITIVE_PROPERTY)
    set_target_properties(
        Qt6::Core
        PROPERTIES
            TRANSITIVE_COMPILE_PROPERTIES CUSTOM_C
            INTERFACE_CUSTOM_C CUSTOM_VALUE
    )
endif()

if(BPR_FAKE_QT_FALSE_DIRECT_LINK)
    set_property(
        TARGET Qt6::Core
        PROPERTY INTERFACE_LINK_LIBRARIES_DIRECT OFF
    )
endif()

if(BPR_FAKE_QT_FALSE_SUPPORTED_VALUES)
    set_target_properties(
        Qt6::Core
        PROPERTIES
            INTERFACE_COMPILE_DEFINITIONS OFF
            INTERFACE_LINK_LIBRARIES OFF
    )
endif()

if(BPR_FAKE_QT_POLICY_ORACLE)
    file(
        GENERATE
        OUTPUT "${BPR_FAKE_QT_POLICY_ORACLE}"
        CONTENT "$<TARGET_FILE:Qt6::Core>"
    )
endif()

set(Qt6_FOUND TRUE)

cmake_policy(POP)
