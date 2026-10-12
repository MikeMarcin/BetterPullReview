include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# KSyntaxHighlighting is a full KDE Frameworks CMake project. It is acquired as
# verified upstream release source, staged into a controlled build-tree
# directory, and then configured, built, and installed by a separate nested
# CMake invocation into a private prefix. The nested build keeps the framework's
# own CMake settings, install rules, cache entries, and generated targets out of
# the BetterPullReview build graph, and it is the reason the framework may write
# into its own source directory without ever touching the shared CPM cache.
#
# extra-cmake-modules is a transitive build input of that framework. It carries
# its own pin, digest, and provenance record.

set(BPR_ECM_VERSION "6.28.0")
set(BPR_ECM_RELEASE_SERIES "6.28")
set(
    BPR_ECM_ARCHIVE_SHA256
    "a32e24b267e8528d0253bc8df18bdc00e676560a43b796533e1b1406f4eef4db"
)
set(
    BPR_ECM_SOURCE_SHA256
    "ec6fc8bf205d88517b354ee54941cfe83d45eab195f9d8bad8a6539ff4f49d3a"
)

set(BPR_KSYNTAXHIGHLIGHTING_VERSION "6.28.1")
set(BPR_KSYNTAXHIGHLIGHTING_RELEASE_SERIES "6.28")
set(
    BPR_KSYNTAXHIGHLIGHTING_ARCHIVE_SHA256
    "fe0d4133af62c6b9c0cf7728928c64d2deb55fe808a264a5de871f4b6bc86f65"
)
set(
    BPR_KSYNTAXHIGHLIGHTING_SOURCE_SHA256
    "b60bfc55ecf48aeddffa1c2c6cc36afc4ba72c9030483c242af551904459ff85"
)

# KSyntaxHighlighting 6.28.1 requires a newer Qt than the BetterPullReview
# floor, so the syntax provider states its own minimum instead of silently
# failing inside the nested configure.
set(BPR_KSYNTAXHIGHLIGHTING_QT_MINIMUM_VERSION "6.9")

function(bpr_syntax_source_digest source_dir output)
    file(
        GLOB_RECURSE relative_paths
        RELATIVE "${source_dir}"
        LIST_DIRECTORIES FALSE
        "${source_dir}/*"
    )
    list(SORT relative_paths)
    set(manifest "")
    foreach(relative IN LISTS relative_paths)
        if(IS_SYMLINK "${source_dir}/${relative}")
            message(
                FATAL_ERROR
                "Verified syntax-highlighting source must not contain a "
                "symbolic link: '${source_dir}/${relative}'"
            )
        endif()
        file(SHA256 "${source_dir}/${relative}" file_digest)
        string(APPEND manifest "${file_digest}  ${relative}\n")
    endforeach()
    string(SHA256 source_digest "${manifest}")
    set(${output} "${source_digest}" PARENT_SCOPE)
endfunction()

function(bpr_validate_syntax_source label source_dir expected)
    if(NOT IS_DIRECTORY "${source_dir}")
        message(FATAL_ERROR "Verified ${label} source is missing: '${source_dir}'")
    endif()
    if(NOT EXISTS "${source_dir}/CMakeLists.txt")
        message(
            FATAL_ERROR
            "Verified ${label} source is incomplete: "
            "'${source_dir}/CMakeLists.txt'"
        )
    endif()
    bpr_syntax_source_digest("${source_dir}" actual)
    if(NOT actual STREQUAL expected)
        message(
            FATAL_ERROR
            "${label} source tree does not match the pinned release.\n"
            "  directory: ${source_dir}\n"
            "  expected:  ${expected}\n"
            "  actual:    ${actual}"
        )
    endif()
endfunction()

function(bpr_find_cached_syntax_source cache_name expected output)
    file(
        GLOB candidate_manifests
        LIST_DIRECTORIES FALSE
        "${CPM_SOURCE_CACHE}/${cache_name}/*/CMakeLists.txt"
    )
    foreach(manifest IN LISTS candidate_manifests)
        get_filename_component(candidate "${manifest}" DIRECTORY)
        bpr_syntax_source_digest("${candidate}" actual)
        if(actual STREQUAL expected)
            set(${output} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${output} "" PARENT_SCOPE)
