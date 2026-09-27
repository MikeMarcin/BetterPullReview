include_guard(GLOBAL)

# Pinned Qt acquisition.
#
# Qt is acquired from upstream's official prebuilt packages. Matching source
# archives are pinned separately in packaging/dependencies.json for recipients.
# The provider mirrors the shape of BprSQLiteProvider — one
# pinned version, a shared cache under CPM_SOURCE_CACHE, a lock around
# acquisition, an offline mode, and validation of what was obtained — while
# retrieving official prebuilt packages rather than source.
#
# The result is a prefix appended to CMAKE_PREFIX_PATH. Discovery itself stays
# an ordinary find_package(Qt6 CONFIG) in BprDependencies.cmake, so nothing
# downstream depends on how Qt arrived.

set(BPR_QT_VERSION "6.11.2")
set(BPR_AQTINSTALL_VERSION "3.3.0")
set(BPR_AQTINSTALL_WINDOWS_REVISION "8c3695d4a4e1ceabf6a74dc6c79681656dc6b74b")

option(
    BPR_QT_PROVIDER
    "Acquire the pinned Qt through the provider instead of requiring a caller-installed package"
    ON
)

set(
    BPR_QT_ROOT
    ""
    CACHE PATH
    "Existing Qt prefix to use instead of acquiring the pinned Qt"
)

# aqtinstall names the host, the architecture, and the installed directory
# differently from one another, so all three are recorded per platform.
function(bpr_qt_platform_identity host_output arch_output directory_output)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(${host_output} "windows" PARENT_SCOPE)
        set(${arch_output} "win64_msvc2022_64" PARENT_SCOPE)
        set(${directory_output} "msvc2022_64" PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(${host_output} "linux" PARENT_SCOPE)
        set(${arch_output} "linux_gcc_64" PARENT_SCOPE)
        set(${directory_output} "gcc_64" PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(${host_output} "mac" PARENT_SCOPE)
        set(${arch_output} "clang_64" PARENT_SCOPE)
        set(${directory_output} "macos" PARENT_SCOPE)
    else()
        message(
            FATAL_ERROR
            "The pinned Qt provider has no package mapping for "
            "${CMAKE_SYSTEM_NAME}. Install Qt >= ${BPR_QT_MINIMUM_VERSION} and "
            "configure with -DBPR_QT_PROVIDER=OFF."
        )
    endif()
endfunction()

# A prefix counts as usable only if the package files this project actually
# consumes are present and report the pinned version. An interrupted download
# leaves a directory that exists but cannot satisfy find_package, and that must
# fail here rather than as a confusing discovery error later.
function(bpr_validate_qt_prefix prefix result_output)
    set(${result_output} FALSE PARENT_SCOPE)
    if(NOT IS_DIRECTORY "${prefix}")
        return()
    endif()

    set(bpr_qt_packages Qt6 Qt6Core Qt6Network Qt6CoreTools)
    if(BPR_BUILD_QT)
        list(APPEND bpr_qt_packages Qt6Gui Qt6Widgets)
    endif()
    foreach(package IN LISTS bpr_qt_packages)
        if(NOT EXISTS "${prefix}/lib/cmake/${package}/${package}Config.cmake")
            return()
        endif()
    endforeach()

    # Qt6ConfigVersion.cmake only forwards to the Impl file, so the literal
    # version has to be read from there; matching against the forwarding file
    # silently never matches.
    set(version_file "${prefix}/lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake")
    if(NOT EXISTS "${version_file}")
        return()
    endif()
    file(
        STRINGS "${version_file}" version_lines
        REGEX "^set\\(PACKAGE_VERSION \"${BPR_QT_VERSION}\"\\)$"
    )
    if(NOT version_lines)
        return()
    endif()

    set(${result_output} TRUE PARENT_SCOPE)
endfunction()

# Released aqtinstall 3.3.0 cannot locate Qt 6.11's split Windows repositories.
# Use the exact upstream merge of https://github.com/miurahr/aqtinstall/pull/1000
# on that host only until a release includes it. This build-only tool is not
# shipped and does not replace or patch Qt's official release binaries.
function(bpr_qt_installer_requirement output)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND BPR_QT_VERSION VERSION_GREATER_EQUAL "6.11")
        set(
            ${output}
            "git+https://github.com/miurahr/aqtinstall.git@${BPR_AQTINSTALL_WINDOWS_REVISION}"
            PARENT_SCOPE
        )
    else()
        set(${output} "aqtinstall==${BPR_AQTINSTALL_VERSION}" PARENT_SCOPE)
    endif()
endfunction()

function(bpr_download_qt host arch prefix_root prefix)
    find_package(Python3 ${BPR_PYTHON_MINIMUM_VERSION} COMPONENTS Interpreter QUIET)
    if(NOT Python3_Interpreter_FOUND)
        message(
            FATAL_ERROR
            "Acquiring the pinned Qt ${BPR_QT_VERSION} requires Python >= "
            "${BPR_PYTHON_MINIMUM_VERSION}. Install it, or supply Qt yourself "
            "with -DBPR_QT_ROOT=<prefix> or -DBPR_QT_PROVIDER=OFF."
        )
    endif()

    # aqtinstall is itself pinned, and lives in a private environment so that
    # acquiring Qt never mutates the interpreter the developer happens to use.
    set(environment "${prefix_root}/.aqt-venv")
    if(NOT EXISTS "${environment}")
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m venv "${environment}"
            RESULT_VARIABLE venv_result
            OUTPUT_VARIABLE venv_output
            ERROR_VARIABLE venv_output
        )
        if(NOT venv_result EQUAL 0)
            file(REMOVE_RECURSE "${environment}")
            message(
                FATAL_ERROR
                "Unable to create the Qt acquisition environment: ${venv_output}"
            )
        endif()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(environment_python "${environment}/Scripts/python.exe")
    else()
        set(environment_python "${environment}/bin/python")
    endif()

    bpr_qt_installer_requirement(installer_requirement)
    execute_process(
        COMMAND
            "${environment_python}" -m pip install --quiet --disable-pip-version-check
            "${installer_requirement}"
        RESULT_VARIABLE pip_result
        OUTPUT_VARIABLE pip_output
        ERROR_VARIABLE pip_output
    )
    if(NOT pip_result EQUAL 0)
        message(
            FATAL_ERROR
            "Unable to install ${installer_requirement}: ${pip_output}"
        )
    endif()

    # aqtinstall reports success while writing an empty prefix when a mirror
    # serves incomplete metadata, and that has been observed intermittently for
    # versions it can otherwise retrieve. Exit status is therefore not evidence:
    # each attempt is judged by revalidating the prefix, and a failed attempt is
    # retried rather than reported, because the next mirror usually works.
    set(attempt_limit 3)
    foreach(attempt RANGE 1 ${attempt_limit})
        message(
            STATUS
            "bpr: acquiring Qt ${BPR_QT_VERSION} (${arch}), attempt "
            "${attempt}/${attempt_limit}"
        )
        execute_process(
            COMMAND
                "${environment_python}" -m aqt install-qt "${host}" desktop
                "${BPR_QT_VERSION}" "${arch}" --outputdir "${prefix_root}"
            # aqtinstall writes aqtinstall.log into its working directory, and
            # the repository must not collect transient tool output.
            WORKING_DIRECTORY "${prefix_root}"
            RESULT_VARIABLE install_result
            OUTPUT_VARIABLE install_output
            ERROR_VARIABLE install_output
        )
        bpr_validate_qt_prefix("${prefix}" attempt_is_valid)
        if(attempt_is_valid)
            return()
        endif()
        # A partially written prefix would otherwise be mistaken for a cache
        # hit by the next configure.
        file(REMOVE_RECURSE "${prefix}")
    endforeach()

    message(
        FATAL_ERROR
        "Unable to acquire Qt ${BPR_QT_VERSION} (${arch}) after "
        "${attempt_limit} attempts. Last status ${install_result}, output:\n"
        "${install_output}"
    )
