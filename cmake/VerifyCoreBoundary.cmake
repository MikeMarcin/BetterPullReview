if(NOT DEFINED BPR_ROOT)
    message(FATAL_ERROR "BPR_ROOT is required")
endif()

file(
    GLOB_RECURSE core_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/core/*.cc"
    "${BPR_ROOT}/src/core/*.cpp"
    "${BPR_ROOT}/src/core/*.cxx"
    "${BPR_ROOT}/src/core/*.h"
    "${BPR_ROOT}/src/core/*.hh"
    "${BPR_ROOT}/src/core/*.hpp"
    "${BPR_ROOT}/src/core/*.hxx"
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
    "#[ \t]*include[ \t]*[<\"]chrono[>\"]"
    "#[ \t]*include[ \t]*[<\"]ctime[>\"]"
    "#[ \t]*include[ \t]*[<\"]random[>\"]"
    "#[ \t]*include[ \t]*[<\"]fstream[>\"]"
    "#[ \t]*include[ \t]*[<\"](atomic|mutex|condition_variable)[>\"]"
    "#[ \t]*include[ \t]*[<\"](asio|boost/asio)"
    "#[ \t]*include[ \t]*[<\"](nlohmann|rapidjson|boost/json)"
    "#[ \t]*include[ \t]*[<\"](sys/socket|winsock)"
    "std::filesystem"
    "std::chrono"
    "std::time[ \t]*\\("
    "(^|[^A-Za-z0-9_])(time|clock_gettime|GetSystemTime)[ \t]*\\("
    "(^|[^A-Za-z0-9_])(std::|::)?getenv[ \t]*\\("
    "(^|[^A-Za-z0-9_])(std::|::)?system[ \t]*\\("
    "(^|[^A-Za-z0-9_])(fopen|freopen)[ \t]*\\("
    "(^|[^A-Za-z0-9_])(popen|fork|execve|CreateProcess)[ \t]*\\("
    "(^|[^A-Za-z0-9_])(socket|connect|listen)[ \t]*\\("
    "(^|[^A-Za-z0-9_])(SecItem|CredRead|CredWrite|CredDelete)"
    "(^|[^A-Za-z0-9_])(SecretService|libsecret|secret_password_)"
    "https?://"
    # Anchored like every other identifier pattern here. Without the
    # boundary it matched inside ordinary uppercase words: the include
    # guard `BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_HPP` contains
    # `QUEUE`, which is not a Qt type and never was. A real Qt name is
    # always preceded by a space, an angle bracket, a colon, or a
    # pointer or reference token.
    "(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*"
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

# Owner, 2026-10-13: core may link only these pinned, pure (no I/O, threads,
# clocks or globals) libraries. Anything else needs the owner's approval first.
set(core_allowed_link_targets glaze::glaze BetterPullReview::ZLib BetterPullReview::UTF8Proc BetterPullReview::Fmt)
set(core_cmake "${BPR_ROOT}/src/core/CMakeLists.txt")
if(EXISTS "${core_cmake}")
    file(READ "${core_cmake}" core_cmake_contents)
    string(REGEX MATCHALL "target_link_libraries[ \t]*\\([^)]*\\)" core_links "${core_cmake_contents}")
    foreach(core_link IN LISTS core_links)
        string(REGEX REPLACE "^target_link_libraries[ \t]*\\(" "" core_items "${core_link}")
        string(REGEX REPLACE "\\)$" "" core_items "${core_items}")
        string(REGEX REPLACE "[ \t\r\n]+" ";" core_items "${core_items}")
        list(REMOVE_ITEM core_items "")
        list(POP_FRONT core_items core_link_target)
        if(NOT core_link_target STREQUAL "better_pull_review_core")
            continue()
        endif()
        foreach(core_item IN LISTS core_items)
            if(core_item STREQUAL "" OR core_item MATCHES "^(PUBLIC|PRIVATE|INTERFACE)$")
                continue()
            endif()
            if(NOT core_item IN_LIST core_allowed_link_targets)
                message(
                    FATAL_ERROR
                    "Functional-core boundary violation in src/core/CMakeLists.txt: "
                    "${core_item} is not an owner-approved core library"
                )
            endif()
        endforeach()
    endforeach()
endif()

message(STATUS "Functional-core dependency boundary verified")
