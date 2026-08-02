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

function(bpr_require_dependency_target_absent target dependency)
    if(TARGET "${target}")
        message(
            FATAL_ERROR
            "${dependency} target '${target}' must not be defined before "
            "BetterPullReview configures its dependency provider"
        )
    endif()
endfunction()

function(bpr_reject_dependency_redirect package)
    if(
        NOT CMAKE_FIND_PACKAGE_REDIRECTS_DIR
        OR NOT IS_DIRECTORY "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}"
    )
        return()
    endif()

    string(TOLOWER "${package}" package_lower)
    file(
        GLOB redirect_entries
        LIST_DIRECTORIES FALSE
        "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/*"
    )
    foreach(entry IN LISTS redirect_entries)
        get_filename_component(name "${entry}" NAME)
        string(TOLOWER "${name}" name_lower)
        if(name_lower MATCHES "^${package_lower}-?config(-?version)?\\.cmake$")
            message(
                FATAL_ERROR
                "Package redirect for provider-owned ${package} is forbidden: "
                "'${entry}'"
            )
        endif()
    endforeach()
endfunction()

function(bpr_configure_dependencies)
    set(records)
    set(CMAKE_MODULE_PATH)

    bpr_reject_dependency_redirect(Threads)
    find_package(Threads MODULE REQUIRED)
    bpr_require_dependency_target(Threads::Threads Threads)
    list(APPEND records "Threads|TOOLCHAIN|Threads::Threads")

    if(BUILD_TESTING)
        bpr_reject_dependency_redirect(Python3)
        bpr_require_dependency_target_absent(
            Python3::Interpreter
            Python3
        )
        find_package(
            Python3
            ${BPR_PYTHON_MINIMUM_VERSION}
            MODULE
            REQUIRED
            COMPONENTS Interpreter
        )
        bpr_require_dependency_target(Python3::Interpreter Python3)
        list(APPEND records "Python3|CMAKE_MODULE|Python3::Interpreter")
    endif()

    if(BPR_BUILD_QT)
        bpr_reject_dependency_redirect(Qt6)
        foreach(component IN ITEMS Core Gui Widgets Network)
            bpr_require_dependency_target_absent(
                "Qt6::${component}"
                "Qt6 ${component}"
            )
        endforeach()
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
            list(APPEND records "Qt6|CMAKE_CONFIG|Qt6::${component}")
        endforeach()

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            bpr_reject_dependency_redirect(PkgConfig)
            bpr_require_dependency_target_absent(
                PkgConfig::BPR_LIBSECRET
                "libsecret ${BPR_LIBSECRET_MINIMUM_VERSION}"
            )
            bpr_require_dependency_target_absent(
                LibSecret::LibSecret
                "libsecret ${BPR_LIBSECRET_MINIMUM_VERSION}"
            )
            find_package(PkgConfig MODULE REQUIRED)
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
            bpr_reject_dependency_redirect(Git)
            bpr_require_dependency_target_absent(Git::Git Git)
            find_package(Git ${BPR_GIT_MINIMUM_VERSION} MODULE REQUIRED)
            bpr_require_dependency_target(Git::Git Git)
            list(APPEND records "Git|CMAKE_MODULE|Git::Git")
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
