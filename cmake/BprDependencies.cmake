include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/BprQtProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprQuillProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprGlazeProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprRe2Provider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprZlibProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprUtf8procProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprFmtProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprSha1dcProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprHaclSha2Provider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprRapidFuzzProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprDtlProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprXdiffProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprSyntaxHighlightingProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprReleaseArchiveProvider.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BprCodeFontProvider.cmake")

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

macro(bpr_include_builtin_find_module package)
    set(
        _bpr_builtin_find_module
        "${CMAKE_ROOT}/Modules/Find${package}.cmake"
    )
    if(NOT EXISTS "${_bpr_builtin_find_module}")
        message(FATAL_ERROR "CMake built-in Find${package}.cmake is missing")
    endif()
    include("${_bpr_builtin_find_module}")
    unset(_bpr_builtin_find_module)
endmacro()

function(bpr_configure_dependencies)
    set(records)

    if(BPR_BUILD_QT AND APPLE)
        bpr_configure_release_archive()
        list(APPEND records "LibArchive|CPM_PINNED|BetterPullReview::ReleaseArchive")
    endif()

    bpr_reject_dependency_redirect(Threads)
    set(Threads_FIND_REQUIRED TRUE)
    bpr_include_builtin_find_module(Threads)
    bpr_require_dependency_target(Threads::Threads Threads)
    list(APPEND records "Threads|TOOLCHAIN|Threads::Threads")

    # [[BPR-1618]]. Owner-approved deterministic source matching in core.
    # RE2's internal synchronization creates no worker threads.
    bpr_acquire_re2()
    bpr_require_dependency_target(BetterPullReview::RE2 RE2)
    list(APPEND records "RE2|PINNED_ARCHIVE|BetterPullReview::RE2")

    # [[BPR-1435]]. The core's review-bundle reader inflates with pinned zlib.
    bpr_acquire_zlib()
    bpr_require_dependency_target(BetterPullReview::ZLib zlib)
    list(APPEND records "zlib|PINNED_ARCHIVE|BetterPullReview::ZLib")

    # [[BPR-1434]]. The core's Unicode decoding, decomposition and case mapping.
    bpr_acquire_utf8proc()
    bpr_require_dependency_target(BetterPullReview::UTF8Proc utf8proc)
    list(APPEND records "utf8proc|PINNED_ARCHIVE|BetterPullReview::UTF8Proc")

    # [[BPR-1439]]. fmt formats text, never `std::format` (owner, 2026-10-13).
    bpr_acquire_fmt()
    bpr_require_dependency_target(BetterPullReview::Fmt fmt)
    list(APPEND records "fmt|PINNED_ARCHIVE|BetterPullReview::Fmt")

    # [[BPR-1450]]. Every SHA-1 Git identity goes through sha1collisiondetection.
    bpr_acquire_sha1dc()
    bpr_require_dependency_target(BetterPullReview::Sha1dc sha1collisiondetection)
    list(APPEND records "sha1collisiondetection|PINNED_ARCHIVE|BetterPullReview::Sha1dc")

    # [[BPR-1451]]. The core's SHA-256 is HACL*'s generated SHA-2.
    bpr_acquire_hacl_sha2()
    bpr_require_dependency_target(BetterPullReview::HaclSha2 hacl-star)
    list(APPEND records "hacl-star|PINNED_ARCHIVE|BetterPullReview::HaclSha2")

    # [[BPR-1480]]. The diff engine scores line pairs with rapidfuzz-cpp's
    # Indel distance.
    bpr_acquire_rapidfuzz()
    bpr_require_dependency_target(BetterPullReview::RapidFuzz rapidfuzz-cpp)
    list(APPEND records "rapidfuzz-cpp|PINNED_ARCHIVE|BetterPullReview::RapidFuzz")

    # [[BPR-1481]] [[BPR-1498]]. The diff engine's intraline ranges are dtl's
    # common scalars of one line pair, and the Review Cycle suggestion hunks
    # its bounded line edit.
    bpr_acquire_dtl()
    bpr_require_dependency_target(BetterPullReview::Dtl dtl)
    list(APPEND records "dtl|PINNED_ARCHIVE|BetterPullReview::Dtl")

    # [[BPR-1485]]. The diff engine's primary line matcher: Git 2.56.0's xdiff
    # as its own shared library, which only core links through its private matcher.
    bpr_acquire_xdiff()
    bpr_require_dependency_target(BetterPullReview::Xdiff git-xdiff)
    list(APPEND records "git-xdiff|PINNED_ARCHIVE|BetterPullReview::Xdiff")

    # [[BPR-1433]]. glaze is an owner-approved core library (2026-10-13) and
    # the MCP helper's JSON decoder, so the Qt-free graph acquires it too.
    bpr_acquire_glaze()
    bpr_require_dependency_target(glaze::glaze Glaze)
    list(APPEND records "Glaze|PINNED_ARCHIVE|glaze::glaze")

    if(
        BUILD_TESTING
        OR BPR_ENABLE_CODING_TARGETS
        OR BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE
    )
        bpr_reject_dependency_redirect(Python3)
        bpr_require_dependency_target_absent(
            Python3::Interpreter
            Python3
        )
        set(Python3_FIND_COMPONENTS Interpreter)
        set(Python3_FIND_REQUIRED TRUE)
        set(Python3_FIND_REQUIRED_Interpreter TRUE)
        bpr_include_builtin_find_module(Python3)
        if(Python3_VERSION VERSION_LESS BPR_PYTHON_MINIMUM_VERSION)
            message(
                FATAL_ERROR
                "Python3 ${Python3_VERSION} is below required version "
                "${BPR_PYTHON_MINIMUM_VERSION}"
            )
        endif()
        bpr_require_dependency_target(Python3::Interpreter Python3)
        list(APPEND records "Python3|CMAKE_MODULE|Python3::Interpreter")
    endif()

    if(BPR_BUILD_QT OR BPR_BUILD_TOUR_CLI)
        set(bpr_qt_components Core Network)
        if(BPR_BUILD_QT)
            # RFC-BPR-0084 Q1: Svg rasterizes SVG body images.
            list(APPEND bpr_qt_components Gui Widgets Svg)
        endif()
        bpr_reject_dependency_redirect(Qt6)
        foreach(component IN LISTS bpr_qt_components)
            bpr_require_dependency_target_absent(
                "Qt6::${component}"
                "Qt6 ${component}"
            )
        endforeach()
        bpr_require_dependency_target_absent(Qt6::rcc "Qt6 CoreTools")
        set(qt_version_arguments "${BPR_QT_MINIMUM_VERSION}")
        if(BPR_QT_PROVIDER)
            bpr_acquire_qt(bpr_qt_prefix)
            bpr_expose_qt_runtime("${bpr_qt_prefix}")
            list(PREPEND CMAKE_PREFIX_PATH "${bpr_qt_prefix}")
            # A cached Qt6_DIR from the previous pin outranks prefix search.
            # Never mix its CMake helpers with the newly acquired modules.
            set(Qt6_DIR "${bpr_qt_prefix}/lib/cmake/Qt6" CACHE PATH "Pinned Qt package" FORCE)
            set(qt_version_arguments "${BPR_QT_VERSION}" EXACT)
        endif()
        find_package(
            Qt6
            ${qt_version_arguments}
            CONFIG
            QUIET
            COMPONENTS ${bpr_qt_components}
        )
        if(NOT Qt6_FOUND)
            if(BPR_QT_PROVIDER)
                message(
                    FATAL_ERROR
                    "The pinned Qt ${BPR_QT_VERSION} provider produced "
                    "'${bpr_qt_prefix}', but Qt6 components ${bpr_qt_components} did not resolve from it."
                )
            endif()
            message(
                FATAL_ERROR
                "BPR_QT_PROVIDER=OFF requires caller-installed Qt >= "
                "${BPR_QT_MINIMUM_VERSION} with ${bpr_qt_components}. "
                "Install a compatible Qt package and set Qt6_ROOT, Qt6_DIR, or "
                "CMAKE_PREFIX_PATH, or re-enable the pinned provider."
            )
        endif()
        foreach(component IN LISTS bpr_qt_components)
            bpr_require_dependency_target("Qt6::${component}" "Qt6 ${component}")
            list(APPEND records "Qt6|CMAKE_CONFIG|Qt6::${component}")
        endforeach()

        bpr_require_dependency_target(Qt6::rcc "Qt6 CoreTools")
        list(APPEND records "Qt6CoreTools|CMAKE_CONFIG|Qt6::rcc")
        # Qt's resource macro runs outside this function. Preserve the package
        # namespace it uses to address the already-proven Qt6::rcc target.
        set(QT_CMAKE_EXPORT_NAMESPACE Qt6 PARENT_SCOPE)
        set(BPR_PRODUCT_QT_VERSION "${Qt6_VERSION}" PARENT_SCOPE)

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
            set(PkgConfig_FIND_REQUIRED TRUE)
            bpr_include_builtin_find_module(PkgConfig)
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

            # [[BPR-1024]]. The composition's X11 activation host names one
            # atom on the connection Qt's xcb platform opened. libxcb is
            # already in the runtime closure of Qt's xcb platform plugin.
            if(BPR_BUILD_QT)
                bpr_require_dependency_target_absent(PkgConfig::BPR_XCB xcb)
                pkg_check_modules(
                    BPR_XCB
                    REQUIRED
                    IMPORTED_TARGET
                    GLOBAL
                    xcb
                )
                bpr_require_dependency_target(PkgConfig::BPR_XCB xcb)
                list(APPEND records "XCB|PKG_CONFIG|PkgConfig::BPR_XCB")
            endif()
        endif()

        if(BUILD_TESTING)
            bpr_reject_dependency_redirect(Git)
            bpr_require_dependency_target_absent(Git::Git Git)
            if(NOT GIT_EXECUTABLE)
                set(_bpr_git_roots)
                if(Git_ROOT)
                    list(APPEND _bpr_git_roots "${Git_ROOT}")
                endif()
                if(DEFINED ENV{Git_ROOT} AND NOT "$ENV{Git_ROOT}" STREQUAL "")
                    list(APPEND _bpr_git_roots "$ENV{Git_ROOT}")
                endif()
                if(_bpr_git_roots)
                    find_program(
                        _bpr_git_from_root
                        NAMES git git.cmd
                        PATHS ${_bpr_git_roots}
                        PATH_SUFFIXES bin cmd
                        NO_DEFAULT_PATH
                        NO_CACHE
                    )
                    if(_bpr_git_from_root)
                        set(GIT_EXECUTABLE "${_bpr_git_from_root}")
                    endif()
                endif()
            endif()
            set(Git_FIND_REQUIRED TRUE)
            bpr_include_builtin_find_module(Git)
            if(Git_VERSION VERSION_LESS BPR_GIT_MINIMUM_VERSION)
                message(
                    FATAL_ERROR
                    "Git ${Git_VERSION} is below required version "
                    "${BPR_GIT_MINIMUM_VERSION}"
                )
            endif()
            bpr_require_dependency_target(Git::Git Git)
            list(APPEND records "Git|CMAKE_MODULE|Git::Git")
        endif()


        if(BPR_BUILD_QT)
            bpr_acquire_quill()
            bpr_require_dependency_target(quill::quill Quill)
            list(APPEND records "Quill|PINNED_ARCHIVE|quill::quill")

            bpr_acquire_syntax_highlighting()

            # [[BPR-1270]]. The bundled default code font's four faces.
            bpr_acquire_code_font()
            bpr_require_dependency_target(BetterPullReview::CodeFont "JetBrains Mono NL")
            list(APPEND records "JetBrainsMonoNL|PINNED_ARCHIVE|BetterPullReview::CodeFont")
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