endfunction()

function(bpr_stage_syntax_source label source_dir expected stage)
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(COPY "${source_dir}/" DESTINATION "${stage}")
    bpr_validate_syntax_source("${label}" "${stage}" "${expected}")
endfunction()

function(bpr_syntax_forward arguments_variable name)
    if(NOT DEFINED ${name})
        return()
    endif()
    set(bpr_syntax_forward_value "${${name}}")
    if(bpr_syntax_forward_value STREQUAL "")
        return()
    endif()
    # The temporary below must not be able to share a name with the caller's
    # accumulator. It used to be called `forwarded`, and every caller passes an
    # accumulator called `forwarded`, so the local shadowed it: by the time
    # `${${arguments_variable}}` was read it held this one escaped value rather
    # than the list built so far. Each call therefore threw away everything
    # accumulated before it, and only the last name survived -- as a bare value
    # plus its own `-D`. The compiler, flags, build type, toolchain file, sysroot
    # and prefix path were all silently dropped from the nested configure, which
    # then found whatever Qt the host happened to have.
    string(
        REPLACE ";" "\\;"
        bpr_syntax_forward_value
        "${bpr_syntax_forward_value}"
    )
    # Append without re-expanding what is already there. Expanding the
    # accumulator unquoted turns every `\;` a previous call escaped back into
    # a bare `;`, so a list value such as CMAKE_CONFIGURATION_TYPES survived
    # its own round and was split on the next one: the nested configure then
    # received `-DCMAKE_CONFIGURATION_TYPES=Debug` followed by `Release`,
    # `MinSizeRel`, and `RelWithDebInfo` as stray arguments, and the
    # multi-configuration build that followed asked for a Release it had never
    # generated. `list(APPEND)` and the quoted PARENT_SCOPE copy keep each
    # escaped element intact until the command line is formed.
    list(APPEND ${arguments_variable} "-D${name}=${bpr_syntax_forward_value}")
    set(${arguments_variable} "${${arguments_variable}}" PARENT_SCOPE)
endfunction()

