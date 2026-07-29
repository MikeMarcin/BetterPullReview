if(NOT DEFINED BPR_ROOT)
    message(FATAL_ERROR "BPR_ROOT is required")
endif()

file(
    GLOB_RECURSE core_files
    "${BPR_ROOT}/src/core/*.cpp"
    "${BPR_ROOT}/src/core/*.h"
    "${BPR_ROOT}/src/core/*.hpp"
)

if(NOT core_files)
    message(FATAL_ERROR "No functional-core sources were found")
endif()

set(
    forbidden_patterns
    "#[ \t]*include[ \t]*[<\"]Q"
    "#[ \t]*include[ \t]*[<\"]Qt"
    "#[ \t]*include[ \t]*[<\"]sqlite3"
    "#[ \t]*include[ \t]*[<\"]curl"
    "#[ \t]*include[ \t]*[<\"]thread[>\"]"
    "#[ \t]*include[ \t]*[<\"]future[>\"]"
    "#[ \t]*include[ \t]*[<\"]filesystem[>\"]"
    "#[ \t]*include[ \t]*[<\"]random[>\"]"
    "std::filesystem"
    "Q[A-Z][A-Za-z0-9_]*"
)

foreach(core_file IN LISTS core_files)
    file(READ "${core_file}" contents)
    foreach(pattern IN LISTS forbidden_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${core_file}")
            message(
                FATAL_ERROR
                "Functional-core boundary violation in ${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "Functional-core dependency boundary verified")
