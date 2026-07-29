find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
    NAME core_dependency_boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyCoreBoundary.cmake"
)

add_test(
    NAME architecture_boundaries
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyArchitectureBoundaries.cmake"
)

add_test(
    NAME documentation_structure
    COMMAND
        "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/scripts/verify_documentation.py"
        --root
        "${PROJECT_SOURCE_DIR}"
)

add_custom_target(
    check_boundaries
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyCoreBoundary.cmake"
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyArchitectureBoundaries.cmake"
    VERBATIM
)

add_subdirectory(core)
add_subdirectory(application)
add_subdirectory(contract)

if(BPR_BUILD_QT)
    add_subdirectory(adapters_qt)
    add_subdirectory(end_to_end)
endif()
