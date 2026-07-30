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
