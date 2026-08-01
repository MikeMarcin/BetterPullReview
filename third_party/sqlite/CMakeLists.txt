find_package(Threads REQUIRED)

add_library(
    better_pull_review_sqlite3
    STATIC
        amalgamation/sqlite3.c
)
add_library(
    BetterPullReview::SQLite3
    ALIAS
    better_pull_review_sqlite3
)

target_include_directories(
    better_pull_review_sqlite3
    SYSTEM
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/amalgamation"
)
target_link_libraries(
    better_pull_review_sqlite3
    PUBLIC
        Threads::Threads
)
target_compile_definitions(
    better_pull_review_sqlite3
    PUBLIC
        SQLITE_DEFAULT_FILE_PERMISSIONS=0600
        SQLITE_DEFAULT_FOREIGN_KEYS=1
        SQLITE_DEFAULT_SYNCHRONOUS=2
        SQLITE_DEFAULT_WAL_SYNCHRONOUS=2
        SQLITE_DQS=0
        SQLITE_ENABLE_API_ARMOR=1
        SQLITE_MAX_ATTACHED=0
        SQLITE_MAX_PAGE_COUNT=262144
        SQLITE_OMIT_AUTOINIT=1
        SQLITE_OMIT_DEPRECATED=1
        SQLITE_OMIT_LOAD_EXTENSION=1
        SQLITE_OMIT_SHARED_CACHE=1
        SQLITE_TEMP_STORE=3
        SQLITE_THREADSAFE=1
        SQLITE_TRUSTED_SCHEMA=0
        SQLITE_USE_URI=0
)
set_target_properties(
    better_pull_review_sqlite3
    PROPERTIES
        C_EXTENSIONS OFF
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
)

bpr_enable_hardening(better_pull_review_sqlite3)
