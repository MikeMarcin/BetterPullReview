include_guard(GLOBAL)

set(BPR_QT_MINIMUM_VERSION "6.8")
set(BPR_LIBSECRET_MINIMUM_VERSION "0.20")
set(BPR_PYTHON_MINIMUM_VERSION "3.10")
set(BPR_GIT_MINIMUM_VERSION "2.39")

function(bpr_require_dependency_target target dependency)
    if(NOT TARGET "${target}")
        message(
            FATAL_ERROR
            "${dependency} did not provide required target '${target}'"
        )
    endif()
endfunction()

function(bpr_configure_dependencies)
    set(records)

    find_package(Threads REQUIRED)
    bpr_require_dependency_target(Threads::Threads Threads)
    list(APPEND records "Threads|TOOLCHAIN|Threads::Threads")

    if(BUILD_TESTING)
        find_package(
            Python3
            ${BPR_PYTHON_MINIMUM_VERSION}
            REQUIRED
            COMPONENTS Interpreter
        )
        bpr_require_dependency_target(Python3::Interpreter Python3)
        list(APPEND records "Python3|PACKAGE|Python3::Interpreter")
    endif()

    if(BPR_BUILD_QT)
        find_package(
            Qt6
            ${BPR_QT_MINIMUM_VERSION}
            CONFIG
            QUIET
            COMPONENTS Core Gui Widgets Network
        )
        if(NOT Qt6_FOUND)
            message(
                FATAL_ERROR
                "BetterPullReview requires caller-installed Qt >= "
                "${BPR_QT_MINIMUM_VERSION} with Core, Gui, Widgets, and Network. "
                "Install a compatible Qt package and set Qt6_ROOT, Qt6_DIR, or "
                "CMAKE_PREFIX_PATH. Qt is never fetched automatically."
            )
        endif()
        foreach(component IN ITEMS Core Gui Widgets Network)
            bpr_require_dependency_target("Qt6::${component}" "Qt6 ${component}")
            list(APPEND records "Qt6|PACKAGE|Qt6::${component}")
        endforeach()

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(
                BPR_LIBSECRET
                REQUIRED
                IMPORTED_TARGET
                GLOBAL
                "libsecret-1>=${BPR_LIBSECRET_MINIMUM_VERSION}"
            )
            bpr_require_dependency_target(
                PkgConfig::BPR_LIBSECRET
                "libsecret ${BPR_LIBSECRET_MINIMUM_VERSION}"
            )
            if(NOT TARGET LibSecret::LibSecret)
                add_library(
                    LibSecret::LibSecret
                    ALIAS PkgConfig::BPR_LIBSECRET
                )
            endif()
            list(APPEND records "LibSecret|PKG_CONFIG|LibSecret::LibSecret")
        endif()

        if(BUILD_TESTING)
            find_package(Git ${BPR_GIT_MINIMUM_VERSION} REQUIRED)
            bpr_require_dependency_target(Git::Git Git)
            list(APPEND records "Git|PACKAGE|Git::Git")
        endif()
    endif()

    set(audit "set(BPR_PRODUCT_DEPENDENCY_RECORDS\n")
    foreach(record IN LISTS records)
        string(APPEND audit "  [==[${record}]==]\n")
    endforeach()
    string(APPEND audit ")\n")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_bpr")
    set(audit_path "${CMAKE_BINARY_DIR}/_bpr/product-dependency-audit.cmake")
    set(audit_temporary "${audit_path}.tmp")
    file(WRITE "${audit_temporary}" "${audit}")
    configure_file("${audit_temporary}" "${audit_path}" COPYONLY)
    file(REMOVE "${audit_temporary}")
endfunction()
