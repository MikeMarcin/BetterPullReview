if(BPR_BUILD_QT OR BPR_BUILD_TOUR_CLI)
    bpr_add_network_isolated_test(NAME mimalloc_provider WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/mimalloc_provider_tests.py"
            "${PROJECT_SOURCE_DIR}" "${CPM_SOURCE_CACHE}")
    set_tests_properties(mimalloc_provider PROPERTIES TIMEOUT 240)
endif()

if(NOT (APPLE OR WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux") OR NOT BPR_MIMALLOC_ACTIVE)
    return()
endif()

add_library(bpr_mimalloc_probe_module SHARED module.cpp)
target_link_libraries(bpr_mimalloc_probe_module PRIVATE Qt6::Core)
set_target_properties(bpr_mimalloc_probe_module PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
target_compile_features(bpr_mimalloc_probe_module PRIVATE cxx_std_23)
bpr_enable_warnings(bpr_mimalloc_probe_module)
get_property(products GLOBAL PROPERTY BPR_PRODUCT_EXECUTABLES)
foreach(product IN LISTS products)
    target_sources("${product}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/process_probe.cpp")
    target_compile_features("${product}" PRIVATE cxx_std_23)
    target_link_libraries("${product}" PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    add_dependencies("${product}" bpr_mimalloc_probe_module)
    bpr_add_network_isolated_test(NAME "mimalloc_${product}" WINDOWS_UNCONTAINED
        COMMAND "$<TARGET_FILE:${product}>")
    set_tests_properties("mimalloc_${product}" PROPERTIES TIMEOUT 60
        ENVIRONMENT "BPR_MIMALLOC_PROBE=1;BPR_MIMALLOC_PROBE_MODULE=$<TARGET_FILE:bpr_mimalloc_probe_module>"
        PASS_REGULAR_EXPRESSION "MIMALLOC_PROBE_PASS")
endforeach()

bpr_add_executable(test_mimalloc_unattached unattached.cpp process_probe.cpp)
target_compile_features(test_mimalloc_unattached PRIVATE cxx_std_23)
target_link_libraries(test_mimalloc_unattached PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
bpr_enable_warnings(test_mimalloc_unattached)
bpr_add_network_isolated_test(NAME mimalloc_missing_override
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/negative_control.py"
        "$<TARGET_FILE:test_mimalloc_unattached>")

if(WIN32)
    bpr_add_network_isolated_test(NAME mimalloc_disabled_redirect WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/negative_control.py"
            "$<TARGET_FILE:app_better_pull_review>" --disabled-redirect)
endif()
