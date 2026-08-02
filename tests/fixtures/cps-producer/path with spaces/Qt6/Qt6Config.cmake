if(NOT BPR_FAKE_QT_VERSION)
    set(BPR_FAKE_QT_VERSION "6.8.1")
endif()
set(Qt6_VERSION "${BPR_FAKE_QT_VERSION}")
get_filename_component(_bpr_fake_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

foreach(component IN ITEMS Core Gui Widgets Network)
    add_library("Qt6::${component}" SHARED IMPORTED GLOBAL)
    string(TOLOWER "${component}" component_lower)
    string(TOUPPER "${component}" component_upper)
    set_target_properties(
        "Qt6::${component}"
        PROPERTIES
            IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
            IMPORTED_LOCATION_DEBUG "${_bpr_fake_root}/artifacts/${component_lower}-debug.dylib"
            IMPORTED_LOCATION_RELEASE "${_bpr_fake_root}/artifacts/${component_lower}-release.dylib"
            INTERFACE_INCLUDE_DIRECTORIES "${_bpr_fake_root}/include path/${component}"
            INTERFACE_COMPILE_DEFINITIONS "QT_${component_upper}_LIB"
    )
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

set(Qt6_FOUND TRUE)
