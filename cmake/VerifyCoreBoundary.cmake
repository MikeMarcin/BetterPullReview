if(NOT DEFINED BPR_ROOT)
    message(FATAL_ERROR "BPR_ROOT is required")
endif()

file(
    GLOB_RECURSE core_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/core/*.c"
    "${BPR_ROOT}/src/core/*.cc"
    "${BPR_ROOT}/src/core/*.cpp"
    "${BPR_ROOT}/src/core/*.cxx"
    "${BPR_ROOT}/src/core/*.h"
    "${BPR_ROOT}/src/core/*.hh"
    "${BPR_ROOT}/src/core/*.hpp"
    "${BPR_ROOT}/src/core/*.hxx"
    "${BPR_ROOT}/src/core/*.m"
    "${BPR_ROOT}/src/core/*.mm"
)

if(NOT core_files)
    message(FATAL_ERROR "No functional-core sources were found")
endif()

set(filesystem_effects "absolute|canonical|weakly_canonical|relative|proximate|current_path|temp_directory_path|exists|status|symlink_status|is_regular_file|is_directory|is_symlink|is_block_file|is_character_file|is_fifo|is_socket|is_other|is_empty|equivalent|file_size|hard_link_count|last_write_time|space|copy|copy_file|copy_symlink|create_directory|create_directories|create_hard_link|create_symlink|create_directory_symlink|read_symlink|remove|remove_all|rename|resize_file|permissions|directory_iterator|recursive_directory_iterator|directory_entry")
set(standard_clocks "(system|steady|high_resolution|utc|tai|gps|file)_clock")
set(file_stream_types "(w?([io]?fstream|filebuf)|basic_([io]?fstream|filebuf)<[^;{}]+>)")
set(acquisition_types "std::(j?thread|${file_stream_types})")

set(
    forbidden_patterns
    "#[ \t]*include[ \t]*[<\"]Q"
    "#[ \t]*include[ \t]*[<\"]Qt"
    "#[ \t]*include[ \t]*[<\"]sqlite3"
    "#[ \t]*include[ \t]*[<\"]curl"
    "#[ \t]*include[ \t]*[<\"](asio|boost/asio)"
    "#[ \t]*include[ \t]*[<\"](nlohmann|rapidjson|boost/json)"
    "#[ \t]*include[ \t]*[<\"](sys/socket|winsock)"
    "#[ \t]*include[ \t]*[<\"](windows\\.h|bcrypt\\.h|CommonCrypto/)"
    "(^|[^A-Za-z0-9_])(BCrypt[A-Za-z0-9_]*|BCRYPT_[A-Za-z0-9_]*|CC_SHA[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)"
    "std::filesystem::[ \t\r\n]*(${filesystem_effects})([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_:])((std::chrono::)?${standard_clocks}|std::filesystem::file_time_type::clock)::now[ \t\r\n]*\\("
    # Headers and value/reference declarations are not effects. These known
    # forms acquire a file/worker or invoke host scheduling. A nonempty
    # constructor is conservative: it can also move an existing handle.
    "${acquisition_types}[ \t\r\n]*([A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*(=[ \t\r\n]*)?)?[({][ \t\r\n]*[^ \t\r\n)}]"
    "std::async[ \t\r\n]*(<[^;{}]+>)?[ \t\r\n]*\\("
    "std::this_thread::(sleep_for|sleep_until|yield|get_id)[ \t\r\n]*\\("
    "std::(j?thread)::hardware_concurrency[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_])(std::)?random_device([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_.>:])(std::|::)?(time|clock|clock_gettime|gettimeofday|timespec_get|GetSystemTime|QueryPerformanceCounter|GetTickCount(64)?)[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_.>:])(std::|::)?(localtime(_[rs])?|mktime|ctime(_[rs])?|strftime|_?tzset)[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_.>:])(std::|::)?(rand|srand|random|srandom)[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_.>:])(std::|::)?getenv[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_.>:])(std::|::)?system[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_.>:])(std::|::)?(fopen|freopen)[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_.>:])(::)?(popen|fork|execve|CreateProcess)[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_.>:])(::)?(socket|connect|listen)[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_])(SecItem|CredRead|CredWrite|CredDelete)"
    "(^|[^A-Za-z0-9_])(SecretService|libsecret|secret_password_)"
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
    string(REGEX REPLACE "[ \t\r\n]*::[ \t\r\n]*" "::" contents "${contents}")
    string(REPLACE "::std::" "std::" contents "${contents}")
    # Normalize simple namespace/type aliases for the capabilities checked
    # here, not arbitrary user types. This textual guard does not resolve C++
    # scopes, templates, macros, indirect calls or upstream implementation.
    string(REGEX MATCHALL "namespace[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*=[ \t\r\n]*(::)?std(::(filesystem|chrono|this_thread))?[ \t]*;" namespace_aliases "${contents}")
    foreach(namespace_alias IN LISTS namespace_aliases)
        string(REGEX REPLACE "namespace[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*=.*" "\\1" alias_name "${namespace_alias}")
        string(REGEX REPLACE ".*=[ \t\r\n]*(::)?(std(::[A-Za-z_]+)?)[ \t]*$" "\\2" alias_target "${namespace_alias}")
        string(REGEX REPLACE "(^|[^A-Za-z0-9_])${alias_name}::" "\\1${alias_target}::" contents "${contents}")
    endforeach()
    string(REGEX MATCHALL "using[ \t\r\n]+std::(async|j?thread|${file_stream_types}|this_thread::(sleep_for|sleep_until|yield|get_id))[ \t]*;" capability_imports "${contents}")
    foreach(capability_import IN LISTS capability_imports)
        string(REGEX REPLACE "using[ \t\r\n]+(std::.*)[ \t]*$" "\\1" imported_name "${capability_import}")
        string(STRIP "${imported_name}" imported_name)
        string(REGEX REPLACE ".*::" "" local_name "${imported_name}")
        string(REGEX REPLACE "(^|[^A-Za-z0-9_:])${local_name}([^A-Za-z0-9_]|$)" "\\1${imported_name}\\2" contents "${contents}")
    endforeach()
    # Explicit deferred async creates lazy caller-owned state, not a worker.
    # Remove only this launch-policy prefix; the callable still scans for effects.
    string(REGEX REPLACE "std::async[ \t\r\n]*\\([ \t\r\n]*std::launch::deferred[ \t\r\n]*," "deferred_computation(" contents "${contents}")
    set(alias_types "std::(chrono::${standard_clocks}|filesystem::file_time_type::clock|random_device|j?thread|${file_stream_types})")
    string(REGEX MATCHALL "using[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*[ \t]*=[ \t\r\n]*${alias_types}[ \t]*;|typedef[ \t\r\n]+${alias_types}[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*[ \t]*;" type_aliases "${contents}")
    foreach(type_alias IN LISTS type_aliases)
        if(type_alias MATCHES "^using[ \t\r\n]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*=[ \t\r\n]*(.*)[ \t]*$")
            set(alias_name "${CMAKE_MATCH_1}")
            set(alias_target "${CMAKE_MATCH_2}")
        elseif(type_alias MATCHES "^typedef[ \t\r\n]+(.*)[ \t\r\n]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*$")
            set(alias_name "${CMAKE_MATCH_2}")
            set(alias_target "${CMAKE_MATCH_1}")
        endif()
        string(STRIP "${alias_target}" alias_target)
        string(REGEX REPLACE "(^|[^A-Za-z0-9_])${alias_name}([^A-Za-z0-9_]|$)" "\\1${alias_target}\\2" contents "${contents}")
    endforeach()
    # A default-constructed stream does not open a file. Check open only on
    # named standard file streams/buffers, never arbitrary domain .open().
    string(REGEX MATCHALL "std::${file_stream_types}[&* \t\r\n]+[A-Za-z_][A-Za-z0-9_]*" file_stream_declarations "${contents}")
    foreach(declaration IN LISTS file_stream_declarations)
        string(REGEX REPLACE ".*[^A-Za-z0-9_]([A-Za-z_][A-Za-z0-9_]*)$" "\\1" stream_name "${declaration}")
        if(contents MATCHES "(^|[^A-Za-z0-9_])${stream_name}[ \t\r\n]*(\\.|->)[ \t\r\n]*open[ \t\r\n]*\\(")
            message(FATAL_ERROR "Functional-core boundary violation: file stream acquisition in ${core_file}")
        endif()
    endforeach()
    if(contents MATCHES "using[ \t\r\n]+namespace[ \t\r\n]+std::filesystem[ \t]*;")
        if(contents MATCHES "(^|[^A-Za-z0-9_])(${filesystem_effects})[ \t\r\n]*[(\{]")
            message(FATAL_ERROR "Functional-core boundary violation: filesystem effect through imported namespace")
        endif()
    endif()
    # The approved platform SHA-256 wrappers have two exact owners;
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

# Approved dependency inventory, not a definition of semantic determinism.
# External determinism governs computation; adoption still needs owner approval.
# Source checks preserve effect ownership and private wrappers, not a full
# semantic/effect audit of upstream implementations.
# sha1collisiondetection and HACL*'s generated SHA-2 were approved 2026-10-13
# (RFC-BPR-0080 Q1 and Q2), and rapidfuzz-cpp and dtl through RFC-BPR-0082 Q2
# (dtl only for per-line intraline comparison and the Review Cycle suggestion
# hunks' line edit bounded at 512 changes; architecture_boundaries keeps it to
# those two sources).
# Owner, 2026-10-18: Git xdiff may be privately called from core, but it
# stays its own dynamically linked shared library (BPR-1619).
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
    BetterPullReview::Xdiff
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
