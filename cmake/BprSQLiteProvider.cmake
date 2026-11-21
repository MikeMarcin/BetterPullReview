include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

set(BPR_SQLITE_VERSION "3.54.1")
set(BPR_SQLITE_SOURCE_NUMBER "3540100")
set(
    BPR_SQLITE_ARCHIVE_SHA3_256
    "92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30"
)
set(
    BPR_SQLITE_C_SHA3_256
    "e2b869d3003b33c942b7dd7063604fbc53553e1b66609570df39b9b7b558ee81"
)
set(
    BPR_SQLITE_H_SHA3_256
    "15ef3f5dbc4a7a980f828fa0d1e473cee5a124e7bb77e9bc70fc0bfa3f9a2af8"
)
set(
    BPR_SQLITE_SOURCE_ID
    "2026-10-16 14:45:20 555e07ef33f69a0957d24d8e555b702464a131d6f1607f4c3c1e4551e8e6ca76"
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

function(bpr_stage_sqlite_source source_dir output)
    bpr_validate_sqlite_source("${source_dir}")

    set(stage "${CMAKE_BINARY_DIR}/_bpr/sqlite/${BPR_SQLITE_VERSION}")
    file(MAKE_DIRECTORY "${stage}")
    file(GLOB stage_entries LIST_DIRECTORIES TRUE "${stage}/*")
    foreach(entry IN LISTS stage_entries)
        get_filename_component(name "${entry}" NAME)
        if(NOT name STREQUAL "sqlite3.c" AND NOT name STREQUAL "sqlite3.h")
            message(
                FATAL_ERROR
                "Controlled SQLite staging directory contains unexpected "
                "entry: '${entry}'"
            )
        endif()
    endforeach()
    foreach(name IN ITEMS sqlite3.c sqlite3.h)
        if(IS_SYMLINK "${stage}/${name}")
            message(
                FATAL_ERROR
                "Controlled SQLite staging file must not be a symlink: "
                "'${stage}/${name}'"
            )
        endif()
        configure_file(
            "${source_dir}/${name}"
            "${stage}/${name}"
            COPYONLY
        )
    endforeach()

    bpr_validate_sqlite_source("${stage}")
    set(${output} "${stage}" PARENT_SCOPE)
endfunction()

function(bpr_acquire_sqlite output)
    if(
        DEFINED FETCHCONTENT_SOURCE_DIR_BPRSQLITE
        AND NOT FETCHCONTENT_SOURCE_DIR_BPRSQLITE STREQUAL ""
    )
        message(
            FATAL_ERROR
            "FETCHCONTENT_SOURCE_DIR_BPRSQLITE is forbidden; SQLite must use the verified CPM provider"
        )
    endif()
    set(
        FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER
        CACHE STRING
        "SQLite must not resolve through find_package"
        FORCE
    )
    bpr_load_cpm()
    set(
        sqlite_lock
        "${CPM_SOURCE_CACHE}/bprsqlite-${BPR_SQLITE_VERSION}.lock"
    )
    file(
        LOCK "${sqlite_lock}"
        GUARD FUNCTION
        TIMEOUT 300
        RESULT_VARIABLE lock_result
    )
    if(lock_result)
        message(
            FATAL_ERROR
            "Unable to lock the shared SQLite CPM cache: ${lock_result}"
        )
    endif()
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
            FORCE YES
        )
    else()
        CPMAddPackage(
            NAME BprSQLite
            VERSION "${BPR_SQLITE_VERSION}"
            URL "https://sqlite.org/2026/sqlite-amalgamation-${BPR_SQLITE_SOURCE_NUMBER}.zip"
            URL_HASH "SHA3_256=${BPR_SQLITE_ARCHIVE_SHA3_256}"
            DOWNLOAD_ONLY YES
            FORCE YES
        )
    endif()
    if(NOT BprSQLite_SOURCE_DIR OR NOT EXISTS "${BprSQLite_SOURCE_DIR}")
        message(FATAL_ERROR "CPM did not provide SQLite ${BPR_SQLITE_VERSION}")
    endif()
    bpr_stage_sqlite_source("${BprSQLite_SOURCE_DIR}" staged_source)
    set(${output} "${staged_source}" PARENT_SCOPE)
endfunction()