endfunction()

function(bpr_acquire_qt output)
    # The pin is the version everything downstream actually gets, so it must
    # clear the floor the rest of the build asserts. A future re-pin that drops
    # below it should fail here rather than at an unrelated discovery error.
    if(BPR_QT_VERSION VERSION_LESS BPR_QT_MINIMUM_VERSION)
        message(
            FATAL_ERROR
            "The pinned Qt ${BPR_QT_VERSION} is below the required minimum "
            "${BPR_QT_MINIMUM_VERSION}."
        )
    endif()

    if(BPR_QT_ROOT)
        bpr_validate_qt_prefix("${BPR_QT_ROOT}" supplied_is_valid)
        if(NOT supplied_is_valid)
            message(
                FATAL_ERROR
                "BPR_QT_ROOT='${BPR_QT_ROOT}' is not a Qt ${BPR_QT_VERSION} "
                "prefix providing Core, Gui, Widgets, Network, and CoreTools."
            )
        endif()
        set(${output} "${BPR_QT_ROOT}" PARENT_SCOPE)
        return()
    endif()

    bpr_qt_platform_identity(qt_host qt_arch qt_directory)
    set(prefix_root "${CPM_SOURCE_CACHE}/bprqt/${BPR_QT_VERSION}")
    set(prefix "${prefix_root}/${BPR_QT_VERSION}/${qt_directory}")

    file(MAKE_DIRECTORY "${prefix_root}")
    file(
        LOCK "${CPM_SOURCE_CACHE}/bprqt-${BPR_QT_VERSION}.lock"
        GUARD FUNCTION
        TIMEOUT 1800
        RESULT_VARIABLE lock_result
    )
    if(lock_result)
        message(
            FATAL_ERROR
            "Unable to lock the shared Qt cache: ${lock_result}"
        )
    endif()

    bpr_validate_qt_prefix("${prefix}" cached_is_valid)
    if(NOT cached_is_valid)
        if(BPR_DEPENDENCY_OFFLINE)
            message(
                FATAL_ERROR
                "BPR_DEPENDENCY_OFFLINE=ON requires Qt ${BPR_QT_VERSION} "
                "(${qt_arch}) in CPM_SOURCE_CACHE='${CPM_SOURCE_CACHE}'"
            )
        endif()
        bpr_download_qt("${qt_host}" "${qt_arch}" "${prefix_root}" "${prefix}")
    endif()

    set(${output} "${prefix}" PARENT_SCOPE)
endfunction()

# Qt executables produced outside Qt's own bin directory — the syntax
# highlighting indexer is one — resolve their Qt libraries through the process
# environment. Windows has no rpath equivalent, so a tool run during configure
# exits with STATUS_DLL_NOT_FOUND (0xc0000135) unless Qt's bin directory is
# reachable. Prepending it here keeps that failure out of every consumer.
function(bpr_expose_qt_runtime prefix)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
        return()
    endif()
    file(TO_NATIVE_PATH "${prefix}/bin" qt_binary_directory)
    if(NOT "$ENV{PATH}" MATCHES "${qt_binary_directory}")
        set(ENV{PATH} "${qt_binary_directory};$ENV{PATH}")
    endif()
endfunction()
