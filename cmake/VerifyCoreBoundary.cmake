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
    "#[ \t]*include[ \t]*[<\"]ctime[>\"]"
    "#[ \t]*include[ \t]*[<\"]fstream[>\"]"
    "#[ \t]*include[ \t]*[<\"](atomic|mutex|condition_variable)[>\"]"
    "#[ \t]*include[ \t]*[<\"](asio|boost/asio)"
    "#[ \t]*include[ \t]*[<\"](nlohmann|rapidjson|boost/json)"
    "#[ \t]*include[ \t]*[<\"](sys/socket|winsock)"
    "#[ \t]*include[ \t]*[<\"](windows\\.h|bcrypt\\.h|CommonCrypto/)"
    "(^|[^A-Za-z0-9_])(BCrypt[A-Za-z0-9_]*|BCRYPT_[A-Za-z0-9_]*|CC_SHA[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)"
    "std::filesystem"
    "::[ \t\r\n]*now[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_])(std::)?random_device([^A-Za-z0-9_]|$)"
    "std::time[ \t]*\\("
    "(^|[^A-Za-z0-9_])(time|clock|clock_gettime|gettimeofday|timespec_get|GetSystemTime|QueryPerformanceCounter|GetTickCount(64)?)[ \t]*\\("
    "(^|[^A-Za-z0-9_])(rand|srand|random|srandom)[ \t]*\\("
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
    # The owner's deterministic SHA-256 exception has two exact owners;
    # all ordinary effect patterns still scan those files.
    if(core_file STREQUAL "${BPR_ROOT}/src/core/src/sha256_commoncrypto.cpp")
        string(REPLACE "#include <CommonCrypto/CommonDigest.h>" "" contents "${contents}")
        string(REGEX REPLACE "CC_SHA256_(CTX|DIGEST_LENGTH|Init|Update|Final)" "" contents "${contents}")
    elseif(core_file STREQUAL "${BPR_ROOT}/src/core/src/sha256_cng.cpp")
        string(REPLACE "#include <windows.h>" "" contents "${contents}")
        string(REPLACE "#include <bcrypt.h>" "" contents "${contents}")
        string(REGEX REPLACE "(BCrypt(CreateHash|DuplicateHash|HashData|FinishHash|DestroyHash)|BCRYPT_SHA256_ALG_HANDLE|BCRYPT_HASH_HANDLE)" "" contents "${contents}")
    endif()
    if(contents MATCHES "#[ \t]*include[ \t]*[<\"](re2/|absl/)"
       AND (NOT core_file STREQUAL "${BPR_ROOT}/src/core/src/source_regex_engine.cpp"
            OR contents MATCHES "#[ \t]*include[ \t]*[<\"]absl/"))
        message(FATAL_ERROR "Functional-core boundary violation: RE2 belongs only to the private source matcher; Abseil is transitive only")
    endif()
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
# sha1collisiondetection and HACL*'s generated SHA-2 were approved the same day
# (RFC-BPR-0080 Q1 and Q2), and rapidfuzz-cpp and dtl through RFC-BPR-0082 Q2
# (dtl only for per-line intraline comparison and the Review Cycle suggestion
# hunks' line edit bounded at 512 changes; architecture_boundaries keeps it to
# those two sources).
# Owner, 2026-10-18: pinned RE2/Abseil may implement source matching with
# externally deterministic results and no worker creation (BPR-1618). Only
# the private production wrapper includes RE2; Abseil is transitive only.
set(
    core_allowed_link_targets
    glaze::glaze
    BetterPullReview::ZLib
    BetterPullReview::UTF8Proc
    BetterPullReview::Fmt
    BetterPullReview::Sha1dc
    BetterPullReview::HaclSha2
    BetterPullReview::RapidFuzz
    BetterPullReview::Dtl
    BetterPullReview::RE2
    bcrypt
)
set(core_cmake "${BPR_ROOT}/src/core/CMakeLists.txt")
if(EXISTS "${core_cmake}")
    file(READ "${core_cmake}" core_cmake_contents)
    # RFC-BPR-0080 r4: only the Windows SHA-256 source branch may link CNG.
    if(core_cmake_contents MATCHES "target_link_libraries\\([^)]*bcrypt"
       AND NOT core_cmake_contents MATCHES "elseif\\(WIN32\\)[ \t\r\n]+target_sources\\(better_pull_review_core PRIVATE src/sha256_cng.cpp\\)[ \t\r\n]+target_link_libraries\\(better_pull_review_core PRIVATE bcrypt\\)")
        message(FATAL_ERROR "Functional-core boundary violation: bcrypt must belong to the Windows SHA-256 branch")
    endif()
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
