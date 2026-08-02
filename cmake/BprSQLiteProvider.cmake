include_guard(GLOBAL)

include(AitoCpm)

set(BPR_SQLITE_VERSION "3.53.4")
set(BPR_SQLITE_SOURCE_NUMBER "3530400")
set(
    BPR_SQLITE_ARCHIVE_SHA3_256
    "628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e"
)
set(
    BPR_SQLITE_C_SHA3_256
    "67f423e9ebbbdc473cbc4772c872ee6b89f31fde4ed0279a5c25d5f65c043a16"
)
set(
    BPR_SQLITE_H_SHA3_256
    "89d0de498b1012938bb78dc18b68fea52e26f7f7b6db6347d126a2e58b86a05c"
)
set(
    BPR_SQLITE_SOURCE_ID
    "2026-07-24 19:02:57 bf7c7f30031888f4e796e429ab3978879485813aaca6f641c7b33e4e09459bcc"
)

function(bpr_validate_sqlite_source source_dir)
    set(sqlite_c "${source_dir}/sqlite3.c")
    set(sqlite_h "${source_dir}/sqlite3.h")
    foreach(path IN ITEMS "${sqlite_c}" "${sqlite_h}")
        if(NOT EXISTS "${path}")
            message(FATAL_ERROR "Verified SQLite archive is incomplete: '${path}'")
        endif()
    endforeach()

    file(SHA3_256 "${sqlite_c}" sqlite_c_sha3)
    file(SHA3_256 "${sqlite_h}" sqlite_h_sha3)
    if(NOT sqlite_c_sha3 STREQUAL BPR_SQLITE_C_SHA3_256)
        message(FATAL_ERROR "sqlite3.c does not match SQLite ${BPR_SQLITE_VERSION}")
    endif()
    if(NOT sqlite_h_sha3 STREQUAL BPR_SQLITE_H_SHA3_256)
        message(FATAL_ERROR "sqlite3.h does not match SQLite ${BPR_SQLITE_VERSION}")
    endif()

    file(STRINGS "${sqlite_h}" version_lines REGEX "^#define SQLITE_VERSION[ ]+")
    if(NOT version_lines MATCHES "\"${BPR_SQLITE_VERSION}\"")
        message(FATAL_ERROR "sqlite3.h reports the wrong SQLITE_VERSION")
    endif()
    file(STRINGS "${sqlite_c}" source_id_lines REGEX "SQLITE_SOURCE_ID")
    string(FIND "${source_id_lines}" "${BPR_SQLITE_SOURCE_ID}" source_id_index)
    if(source_id_index EQUAL -1)
        message(FATAL_ERROR "sqlite3.c reports the wrong SQLITE_SOURCE_ID")
    endif()
endfunction()

function(bpr_find_cached_sqlite output)
    file(
        GLOB_RECURSE candidates
        LIST_DIRECTORIES FALSE
        "${CPM_SOURCE_CACHE}/bprsqlite/*/sqlite3.c"
    )
    foreach(sqlite_c IN LISTS candidates)
        get_filename_component(candidate "${sqlite_c}" DIRECTORY)
        if(NOT EXISTS "${candidate}/sqlite3.h")
            continue()
        endif()
        file(SHA3_256 "${sqlite_c}" sqlite_c_sha3)
        file(SHA3_256 "${candidate}/sqlite3.h" sqlite_h_sha3)
        if(
            sqlite_c_sha3 STREQUAL BPR_SQLITE_C_SHA3_256
            AND sqlite_h_sha3 STREQUAL BPR_SQLITE_H_SHA3_256
        )
            set(${output} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${output} "" PARENT_SCOPE)
endfunction()

function(bpr_acquire_sqlite output)
    bpr_load_cpm()
    set(CPM_BprSQLite_SOURCE "")
    set(CPM_DECLARATION_BprSQLite "")
    if(BPR_DEPENDENCY_OFFLINE)
        bpr_find_cached_sqlite(cached_source)
        if(NOT cached_source)
            message(
                FATAL_ERROR
                "BPR_DEPENDENCY_OFFLINE=ON requires SQLite ${BPR_SQLITE_VERSION} "
                "in CPM_SOURCE_CACHE='${CPM_SOURCE_CACHE}'"
            )
        endif()
        CPMAddPackage(
            NAME BprSQLite
            VERSION "${BPR_SQLITE_VERSION}"
            SOURCE_DIR "${cached_source}"
            DOWNLOAD_ONLY YES
        )
    else()
        CPMAddPackage(
            NAME BprSQLite
            VERSION "${BPR_SQLITE_VERSION}"
            URL "https://sqlite.org/2026/sqlite-amalgamation-${BPR_SQLITE_SOURCE_NUMBER}.zip"
            URL_HASH "SHA3_256=${BPR_SQLITE_ARCHIVE_SHA3_256}"
            DOWNLOAD_ONLY YES
        )
    endif()
    if(NOT BprSQLite_SOURCE_DIR OR NOT EXISTS "${BprSQLite_SOURCE_DIR}")
        message(FATAL_ERROR "CPM did not provide SQLite ${BPR_SQLITE_VERSION}")
    endif()
    bpr_validate_sqlite_source("${BprSQLite_SOURCE_DIR}")
    set(${output} "${BprSQLite_SOURCE_DIR}" PARENT_SCOPE)
endfunction()