function(bpr_syntax_execute label log)
    # The command arrives with forwarded `-D<name>=<list>` arguments whose
    # semicolons are escaped. `ARGN` would re-join every argument with a bare
    # `;` and hand execute_process four arguments where the caller passed one;
    # PARSE_ARGV keeps each argument exactly as it was given.
    cmake_parse_arguments(PARSE_ARGV 2 bpr_syntax_execute "" "" "")
    execute_process(
        COMMAND ${bpr_syntax_execute_UNPARSED_ARGUMENTS}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    file(WRITE "${log}" "${output}")
    if(NOT result EQUAL 0)
        message(
            FATAL_ERROR
            "Verified KSyntaxHighlighting ${label} failed (${result}).\n"
            "  log: ${log}\n"
            "${output}"
        )
    endif()
endfunction()

function(bpr_acquire_syntax_highlighting)
    if(NOT COMMAND bpr_reject_dependency_redirect)
        message(
            FATAL_ERROR
            "BprSyntaxHighlightingProvider requires cmake/BprDependencies.cmake"
        )
    endif()
    if(NOT TARGET Qt6::Core OR NOT TARGET Qt6::Gui OR NOT TARGET Qt6::Network)
        message(
            FATAL_ERROR
            "KSyntaxHighlighting requires the caller-installed Qt Core, Gui, "
            "and Network targets to be resolved first"
        )
    endif()
    if(Qt6_VERSION VERSION_LESS BPR_KSYNTAXHIGHLIGHTING_QT_MINIMUM_VERSION)
        message(
            FATAL_ERROR
            "KSyntaxHighlighting ${BPR_KSYNTAXHIGHLIGHTING_VERSION} requires "
            "caller-installed Qt >= ${BPR_KSYNTAXHIGHLIGHTING_QT_MINIMUM_VERSION}, "
            "but Qt ${Qt6_VERSION} was found"
        )
    endif()
    bpr_reject_dependency_redirect(KF6SyntaxHighlighting)
    if(TARGET KF6::SyntaxHighlighting)
        message(
            FATAL_ERROR
            "KSyntaxHighlighting target 'KF6::SyntaxHighlighting' must not be "
            "defined before BetterPullReview configures its dependency provider"
        )
    endif()
    foreach(
        override
        IN ITEMS
            FETCHCONTENT_SOURCE_DIR_BPRKSYNTAXHIGHLIGHTING
            FETCHCONTENT_SOURCE_DIR_BPREXTRACMAKEMODULES
    )
        if(DEFINED ${override})
            set(override_value "${${override}}")
            if(NOT override_value STREQUAL "")
                message(
                    FATAL_ERROR
                    "${override} is forbidden; KSyntaxHighlighting must use "
                    "the verified CPM provider"
                )
            endif()
        endif()
    endforeach()

    set(
        FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER
        CACHE STRING
        "KSyntaxHighlighting must not resolve through find_package"
        FORCE
    )
    set(CMAKE_TLS_VERIFY ON)
    bpr_load_cpm()
    set(
        syntax_lock
        "${CPM_SOURCE_CACHE}/bprksyntaxhighlighting-${BPR_KSYNTAXHIGHLIGHTING_VERSION}.lock"
    )
    file(
        LOCK "${syntax_lock}"
        GUARD FUNCTION
        TIMEOUT 600
        RESULT_VARIABLE lock_result
    )
    if(lock_result)
        message(
            FATAL_ERROR
            "Unable to lock the shared KSyntaxHighlighting CPM cache: ${lock_result}"
        )
    endif()

    set(CPM_BprExtraCmakeModules_SOURCE "")
    set(CPM_DECLARATION_BprExtraCmakeModules "")
    set(CPM_BprKSyntaxHighlighting_SOURCE "")
    set(CPM_DECLARATION_BprKSyntaxHighlighting "")
    if(BPR_DEPENDENCY_OFFLINE)
        bpr_find_cached_syntax_source(
            bprextracmakemodules
            "${BPR_ECM_SOURCE_SHA256}"
            cached_ecm
        )
        if(NOT cached_ecm)
            message(
                FATAL_ERROR
                "BPR_DEPENDENCY_OFFLINE=ON requires extra-cmake-modules "
                "${BPR_ECM_VERSION} in CPM_SOURCE_CACHE='${CPM_SOURCE_CACHE}'"
            )
        endif()
        bpr_find_cached_syntax_source(
            bprksyntaxhighlighting
            "${BPR_KSYNTAXHIGHLIGHTING_SOURCE_SHA256}"
            cached_syntax
        )
        if(NOT cached_syntax)
            message(
                FATAL_ERROR
                "BPR_DEPENDENCY_OFFLINE=ON requires KSyntaxHighlighting "
                "${BPR_KSYNTAXHIGHLIGHTING_VERSION} in "
                "CPM_SOURCE_CACHE='${CPM_SOURCE_CACHE}'"
            )
        endif()
        CPMAddPackage(
            NAME BprExtraCmakeModules
            VERSION "${BPR_ECM_VERSION}"
            SOURCE_DIR "${cached_ecm}"
            DOWNLOAD_ONLY YES
            FORCE YES
        )
        CPMAddPackage(
            NAME BprKSyntaxHighlighting
            VERSION "${BPR_KSYNTAXHIGHLIGHTING_VERSION}"
            SOURCE_DIR "${cached_syntax}"
            DOWNLOAD_ONLY YES
            FORCE YES
        )
    else()
        CPMAddPackage(
            NAME BprExtraCmakeModules
            VERSION "${BPR_ECM_VERSION}"
            URL "https://download.kde.org/stable/frameworks/${BPR_ECM_RELEASE_SERIES}/extra-cmake-modules-${BPR_ECM_VERSION}.tar.xz"
            URL_HASH "SHA256=${BPR_ECM_ARCHIVE_SHA256}"
            DOWNLOAD_ONLY YES
            FORCE YES
        )
        CPMAddPackage(
            NAME BprKSyntaxHighlighting
            VERSION "${BPR_KSYNTAXHIGHLIGHTING_VERSION}"
            URL "https://download.kde.org/stable/frameworks/${BPR_KSYNTAXHIGHLIGHTING_RELEASE_SERIES}/syntax-highlighting-${BPR_KSYNTAXHIGHLIGHTING_VERSION}.tar.xz"
            URL_HASH "SHA256=${BPR_KSYNTAXHIGHLIGHTING_ARCHIVE_SHA256}"
            DOWNLOAD_ONLY YES
            FORCE YES
        )
    endif()
    foreach(
        provided
        IN ITEMS
            BprExtraCmakeModules_SOURCE_DIR
            BprKSyntaxHighlighting_SOURCE_DIR
    )
        if(NOT ${provided} OR NOT EXISTS "${${provided}}")
            message(
                FATAL_ERROR
                "CPM did not provide KSyntaxHighlighting "
                "${BPR_KSYNTAXHIGHLIGHTING_VERSION} and its build inputs"
            )
        endif()
    endforeach()
    bpr_validate_syntax_source(
        "extra-cmake-modules ${BPR_ECM_VERSION}"
        "${BprExtraCmakeModules_SOURCE_DIR}"
        "${BPR_ECM_SOURCE_SHA256}"
    )
    bpr_validate_syntax_source(
        "KSyntaxHighlighting ${BPR_KSYNTAXHIGHLIGHTING_VERSION}"
        "${BprKSyntaxHighlighting_SOURCE_DIR}"
        "${BPR_KSYNTAXHIGHLIGHTING_SOURCE_SHA256}"
    )

    set(
        root
        "${CMAKE_BINARY_DIR}/_bpr/ksyntaxhighlighting/${BPR_KSYNTAXHIGHLIGHTING_VERSION}"
    )
    set(prefix "${root}/prefix")
    set(ecm_prefix "${root}/extra-cmake-modules-prefix")
    set(ecm_module_directory "${ecm_prefix}/share/ECM/cmake")
    set(stamp "${root}/verified-build.stamp")

    set(forwarded)
    foreach(
        name
        IN ITEMS
            CMAKE_MAKE_PROGRAM
            CMAKE_TOOLCHAIN_FILE
            CMAKE_C_COMPILER
            CMAKE_CXX_COMPILER
            CMAKE_C_FLAGS
            CMAKE_CXX_FLAGS
            CMAKE_EXE_LINKER_FLAGS
            CMAKE_SHARED_LINKER_FLAGS
            CMAKE_STATIC_LINKER_FLAGS
            CMAKE_BUILD_TYPE
            CMAKE_CONFIGURATION_TYPES
            CMAKE_MSVC_RUNTIME_LIBRARY
            CMAKE_OSX_ARCHITECTURES
            CMAKE_OSX_DEPLOYMENT_TARGET
            CMAKE_OSX_SYSROOT
            CMAKE_PREFIX_PATH
            Qt6_DIR
    )
        bpr_syntax_forward(forwarded "${name}")
    endforeach()

    # A multi-configuration generator builds and installs the framework once
    # per configuration into this one prefix, and the framework names its
    # library identically in every one of them, so without a postfix the last
    # install overwrote the rest and every configuration linked whichever
    # runtime that happened to be: a Debug application against a
    # release-runtime library, which the linker refuses (LNK2038). Release
    # keeps the bare name; every other configuration gets its own, and the
    # exported IMPORTED_LOCATION_<CONFIG> then names the library that
    # configuration actually built. These join the forwarded list so the
    # stamp signature below changes with them.
    get_property(bpr_syntax_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(bpr_syntax_multi_config)
        foreach(configuration IN LISTS CMAKE_CONFIGURATION_TYPES)
            if(configuration STREQUAL "Release")
                continue()
            endif()
            string(TOUPPER "${configuration}" configuration_upper)
            string(TOLOWER "${configuration}" configuration_lower)
            list(
                APPEND
                forwarded
                "-DCMAKE_${configuration_upper}_POSTFIX=_${configuration_lower}"
            )
        endforeach()
    endif()

    # Earlier ECM stages used the machine default generator. Reconfigure
    # those stages once so their cached build graph agrees with the parent.
    set(signature "ECM_USES_PARENT_GENERATOR=ON\n")
    foreach(
        recorded
        IN ITEMS
            BPR_KSYNTAXHIGHLIGHTING_VERSION
            BPR_KSYNTAXHIGHLIGHTING_SOURCE_SHA256
            BPR_ECM_VERSION
            BPR_ECM_SOURCE_SHA256
            Qt6_VERSION
            CMAKE_VERSION
            CMAKE_GENERATOR
            CMAKE_GENERATOR_PLATFORM
            CMAKE_GENERATOR_TOOLSET
            CMAKE_GENERATOR_INSTANCE
    )
        string(APPEND signature "${recorded}=${${recorded}}\n")
    endforeach()
    foreach(argument IN LISTS forwarded)
        string(APPEND signature "${argument}\n")
    endforeach()

    # [[BPR-0974]]. A graph that keeps its Qt resources outside the image
    # (cmake/BprQtResources.cmake) builds the framework without its syntax
    # resource, and the application bundles the same definitions from the
    # framework's own collection instead. Every other graph embeds them as
    # before, and its signature, and so its staged build, is unchanged.
    set(qrc_syntax ON)
    if(BPR_EXTERNAL_QT_RESOURCES)
        set(qrc_syntax OFF)
        string(APPEND signature "QRC_SYNTAX=OFF\n")
    endif()

    set(staged FALSE)
    if(EXISTS "${stamp}")
        file(READ "${stamp}" recorded_signature)
        if(recorded_signature STREQUAL signature)
            set(staged TRUE)
        endif()
    endif()

    if(NOT staged)
        message(
            STATUS
            "Building verified KSyntaxHighlighting "
            "${BPR_KSYNTAXHIGHLIGHTING_VERSION} with extra-cmake-modules "
            "${BPR_ECM_VERSION}"
        )
        file(REMOVE "${stamp}")
        file(REMOVE_RECURSE "${prefix}" "${ecm_prefix}" "${root}/logs")
        file(MAKE_DIRECTORY "${root}/logs")

        set(ecm_source "${root}/extra-cmake-modules-source")
        bpr_stage_syntax_source(
            "extra-cmake-modules ${BPR_ECM_VERSION}"
            "${BprExtraCmakeModules_SOURCE_DIR}"
            "${BPR_ECM_SOURCE_SHA256}"
            "${ecm_source}"
        )
        set(syntax_source "${root}/syntax-highlighting-source")
        bpr_stage_syntax_source(
            "KSyntaxHighlighting ${BPR_KSYNTAXHIGHLIGHTING_VERSION}"
            "${BprKSyntaxHighlighting_SOURCE_DIR}"
            "${BPR_KSYNTAXHIGHLIGHTING_SOURCE_SHA256}"
            "${syntax_source}"
        )

        # Both nested projects use the parent generator. Otherwise ECM
        # defaults to Visual Studio on Windows even when the release graph
        # uses Ninja, and FileTracker rejects the nested try-compile paths.
        set(generator_arguments -G "${CMAKE_GENERATOR}")
        if(CMAKE_GENERATOR_PLATFORM)
            list(APPEND generator_arguments -A "${CMAKE_GENERATOR_PLATFORM}")
        endif()
        if(CMAKE_GENERATOR_TOOLSET)
            list(APPEND generator_arguments -T "${CMAKE_GENERATOR_TOOLSET}")
        endif()
        set(ecm_generator_arguments ${generator_arguments})
        bpr_syntax_forward(ecm_generator_arguments CMAKE_MAKE_PROGRAM)
        set(ecm_build "${root}/extra-cmake-modules-build")
        file(REMOVE_RECURSE "${ecm_build}")
        bpr_syntax_execute(
            "extra-cmake-modules configure"
            "${root}/logs/extra-cmake-modules-configure.log"
            "${CMAKE_COMMAND}"
            -S "${ecm_source}"
            -B "${ecm_build}"
            ${ecm_generator_arguments}
            "-DCMAKE_INSTALL_PREFIX=${ecm_prefix}"
            -DBUILD_TESTING=OFF
            -DBUILD_HTML_DOCS=OFF
            -DBUILD_MAN_DOCS=OFF
            -DBUILD_QTHELP_DOCS=OFF
        )
        bpr_syntax_execute(
            "extra-cmake-modules install"
            "${root}/logs/extra-cmake-modules-install.log"
            "${CMAKE_COMMAND}" --install "${ecm_build}"
        )
        if(NOT EXISTS "${ecm_module_directory}/ECMConfig.cmake")
            message(
                FATAL_ERROR
                "extra-cmake-modules ${BPR_ECM_VERSION} did not install "
                "'${ecm_module_directory}/ECMConfig.cmake'"
            )
        endif()

        set(syntax_build "${root}/syntax-highlighting-build")
        file(REMOVE_RECURSE "${syntax_build}")
        bpr_syntax_execute(
            "configure"
            "${root}/logs/syntax-highlighting-configure.log"
            "${CMAKE_COMMAND}"
            -S "${syntax_source}"
            -B "${syntax_build}"
            ${generator_arguments}
            ${forwarded}
            "-DCMAKE_INSTALL_PREFIX=${prefix}"
            "-DECM_DIR=${ecm_module_directory}"
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_TESTING=OFF
            -DDISABLE_ALL_OPTIONAL_SUBDIRECTORIES=TRUE
            -DKF_SKIP_PO_PROCESSING=ON
            -DKSYNTAXHIGHLIGHTING_USE_GUI=ON
            "-DQRC_SYNTAX=${qrc_syntax}"
            -DNO_STANDARD_PATHS=ON
        )

        get_property(
            multi_config
            GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG
        )
        set(configurations "")
        if(multi_config)
            # One dependency build and install per configuration the tree
            # can select. That list is whatever CMAKE_CONFIGURATION_TYPES
            # holds, so the preset that selects a multi-config generator must
            # pin it to the configurations the project builds: the Visual
            # Studio default adds MinSizeRel and RelWithDebInfo, nothing here
            # selects either, and MinSizeRel does not compile (BPR-0495).
            # Asking the generator for a configuration outside this list
            # fails at build time rather than linking the wrong install.
            set(configurations ${CMAKE_CONFIGURATION_TYPES})
        endif()
        if(NOT configurations)
            # Single-configuration generators build once and take the
            # configuration from the forwarded CMAKE_BUILD_TYPE.
            set(configurations "SINGLE_CONFIGURATION")
        endif()
        foreach(configuration IN LISTS configurations)
            set(configuration_arguments "")
            set(configuration_suffix "")
            if(NOT configuration STREQUAL "SINGLE_CONFIGURATION")
                set(configuration_arguments --config "${configuration}")
                set(configuration_suffix "-${configuration}")
            endif()
            bpr_syntax_execute(
                "build${configuration_suffix}"
                "${root}/logs/syntax-highlighting-build${configuration_suffix}.log"
                "${CMAKE_COMMAND}" --build "${syntax_build}"
                ${configuration_arguments} --parallel
            )
            bpr_syntax_execute(
                "install${configuration_suffix}"
                "${root}/logs/syntax-highlighting-install${configuration_suffix}.log"
                "${CMAKE_COMMAND}" --install "${syntax_build}"
                ${configuration_arguments}
            )
        endforeach()

        file(WRITE "${stamp}" "${signature}")
    endif()

    find_package(
        KF6SyntaxHighlighting
        "${BPR_KSYNTAXHIGHLIGHTING_VERSION}"
        REQUIRED
        CONFIG
        EXACT
        PATHS "${prefix}"
        NO_DEFAULT_PATH
    )
    if(NOT TARGET KF6::SyntaxHighlighting)
        message(
            FATAL_ERROR
            "KSyntaxHighlighting did not provide required target "
            "'KF6::SyntaxHighlighting'"
        )
    endif()

    # [[BPR-0974]]. The framework's own syntax collection -- the index and the
    # compressed definitions its build generates in both modes -- for the
    # external Qt resource bundle (cmake/BprQtResources.cmake).
    set(syntax_data "${root}/syntax-highlighting-build/data")
    foreach(generated IN ITEMS syntax-data.qrc index.katesyntax)
        if(BPR_QT_RESOURCE_BUNDLE AND NOT EXISTS "${syntax_data}/${generated}")
            message(
                FATAL_ERROR
                "KSyntaxHighlighting did not generate '${syntax_data}/${generated}'"
            )
        endif()
    endforeach()
    set_property(
        GLOBAL
        PROPERTY BPR_KSYNTAXHIGHLIGHTING_SYNTAX_COLLECTION
            "${syntax_data}/syntax-data.qrc"
    )
    set_property(
        GLOBAL
        PROPERTY BPR_KSYNTAXHIGHLIGHTING_SYNTAX_INDEX
            "${syntax_data}/index.katesyntax"
    )
endfunction()
